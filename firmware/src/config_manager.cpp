#include "config_manager.h"
#include "sensors.h"

extern Sensors sensors;

// Decode hex string to byte array: "FF00AA" → [0xFF, 0x00, 0xAA]
static std::vector<uint8_t> hexToBytes(const char* hex) {
    std::vector<uint8_t> bytes;
    size_t len = strlen(hex);
    for (size_t i = 0; i + 1 < len; i += 2) {
        char pair[3] = { hex[i], hex[i+1], 0 };
        bytes.push_back((uint8_t)strtoul(pair, NULL, 16));
    }
    return bytes;
}

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
        cfg.icons[String(kv.key().c_str())] = icon;
    }

    return true;
}

bool ConfigManager::fetchData(const String& url, JsonDocument& doc) {
    if (url.isEmpty()) return false;

    // self:// URLs resolve from onboard data, no HTTP needed
    if (url.startsWith("self://")) {
        String path = url.substring(7);
        if (path == "sensors" || path == "/sensors") {
            doc["temperature"] = round(sensors.data.temperature * 10.0) / 10.0;
            doc["humidity"] = round(sensors.data.humidity * 10.0) / 10.0;
            doc["light"] = (int)sensors.data.lightPct;
            doc["light_raw"] = (int)sensors.data.light;
            return true;  // LDR always available
        }
        return false;
    }

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
        JsonVariantConst v = data[key.c_str()];
        if (!v.isNull()) {
            if (v.is<const char*>()) {
                value = v.as<const char*>();
            } else if (v.is<long>()) {
                value = String(v.as<long>());
            } else if (v.is<double>()) {
                // Show 1 decimal, but trim .0 for whole numbers
                double d = v.as<double>();
                if (d == (long)d) value = String((long)d);
                else value = String(d, 1);
            } else if (v.is<bool>()) {
                value = v.as<bool>() ? "1" : "0";
            } else {
                value = v.as<String>();
            }
        }
        result = result.substring(0, start) + value + result.substring(end + 1);
    }
    return result;
}
