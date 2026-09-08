#include <WiFi.h>
#include "screens.h"
#include "display.h"
#include "gauge.h"
#include "particles.h"
#include "thinclock.h"
#include <time.h>
#include <algorithm>

extern Display      display;
extern ConfigManager configMgr;

// -----------------------------------------------------------------------
// State helpers
// -----------------------------------------------------------------------

void resetState(ScreenState& state) {
    state.inited = false;
    state.textStates.clear();
    state.iconStates.clear();
    state.particleSystems.clear();
}

void initScreenState(ScreenState& state, const Screen& scr) {
    state.textStates.clear();
    state.iconStates.clear();
    state.particleSystems.clear();
    state.lastTick  = millis();
    state.startTime = millis();
    state.inited    = true;
    for (const auto& layer : scr.layers) {
        if (layer.type == LAYER_TEXT || layer.type == LAYER_CLOCK) state.textStates.push_back(TextState());
        if (layer.type == LAYER_ICON)      state.iconStates.push_back(IconState());
        if (layer.type == LAYER_PARTICLES) { ParticleSystem ps; ps.init(layer.particles); state.particleSystems.push_back(ps); }
    }
}

bool screenHasScrolling(const ScreenState& state) {
    return std::any_of(state.textStates.begin(), state.textStates.end(),
        [](const TextState& ts) { return (ts.mode == SCROLL_LEFT || ts.mode == SCROLL_BOUNCE) && !ts.completedOnce; });
}

void switchScreen(AppState& state) {
    state.prevState      = state.currentState;
    state.prevScreenIdx  = state.currentScreen;
    state.prevScreenData = state.screenData;
    state.currentScreen  = (state.currentScreen + 1) % state.config.screens.size();
    state.lastScreenSwitch = millis();
    state.screenData.clear();
    state.lastDataFetch = 0;
    resetState(state.currentState);
    state.transitioning      = true;
    state.transitionProgress = 0;
}

void resetCurrentScreen(AppState& state, bool /*prev*/) {
    state.transitioning  = false;
    state.prevScreenIdx  = -1;
    state.currentScreen  = (state.currentScreen - 1 + state.config.screens.size()) % state.config.screens.size();
    state.lastScreenSwitch = millis();
    state.screenData.clear();
    state.lastDataFetch = 0;
    resetState(state.currentState);
}

// -----------------------------------------------------------------------
// Tween engine
// -----------------------------------------------------------------------

static float tweenEase(float t, const String& easing) {
    if (easing == "sine")        return 0.5f - 0.5f * cos(t * 3.14159f);
    if (easing == "ease_in")     return t * t;
    if (easing == "ease_out")    return 1.0f - (1.0f - t) * (1.0f - t);
    if (easing == "ease_in_out") return t < 0.5f ? 2*t*t : 1 - 2*(1-t)*(1-t);
    return t;
}

static float evaluateTween(const Layer::Tween& tw, uint32_t elapsed) {
    if (elapsed < tw.delay) return tw.from;
    uint32_t active = elapsed - tw.delay;
    float progress;
    if (tw.loop == "repeat")        progress = (float)(active % tw.duration) / tw.duration;
    else if (tw.loop == "pingpong") { uint32_t c = active % (tw.duration * 2); progress = (float)c / tw.duration; if (progress > 1.0f) progress = 2.0f - progress; }
    else                            { progress = (float)active / tw.duration; if (progress > 1.0f) progress = 1.0f; }
    return tw.from + (tw.to - tw.from) * tweenEase(progress, tw.easing);
}

static void applyTweens(Layer& layer, uint32_t elapsed) {
    for (const auto& tw : layer.tweens) {
        float val = evaluateTween(tw, elapsed);
        if      (tw.prop == "x")       layer.x       = (int16_t)val;
        else if (tw.prop == "y")       layer.y       = (int16_t)val;
        else if (tw.prop == "opacity") layer.opacity = (uint8_t)constrain((int)val, 0, 255);
    }
}

// -----------------------------------------------------------------------
// renderScreen
// -----------------------------------------------------------------------

