# Pre-merge Testing Checklist

Branch: `clearstackification` → `main`

This session did heavy refactoring across the server, firmware, and HA integration.
Nothing has been runtime-tested since the WiFi/mDNS work. Work through this top to bottom
before merging.

---

## 0. Commit pending changes first

Everything is staged but not committed. Run:

```bash
npm run spec -- all   # must be 12/12
git add -A
git commit -m "Spec 12/12: firmware cppcheck, file splits, HA integration, env unification"
git push
```

---

## 1. Server smoke test (local, no device needed)

```bash
npm run dev
```

**Check:**

- [ ] Server starts without errors
- [ ] All screens load — `Loading screens:` lists expected count
- [ ] HA adapter connects — `[ha] Authenticated` and entity count logged
- [ ] `GET http://localhost:3232/api/config` returns valid JSON with `settings`, `screens`, `icons`
- [ ] `GET http://localhost:3232/api/screens` returns screen list
- [ ] `GET http://localhost:3232/api/active` returns active screens
- [ ] `GET http://localhost:3232/api/schedules` returns schedule definitions
- [ ] `GET http://localhost:3232/api/device-ip` returns `{"ip":"192.168.86.60"}`
- [ ] No import errors — all the new split files (`device-proxy.js`, `ws-render.js`, `routes.js`, etc.) resolve correctly
- [ ] Preview cache starts generating after 5s — `[preview] generating ...` in logs

**Known risk:** `src/server.js` was heavily restructured. The `queueRender` export is now in
`ws-render.js` but nothing calls it yet — that's fine, just confirm no startup crash.

---

## 2. Firmware compile

```bash
npm run build
```

**Check:**

- [ ] Compiles without errors
- [ ] No new warnings beyond what existed before (cppcheck already validated this)
- [ ] Flash size still reasonable (was 87.2% last known good)

