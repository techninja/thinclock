#pragma once
#include "thinclock.h"
#include <HTTPClient.h>

class ConfigManager {
public:
    bool fetchConfig(const String& url, Config& cfg);
    bool fetchData(const String& url, JsonDocument& doc);
    String resolvePlaceholders(const String& tpl, const JsonDocument& data);
};
