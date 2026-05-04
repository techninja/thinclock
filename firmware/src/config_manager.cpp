#include "config_manager.h"
#include "sensors.h"

extern Sensors sensors;

static std::vector<uint8_t> hexToBytes(const char* hex) {
    std::vector<uint8_t> bytes;
    size_t len = strlen(hex);
    for (size_t i = 0; i + 1 < len; i += 2) {
        char pair[3] = { hex[i], hex[i+1], 0 };
        bytes.push_back((uint8_t)strtoul(pair, NULL, 16));
    }
    return bytes;
}

static ColorRange parseColorRange(JsonObject obj) {
    ColorRange cr;
    cr.min_val = obj["min"] | 0.0f;
    cr.max_val = obj["max"] | 100.0f;
    JsonArray stops = obj["stops"];
    if (stops) {
        for (JsonArray stop : stops) {
            ColorStop cs;
            cs.pos = stop[0].as<float>();
            uint32_t c = strtoul(stop[1].as<const char*>(), NULL, 16);
            cs.r = (c >> 16) & 0xFF;
            cs.g = (c >> 8) & 0xFF;
            cs.b = c & 0xFF;
            cr.stops.push_back(cs);
        }
    }
    return cr;
}

static ParticleConfig parseParticles(JsonObject pc) {
    ParticleConfig cfg;
    cfg.active = true;
    cfg.gravity = pc["gravity"] | 0.0f;
    const char* edgeStr = pc["edge"] | "die";
    if (strcmp(edgeStr, "bounce") == 0) cfg.edge = PB_BOUNCE;
    else if (strcmp(edgeStr, "wrap") == 0) cfg.edge = PB_WRAP;
    else cfg.edge = PB_DIE;
    cfg.mask = pc["mask"] | "";
    if (pc["colors"].is<JsonObject>()) {
        cfg.colors = parseColorRange(pc["colors"]);
    }
    for (JsonObject em : pc["emitters"].as<JsonArray>()) {
        ParticleEmitter e;
        e.x = em["x"] | -1.0f;
        e.y = em["y"] | -1.0f;
        e.vx_min = em["vx_min"] | 0.0f;
        e.vx_max = em["vx_max"] | 0.0f;
        e.vy_min = em["vy_min"] | 0.0f;
        e.vy_max = em["vy_max"] | 0.0f;
        e.rate = em["rate"] | 1.0f;
        e.lifetime_min = em["life_min"] | 1000;
        e.lifetime_max = em["life_max"] | 3000;
        e.size = em["size"] | 1;
        e.is_rocket = em["rocket"] | false;
        e.accumulator = 0;
        cfg.emitters.push_back(e);
    }
    return cfg;
}

static Layer parseLayer(JsonObject l, uint32_t defaultScrollSpeed) {
    Layer layer;
    layer.x = l["x"] | (int16_t)0;
    layer.y = l["y"] | (int16_t)0;
    layer.opacity = l["opacity"] | 255;

    const char* typeStr = l["type"] | "";
    if (strcmp(typeStr, "icon") == 0) {
        layer.type = LAYER_ICON;
        layer.icon_name = l["name"] | "";
    } else if (strcmp(typeStr, "text") == 0) {
        layer.type = LAYER_TEXT;
        layer.label = l["label"] | "";
        layer.data_url = l["data_url"] | "";
        layer.color = strtoul((l["color"] | "FFFFFF"), NULL, 16);
        const char* scrollStr = l["scroll"] | "none";
        if (strcmp(scrollStr, "auto") == 0) layer.scroll = SCROLL_AUTO;
        else if (strcmp(scrollStr, "left") == 0) layer.scroll = SCROLL_LEFT;
        else if (strcmp(scrollStr, "bounce") == 0) layer.scroll = SCROLL_BOUNCE;
        else layer.scroll = SCROLL_NONE;
        layer.scroll_speed = l["scroll_speed"] | defaultScrollSpeed;
        layer.fade_edge = l["fade_edge"] | 2;
    } else if (strcmp(typeStr, "particles") == 0) {
        layer.type = LAYER_PARTICLES;
        layer.particles = parseParticles(l);
    } else if (strcmp(typeStr, "gauge") == 0) {
        layer.type = LAYER_GAUGE;
        const char* gs = l["style"] | "vbar";
        if (strcmp(gs, "hbar") == 0) layer.gauge = GAUGE_HBAR;
        else if (strcmp(gs, "dot") == 0) layer.gauge = GAUGE_DOT;
        else layer.gauge = GAUGE_VBAR;
        layer.gauge_w = l["width"] | 8;
        layer.gauge_h = l["height"] | 8;
        layer.value_key = l["value_key"] | "";
        if (l["range"].is<JsonObject>()) {
            layer.range = parseColorRange(l["range"]);
        }
    } else if (strcmp(typeStr, "clock") == 0) {
        layer.type = LAYER_CLOCK;
        layer.clock_format = l["format"] | "24h";
        layer.color = strtoul((l["color"] | "4488FF"), NULL, 16);
        layer.native_large = l["large"] | true;
        layer.native_spacing = l["spacing"] | 1;
    } else if (strcmp(typeStr, "native") == 0) {
        layer.type = LAYER_NATIVE;
        layer.label = l["label"] | "";
        layer.data_url = l["data_url"] | "";
        layer.color = strtoul((l["color"] | "FFFFFF"), NULL, 16);
        layer.native_large = l["large"] | false;
        layer.native_spacing = l["spacing"] | 1;
    } else if (strcmp(typeStr, "pixels") == 0) {
        layer.type = LAYER_PIXELS;
        layer.pixels_pattern = l["pattern"] | "";
        layer.pixels_data_key = l["data_key"] | "";
        layer.pixels_color = strtoul((l["color"] | "4488FF"), NULL, 16);
        layer.pixels_dim_color = strtoul((l["dim_color"] | "112233"), NULL, 16);
    } else if (strcmp(typeStr, "gradient") == 0) {
        layer.type = LAYER_GRADIENT;
        layer.grad_w = l["width"] | 0;
        layer.grad_h = l["height"] | 0;
        layer.grad_direction = l["direction"] | "horizontal";
        if (l["colors"].is<JsonObject>()) {
            layer.grad_colors = parseColorRange(l["colors"]);
        }
    }

    // Tweens
    if (l["tweens"].is<JsonArray>()) {
        for (JsonObject tw : l["tweens"].as<JsonArray>()) {
            Layer::Tween tween;
            tween.prop = tw["prop"] | "x";
            tween.from = tw["from"] | 0.0f;
            tween.to = tw["to"] | 0.0f;
            tween.duration = tw["duration"] | 1000;
            tween.easing = tw["easing"] | "linear";
            tween.loop = tw["loop"] | "none";
            tween.delay = tw["delay"] | 0;
            layer.tweens.push_back(tween);
        }
    }

    return layer;
}

