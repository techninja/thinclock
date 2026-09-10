#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "setup_page.h"

String setupPageHTML(const String& ssid, const String& cfgURL, bool apMode,
                     bool badPassword, const std::vector<ScannedNet>& scannedNets) {
    String ssidField;
    if (apMode) {
        String options;
        for (const auto& net : scannedNets) {
            String s = net.ssid; s.replace("'", "&#39;");
            options += "<option value='" + s + "'" + (s == ssid ? " selected" : "") +
                       ">" + s + " (" + net.rssi + " dBm)</option>";
        }
        ssidField = options.isEmpty()
            ? "<label><span>WiFi Network</span><input name='ssid' value='" + ssid + "' autocomplete='off' autocapitalize='none'></label>"
            : "<label><span>WiFi Network</span><select name='ssid' style='display:block;width:100%;padding:.65rem .75rem;border-radius:8px;border:1px solid #333;background:#2a2a2a;color:#fff;font-size:1rem'>" + options + "</select></label>";
    } else {
        ssidField = "<label><span>WiFi Network</span><input name='ssid' value='" + ssid + "' autocomplete='off' autocapitalize='none'></label>";
    }

    String connStatus;
    if (apMode) {
        connStatus = badPassword && !ssid.isEmpty()
            ? "<p class='note'><span style='color:#f64'>&#x2718; Wrong password for <b>" + ssid + "</b></span><br>Enter the correct password below.</p>"
            : "<p class='note'>Not connected &mdash; enter your WiFi details below.</p>";
    } else {
        String cfg = cfgURL.isEmpty()
            ? "<span class='dim'>Auto-detect via mDNS</span>"
            : "<span class='ok'>&#x2714; " + cfgURL + "</span>";
        connStatus = "<p class='note'>IP: <b>" + WiFi.localIP().toString() + "</b><br>Server: " + cfg + "</p>";
    }

    return
        "<!doctype html><html><head>"
        "<meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>ThinClock Setup</title>"
        "<style>"
        "*{box-sizing:border-box;margin:0;padding:0}"
        "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
             "background:#111;color:#eee;min-height:100vh;"
             "display:flex;align-items:center;justify-content:center;padding:1rem}"
        ".card{background:#1e1e1e;border-radius:12px;padding:1.5rem;width:100%;max-width:400px}"
        "h2{font-size:1.3rem;margin-bottom:1rem;color:#fff}"
        ".note{font-size:.85rem;color:#aaa;margin-bottom:1.2rem;line-height:1.5}"
        ".ok{color:#4c4}.dim{color:#888}"
        "label{display:block;margin-bottom:1rem}"
        "label span{display:block;font-size:.8rem;color:#aaa;margin-bottom:.35rem}"
        "input{display:block;width:100%;padding:.65rem .75rem;border-radius:8px;"
              "border:1px solid #333;background:#2a2a2a;color:#fff;font-size:1rem}"
        "input:focus{outline:none;border-color:#4af}"
        "button{width:100%;padding:.75rem;border-radius:8px;border:none;"
               "background:#2979ff;color:#fff;font-size:1rem;font-weight:600;"
               "cursor:pointer;margin-top:.5rem}"
        "button:active{background:#1a5fd4}"
        "</style></head><body>"
        "<div class='card'>"
        "<h2>&#x1F551; ThinClock</h2>"
        + connStatus +
        "<form method='POST' action='/setup'>"
        + ssidField +
        "<label><span>WiFi Password</span><input name='pass' type='password' placeholder='" +
        String(apMode ? "" : "(unchanged)") + "' autocomplete='current-password'></label>"
        "<button type='submit'>Save &amp; Reboot</button>"
        "</form>"
        "</div></body></html>";
}

void handleSetupPost(WebServer& server, Preferences& prefs) {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    Serial.printf("[setup] POST ssid='%s' pass_len=%d\n", ssid.c_str(), pass.length());
    if (ssid.isEmpty()) { server.send(400, "text/plain", "SSID required"); return; }
    prefs.begin("thinclock", false);
    prefs.putString("ssid", ssid);
    if (!pass.isEmpty()) prefs.putString("pass", pass);
    prefs.putBool("sta_pending", true);
    prefs.end();
    Serial.println("[setup] NVS written, rebooting");
    server.send(200, "text/html; charset=utf-8",
        "<!doctype html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<style>body{font-family:-apple-system,sans-serif;background:#111;color:#eee;"
        "display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0}"
        ".card{background:#1e1e1e;border-radius:12px;padding:2rem;text-align:center}</style></head>"
        "<body><div class='card'><h2>&#x2714; Saved!</h2><p style='color:#aaa;margin-top:.5rem'>Rebooting&hellip;</p></div></body></html>"
    );
    delay(1000);
    ESP.restart();
}