void renderScreen(AppState& state, Screen& scr, ScreenState& /*ss*/, const JsonDocument& data) {
    if (!state.inited) initScreenState(state, scr);

    uint32_t now = millis();
    uint32_t dt  = now - state.lastTick;
    state.lastTick = now;

    int textIdx = 0, iconIdx = 0, particleIdx = 0;
    display.clear();

    for (auto& layer : scr.layers) {
        if (!layer.tweens.empty()) applyTweens(layer, now - state.startTime);

        bool needsBlend = (layer.opacity < 255 || layer.blend == "add");
        if (needsBlend) display.snapshotLayer();

        switch (layer.type) {

        case LAYER_PARTICLES: {
            if (particleIdx < (int)state.particleSystems.size()) {
                auto& ps = state.particleSystems[particleIdx];
                ps.tick(dt); ps.render(display); particleIdx++;
            }
            break;
        }

        case LAYER_ICON: {
            if (layer.icon_name.isEmpty() || !state.config.icons.count(layer.icon_name)) break;
            Icon& icon = state.config.icons[layer.icon_name];
            if (icon.frames.empty()) break;
            IconState& is = state.iconStates[iconIdx++];
            if (icon.fps > 0 && icon.frames.size() > 1) {
                uint32_t frameMs = 1000 / icon.fps;
                if (now - is.lastStep >= frameMs) { is.frame = (is.frame + 1) % icon.frames.size(); is.lastStep = now; }
            }
            uint8_t fi = is.frame % icon.frames.size();
            if (icon.remap_key != 0 && !icon.remap_range.stops.empty() && !icon.remap_value_key.isEmpty()) {
                float val = data[icon.remap_value_key.c_str()].as<float>();
                uint32_t newColor = colorFromRange(icon.remap_range, val);
                std::vector<uint8_t> remapped = icon.frames[fi];
                remapIconColor(remapped, icon.remap_key, newColor);
                display.drawSprite(remapped.data(), icon.width, icon.height, layer.x, layer.y);
            } else {
                display.drawSprite(icon.frames[fi].data(), icon.width, icon.height, layer.x, layer.y);
            }
            break;
        }

        case LAYER_TEXT: {
            TextState& ts = state.textStates[textIdx++];
            String text = layer.label;
            if ((!layer.data_url.isEmpty() || !scr.data_url.isEmpty()) && !data.isNull())
                text = configMgr.resolvePlaceholders(layer.label, data);
            if (text != ts.resolved) {
                ts.resolved = text;
                ts.textW = display.textWidth(ts.resolved);
                ts.offset = 0; ts.dir = 1; ts.pauseUntil = 0;
                ts.completedOnce = false; ts.lastStep = now;
                bool needsScroll = (ts.textW + layer.x) > MATRIX_WIDTH;
                if      (layer.scroll == SCROLL_NONE)                              ts.mode = SCROLL_NONE;
                else if (layer.scroll == SCROLL_LEFT || layer.scroll == SCROLL_BOUNCE) ts.mode = layer.scroll;
                else    ts.mode = needsScroll ? SCROLL_BOUNCE : SCROLL_NONE;
            }
            if (ts.mode == SCROLL_NONE) {
                display.drawText(ts.resolved, layer.x, layer.y, layer.color);
            } else if (ts.mode == SCROLL_BOUNCE) {
                display.drawText(ts.resolved, layer.x - ts.offset, layer.y, layer.color);
                display.applyEdgeFade(layer.fade_edge);
                if (now >= ts.pauseUntil && now - ts.lastStep >= layer.scroll_speed) {
                    ts.offset += ts.dir; ts.lastStep = now;
                    int16_t maxOff = ts.textW + layer.x - MATRIX_WIDTH; if (maxOff < 0) maxOff = 0;
                    if (ts.dir == 1 && ts.offset >= maxOff)  { ts.offset = maxOff; ts.dir = -1; ts.pauseUntil = now + 800; }
                    else if (ts.dir == -1 && ts.offset <= 0) { ts.offset = 0; ts.dir = 1; ts.pauseUntil = now + 800; ts.completedOnce = true; }
                }
            } else {
                int16_t drawX = MATRIX_WIDTH - ts.offset;
                display.drawText(ts.resolved, drawX, layer.y, layer.color);
                display.applyEdgeFade(layer.fade_edge);
                if (now - ts.lastStep >= layer.scroll_speed) {
                    ts.offset++; ts.lastStep = now;
                    if (drawX + ts.textW < 0) { ts.offset = 0; ts.completedOnce = true; }
                }
            }
            break;
        }

        case LAYER_CLOCK: {
            TextState& ts = state.textStates[textIdx++];
            char buf[6] = "??:??";
            if (layer.clock_format == "timer") {
                if (state.timer.active) {
                    int32_t rem = state.timerPaused ? state.timerPausedRemaining : (int32_t)(state.timer.endTime - millis());
                    if (rem < 0) rem = 0;
                    snprintf(buf, sizeof(buf), "%02d:%02d", rem / 60000, (rem / 1000) % 60);
                } else snprintf(buf, sizeof(buf), "--:--");
            } else {
                struct tm t;
                if (getLocalTime(&t)) {
                    char sep = (t.tm_sec % 2 == 0) ? ':' : ' ';
                    if (layer.clock_format == "12h") {
                        int h = t.tm_hour % 12; if (h == 0) h = 12;
                        snprintf(buf, sizeof(buf), "%02d%c%02d", h, sep, t.tm_min);
                    } else snprintf(buf, sizeof(buf), "%02d%c%02d", t.tm_hour, sep, t.tm_min);
                }
            }
            ts.resolved = buf;
            int16_t clockX = layer.x;
            if (layer.align == "center" || layer.align == "right") {
                int16_t tw   = display.nativeTextWidth(ts.resolved, layer.native_spacing, layer.native_large);
                int16_t area = layer.align_width > 0 ? layer.align_width : (MATRIX_WIDTH - layer.x);
                clockX = layer.x + (layer.align == "center" ? (area - tw) / 2 : area - tw);
            }
            display.drawNativeText(ts.resolved, clockX, layer.y, layer.color, layer.native_spacing, layer.native_large);
            if (layer.clock_format == "12h") {
                struct tm t2;
                if (getLocalTime(&t2) && t2.tm_hour >= 12)
                    display.drawPixel(layer.x + (layer.native_large ? 26 : 18), layer.y + (layer.native_large ? 6 : 4), layer.color);
            }
            break;
        }

        case LAYER_NATIVE: {
            String text = layer.label;
            if ((!layer.data_url.isEmpty() || !scr.data_url.isEmpty()) && !data.isNull())
                text = configMgr.resolvePlaceholders(layer.label, data);
            int16_t natX = layer.x;
            if (layer.align == "center" || layer.align == "right") {
                int16_t tw   = display.nativeTextWidth(text, layer.native_spacing, layer.native_large);
                int16_t area = layer.align_width > 0 ? layer.align_width : (MATRIX_WIDTH - layer.x);
                natX = layer.x + (layer.align == "center" ? (area - tw) / 2 : area - tw);
            }
            display.drawNativeText(text, natX, layer.y, layer.color, layer.native_spacing, layer.native_large);
            break;
        }

        case LAYER_GAUGE: {
            float val = 0;
            if (!layer.value_key.isEmpty()) val = data[layer.value_key.c_str()].as<float>();
            drawGauge(display, layer.gauge, layer.gauge_w, layer.gauge_h, layer.range, val, layer.x, layer.y);
            break;
        }

        case LAYER_PIXELS: {
            if (layer.pixels_pattern == "week_dots") {
                int dayOfWeek = 0;
                if (!layer.pixels_data_key.isEmpty()) dayOfWeek = data[layer.pixels_data_key.c_str()].as<int>();
                else { struct tm t; if (getLocalTime(&t)) dayOfWeek = t.tm_wday; }
                for (int d = 0; d < 7; d++) {
                    uint32_t c = (d == dayOfWeek) ? layer.pixels_color : layer.pixels_dim_color;
                    int16_t dx = layer.x + d * 3;
                    display.drawPixel(dx, layer.y, c); display.drawPixel(dx + 1, layer.y, c);
                }
            } else if (layer.pixels_pattern == "vline") {
                for (int16_t row = layer.y; row < MATRIX_HEIGHT; row++) display.drawPixel(layer.x, row, layer.pixels_color);
            } else if (layer.pixels_pattern == "dots" && !layer.pixels_points.empty()) {
                for (const auto& pt : layer.pixels_points) display.drawPixel(layer.x + pt.first, layer.y + pt.second, layer.pixels_color);
            }
            break;
        }

        case LAYER_GRADIENT: {
            uint8_t gw = layer.grad_w > 0 ? layer.grad_w : MATRIX_WIDTH;
            uint8_t gh = layer.grad_h > 0 ? layer.grad_h : MATRIX_HEIGHT;
            for (uint8_t gy = 0; gy < gh; gy++) {
                for (uint8_t gx = 0; gx < gw; gx++) {
                    float t;
                    if      (layer.grad_direction == "vertical")  t = (float)gy / (gh - 1);
                    else if (layer.grad_direction == "diagonal")  t = ((float)gx / (gw - 1) + (float)gy / (gh - 1)) * 0.5f;
                    else                                          t = (float)gx / (gw - 1);
                    float val = layer.grad_colors.min_val + t * (layer.grad_colors.max_val - layer.grad_colors.min_val);
                    display.drawPixel(layer.x + gx, layer.y + gy, colorFromRange(layer.grad_colors, val));
                }
            }
            break;
        }

        } // switch

        if      (layer.blend == "add")  display.applyLayerAdditive();
        else if (layer.opacity < 255)   display.applyLayerOpacity(layer.opacity);
    }
}

// -----------------------------------------------------------------------
// Fallback clock
// -----------------------------------------------------------------------

void showClock() {
    struct tm t;
    display.clear();
    char buf[6] = "00:00";
    if (getLocalTime(&t)) snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    else { uint32_t s = millis() / 1000; snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)((s/60)%100), (unsigned)(s%60)); }
    display.drawText(buf, 2, 0, 0x00AAFF);
    display.show();
}