bool ConfigManager::fetchConfig(const String& url, Config& cfg) {
    HTTPClient http;
    http.begin(url);
    http.setTimeout(5000);
    int code = http.GET();
    if (code != 200) { http.end(); return false; }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();
    if (err) return false;

    cfg.screens.clear();
    cfg.icons.clear();
    cfg.valid = true;

    // Settings
    JsonObject settings = doc["settings"];
    cfg.brightness = settings["brightness"] | 40;
    cfg.timezone_offset = settings["timezone"] | 0;
    cfg.scroll_speed = settings["scroll_speed"] | 50;
    cfg.event_url = settings["event_url"] | "";
    cfg.time_format = settings["time_format"] | "24h";
    cfg.temp_unit = settings["temp_unit"] | "C";
    cfg.transition_ms = settings["transition"] | 8;
    cfg.buttons = settings["buttons"] | "navigate";
    cfg.allow_beep = settings["allow_beep"] | true;

    // Screens
    for (JsonObject s : doc["screens"].as<JsonArray>()) {
        Screen scr;
        scr.duration = s["duration"] | 5000;
        scr.data_url = s["data_url"] | "";

        for (JsonObject l : s["layers"].as<JsonArray>()) {
            scr.layers.push_back(parseLayer(l, cfg.scroll_speed));
        }
        cfg.screens.push_back(scr);
    }

    // Icons
    JsonObject icons = doc["icons"];
    for (JsonPair kv : icons) {
        Icon icon;
        JsonObject obj = kv.value();
        icon.width = obj["width"] | 8;
        icon.height = obj["height"] | 8;
        icon.fps = obj["fps"] | 0;

        JsonArray dataArr = obj["data"];
        if (dataArr) {
            for (JsonVariant frame : dataArr) {
                icon.frames.push_back(hexToBytes(frame.as<const char*>()));
            }
        }
        icon.remap_key = obj["remap_key"].is<const char*>()
            ? strtoul(obj["remap_key"].as<const char*>(), NULL, 16) : 0;
        icon.remap_value_key = obj["remap_value_key"] | "";
        if (obj["remap_range"].is<JsonObject>()) {
            icon.remap_range = parseColorRange(obj["remap_range"]);
        }
        cfg.icons[String(kv.key().c_str())] = icon;
    }

    return true;
}

bool ConfigManager::fetchData(const String& url, JsonDocument& doc) {
    if (url.isEmpty()) return false;

    if (url.startsWith("self://")) {
        String path = url.substring(7);
        if (path == "sensors" || path == "/sensors") {
            doc["temperature"] = round(sensors.data.temperature * 10.0) / 10.0;
            doc["humidity"] = round(sensors.data.humidity * 10.0) / 10.0;
            doc["light"] = (int)sensors.data.lightPct;
            doc["light_raw"] = (int)sensors.data.light;
            return true;
        }
        return false;
    }

    HTTPClient http;
    http.begin(url);
    http.setTimeout(3000);
    int code = http.GET();
    if (code != 200) { http.end(); return false; }
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();
    return !err;
}

String ConfigManager::resolvePlaceholders(const String& tpl, const JsonDocument& data) {
    String result = tpl;
    int start;
    while ((start = result.indexOf('{')) >= 0) {
        int end = result.indexOf('}', start);
        if (end < 0) break;
        String key = result.substring(start + 1, end);
        String value = "";
        JsonVariantConst v = data[key.c_str()];
        if (!v.isNull()) {
            if (v.is<const char*>()) value = v.as<const char*>();
            else if (v.is<long>()) value = String(v.as<long>());
            else if (v.is<double>()) {
                double d = v.as<double>();
                if (d == (long)d) value = String((long)d);
                else value = String(d, 1);
            } else if (v.is<bool>()) value = v.as<bool>() ? "1" : "0";
            else value = v.as<String>();
        }
        result = result.substring(0, start) + value + result.substring(end + 1);
    }
    return result;
}
