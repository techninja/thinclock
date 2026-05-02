#include "config_manager.h"

bool ConfigManager::fetchConfig(const String& url, Config& cfg) {
    HTTPClient http;
    http.begin(url);
    http.setTimeout(5000);
    int code = http.GET();

    if (code != 200) {
        http.end();
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();

    if (err) return false;

    cfg.screens.clear();
    cfg.sprites.clear();
    cfg.valid = true;

    // Settings
    JsonObject settings = doc["settings"];
    cfg.brightness = settings["brightness"] | 40;
    cfg.timezone_offset = settings["timezone"] | 0;
    cfg.scroll_speed = settings["scroll_speed"] | 50;

    // Screens
    for (JsonObject s : doc["screens"].as<JsonArray>()) {
        Screen scr;
        scr.icon = s["icon"] | "";
        scr.label = s["label"] | "";
        scr.data_url = s["data_url"] | "";
        scr.duration = s["duration"] | 5000;
        scr.text_x = s["x"] | (int16_t)-1;
        scr.text_y = s["y"] | (int16_t)1;
        scr.color = strtoul((s["color"] | "FFFFFF"), NULL, 16);

        // Scroll config
        const char* scrollStr = s["scroll"] | "auto";
        if (strcmp(scrollStr, "none") == 0) scr.scroll = SCROLL_NONE;
        else if (strcmp(scrollStr, "left") == 0) scr.scroll = SCROLL_LEFT;
        else if (strcmp(scrollStr, "bounce") == 0) scr.scroll = SCROLL_BOUNCE;
        else scr.scroll = SCROLL_AUTO;
        scr.scroll_speed = s["scroll_speed"] | cfg.scroll_speed;
        scr.fade_edge = s["fade_edge"] | 2;

        cfg.screens.push_back(scr);
    }

    // Sprites
    for (JsonObject sp : doc["sprites"].as<JsonArray>()) {
        Sprite spr;
        spr.name = sp["name"] | "";
        spr.url = sp["url"] | "";
        spr.width = sp["width"] | 8;
        spr.height = sp["height"] | 8;
        spr.frames = sp["frames"] | 1;
        spr.cached = false;
        cfg.sprites.push_back(spr);
    }

    return true;
}

bool ConfigManager::fetchData(const String& url, JsonDocument& doc) {
    if (url.isEmpty()) return false;

    HTTPClient http;
    http.begin(url);
    http.setTimeout(3000);
    int code = http.GET();

    if (code != 200) {
        http.end();
        return false;
    }

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
        if (data[key].is<const char*>()) {
            value = data[key].as<const char*>();
        } else if (data[key].is<float>()) {
            value = String(data[key].as<float>(), 1);
        } else if (data[key].is<int>()) {
            value = String(data[key].as<int>());
        }
        result = result.substring(0, start) + value + result.substring(end + 1);
    }
    return result;
}