**Known risk:** `std::any_of` added to `main.cpp` with `#include <algorithm>` — confirm
Arduino/ESP32 toolchain accepts this (it should, it's standard C++11).

---

## 3. Flash and device boot test

```bash
npm run flash
npm run monitor
```

**Check:**

- [ ] Device boots, shows `BOOT` then `WIFI` on display
- [ ] Connects to WiFi, scrolls IP in green
- [ ] `[mdns] thinclock.local` logged
- [ ] `thinclock.local` resolves: `curl http://thinclock.local/info`
- [ ] `/info` returns correct firmware version, IP, SSID (with emoji if applicable)
- [ ] `/sensors` returns temperature, humidity, light values
- [ ] Device fetches config from server: `[config] fetched N screens` in serial
- [ ] Screens display and cycle correctly
- [ ] Middle button press scrolls IP when config invalid (test by stopping server)

**Known risk:** The `const` ref changes and struct default initializers in `thinclock.h` —
these are correct C++ but worth confirming nothing regressed in rendering behavior.

---

## 4. Device web UI

Open `http://thinclock.local` in a phone browser.

**Check:**

- [ ] Index page loads with correct IP and config status
- [ ] WiFi SSID displays correctly including emoji (🐾)
- [ ] Config URL field pre-filled correctly
- [ ] Save & Reboot works (test with a dummy change, revert after)
- [ ] `/info`, `/sensors`, `/status` links work

---

## 5. Server → device integration

With both server and device running:

**Check:**

- [ ] Live preview WebSocket connects: `[ws/device] connected` in server logs
- [ ] `GET http://localhost:3232/api/device/info` proxies correctly
- [ ] `GET http://localhost:3232/api/device/sensors` proxies correctly
- [ ] `GET http://localhost:3232/api/preview/clock.gif` returns a GIF (may take a moment to generate)
- [ ] `POST http://localhost:3232/api/preview/regenerate` clears and re-queues cache

---

## 6. Web UI (browser)

Open `http://localhost:3232` in browser.

**Check:**

- [ ] Dashboard loads, live preview shows device framebuffer
- [ ] Device info section populates (IP, uptime, temp, humidity)
- [ ] Rotation page loads, screen list renders
- [ ] Preview GIFs load (or show loading state then load after generation)
- [ ] Toggle a screen on/off — confirm it persists across page reload
- [ ] Editor page loads, can add/remove layers, save a custom screen
- [ ] Settings page loads with device info
- [ ] Notify page loads, can send a test notification

---

## 7. HA Integration (via SSH to Yellow)

SSH setup first:

```bash
ssh-copy-id root@homeassistant.local
```

### 7a. Install custom component

```bash
scp -r homeassistant/custom_components/thinclock root@homeassistant.local:/config/custom_components/
ssh root@homeassistant.local "ha core restart"
```

**Check:**

- [ ] HA restarts without errors
- [ ] Navigate to Settings → Integrations
- [ ] "New device discovered: ThinClock" notification appears (zeroconf)
  - If not: add manually via "+ Add Integration" → ThinClock → enter `http://192.168.86.48:3232`
- [ ] Confirm discovery — device appears with correct IP and firmware version
- [ ] Integration creates device with 5 entities:
  - [ ] `sensor.thinclock_temperature`
  - [ ] `sensor.thinclock_humidity`
  - [ ] `sensor.thinclock_light`
  - [ ] `select.thinclock_current_screen`
  - [ ] `number.thinclock_brightness`
  - [ ] `button.thinclock_button_left`
  - [ ] `button.thinclock_button_middle`
  - [ ] `button.thinclock_button_right`
- [ ] Sensor values update every 30s
- [ ] Screen select shows available screens, changing it switches the display
- [ ] Button entities fire when pressed in HA UI

### 7b. Clean up Awtrix cruft

While in HA, delete the old Awtrix entities:

- `input_button.push_awtrix_app`
- `input_boolean.test_notify`
- `input_boolean.awtrix_2`
- `input_boolean.awtrix_3`
- `automation.awtrix_test_notify`
- `automation.awtrix_custom_sensor`
- `automation.awtrix_test_button`

---

## 8. HA Add-on (local sideload)

```bash
ssh root@homeassistant.local "mkdir -p /addons/thinclock"
scp -r thinclock-addon/* root@homeassistant.local:/addons/thinclock/
ssh root@homeassistant.local "ha supervisor reload"
```

**Check:**

- [ ] Add-on appears in HA under Settings → Add-ons → Local add-ons
- [ ] Configure options (device IP, timezone, HA token auto-wired)
- [ ] Start add-on — check logs for server startup output
- [ ] `http://homeassistant.local:3232/api/config` responds
- [ ] HA ingress panel shows ThinClock UI in sidebar
- [ ] HA adapter connects using `SUPERVISOR_TOKEN` — `[ha] Authenticated` in add-on logs
- [ ] `/data/preview-cache/` persists across add-on restart

**Known risk:** The `Dockerfile` copies from repo root — when sideloading, the build context
is `/addons/thinclock/` not the repo root, so `COPY src/` will fail. The Dockerfile needs
the server source present. Options:

- Copy `src/`, `package.json`, `package-lock.json` alongside the addon files, OR
- Build and push a Docker image and reference it in `config.yaml`

This is the most likely thing to need a fix before the add-on works end-to-end.

---

## 9. Merge to main

Once all checks above pass:

```bash
git checkout main
git merge clearstackification
git push origin main
git tag v0.1.0
git push origin v0.1.0
```

---

## Known deferred items (not blockers for merge)

- `POST /api/device/render` WS path — `queueRender` is implemented but no HTTP endpoint
  calls it yet. The HTTP `/render` proxy still works directly.
- Editor preview — shows "Save and push to see on device" placeholder, no live canvas preview
- `number.thinclock_brightness` — the `/api/device/display` brightness endpoint doesn't exist
  yet on the server side, entity will fail silently
- Add-on Docker build context issue (see §8 above)
- i18n warning in spec is expected (en-only project, ⚠️ not ❌)
