#pragma once
#include "thinclock.h"
#include <HTTPClient.h>

class ConfigManager {
public:
    bool fetchConfig(const String& url, Config& cfg);
    bool fetchData(const String& url, JsonDocument& doc);
    String resolvePlaceholders(const String& tpl, const JsonDocument& data);
    Layer parseLayer(JsonObject l, uint32_t defaultScrollSpeed);
    void parseIcons(JsonObject icons, std::map<String, Icon>& out);
};
