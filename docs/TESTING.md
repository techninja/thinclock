# Pre-merge Testing Checklist

Branch: `clearstackification` → `main`

This session did heavy refactoring across the server, firmware, and HA integration.
Work through this top to bottom before merging.

**Progress: §0–§5 complete ✅**

---

## 0. Commit pending changes first ✅

> Done. `b30148e` — spec 12/12, all changes committed and pushed.

---

## 1. Server smoke test ✅

> Fixed: helper files (`aqi-routes.js`, etc.) moved to `screens/lib/` so the registry ignores
> them. All screens load cleanly, HA authenticated (688 entities), preview cache generating.

- [x] Server starts without errors
- [x] All screens load — 18 valid screens, 6 helpers correctly excluded
- [x] HA adapter connects — `[ha] Authenticated`, 688 entities loaded
- [x] `/api/config`, `/api/screens`, `/api/active`, `/api/schedules` all return valid JSON
- [x] No import errors — all split files resolve correctly
- [x] Preview cache starts generating after 5s

---

## 2. Firmware compile ✅

> Fixed: `Timer` aggregate init conflict with default member initializers, missing
> `evaluateTween()` call in `applyTweens`, `const Screen&` mismatch in `render_client.cpp`
> extern declaration.

- [x] Compiles without errors
- [x] Flash size 89.6% (up from 87.2% — mDNS + web UI account for the increase)
- [x] `std::any_of` / `#include <algorithm>` accepted by ESP32 toolchain

---

## 3. Flash and device boot test ✅

- [x] Device boots, shows `BOOT` then `WIFI` on display
- [x] Connects to WiFi, scrolls IP in green
- [x] `thinclock.local` resolves
- [x] `/info`, `/sensors` return correct data
- [x] Screens display and cycle correctly
- [x] No rendering regressions from `const` ref / struct initializer changes

---

## 4. Device web UI ✅

- [x] Index page loads with correct IP and config status
- [x] WiFi SSID displays correctly including emoji (🐾)
- [x] Config URL field pre-filled correctly
- [x] Save & Reboot works
- [x] `/info`, `/sensors`, `/status` links work

---

## 5. Server → device integration ✅

> Fixed: device IP was hardcoded from `DEVICE_IP` env var (stale `.60`). Now captured live
> from the WS connection handshake — `DEVICE_IP` is just a cold-start fallback.

- [x] `[ws/device] connected 192.168.86.27` in server logs
- [x] `/api/device/info` proxies correctly — firmware version, chip, RSSI, SSID with emoji
- [x] `/api/device/sensors` proxies correctly — temp, humidity, light
- [x] Button events flowing: `[event] button=right screen=N`
- [ ] `GET http://localhost:3232/api/preview/clock.gif` returns a GIF
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
