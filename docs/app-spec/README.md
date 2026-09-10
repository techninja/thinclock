# thinclock Web UI — App Spec

## Vision

A browser-based control panel for thinclock devices that lets users manage screen rotations, create custom animated scenes without code, preview everything in real-time, and share creations with the community.

The UI serves as both a power-user tool (JSON editing, live preview) and an accessible creative canvas (drag/position layers, slider-driven config, visual timeline).

---

## Core Entities

### Device

- IP address, firmware version, build hash, uptime, WiFi info
- Current screen index, brightness, config URL
- Sensor readings (temp, humidity, light, ping)
- Timer/notification state

### Screen (Native)

- Built-in JS modules in `src/api/screens/`
- Has: name, priority, schedule, tags, layers, icons, alerts
- Managed via enable/disable, not editable in UI

### Screen (Custom)

- User-created via the UI
- Stored as JSON in `data/custom-screens/`
- Same layer format as native screens
- Editable: layers, duration, schedule, icons, data sources
- Shareable via export/import

### Community Screen

- Shared screen definitions hosted externally (GitHub Gists, registry repo)
- Importable into local custom screens
- Versioned, attributed, safety-reviewed

---

## Pages

### 1. Dashboard (`/`)

- Device status card (IP, version, WiFi, uptime, sensors)
- Current rotation preview (list with mini visual thumbnails)
- Quick actions: send notification, start timer, test beep
- Active alerts indicator

### 2. Rotation Manager (`/rotation`)

- All screens (native + custom) in a sortable list
- Toggle enabled/disabled per screen
- Visual preview thumbnail for each (static render or animated GIF)
- Schedule indicators (clock icon, date range)
- Priority adjustment (drag to reorder)
- "Preview on device" button — pushes screen to device temporarily

### 3. Screen Editor (`/editor` / `/editor/:id`)

- **Layer stack** — ordered list of layers, add/remove/reorder
- **Layer inspector** — per-layer config panel:
  - Type selector (icon, text, native, clock, particles, gradient, gauge, pixels)
  - Position (x, y) with drag-on-preview
  - Opacity slider
  - Blend mode toggle
  - Tween editor (property, from/to, duration, easing, loop)
  - Type-specific fields (particle emitters, gradient stops, icon pixel editor)
- **Live preview** — 32×8 pixel canvas that renders the screen in real-time
  - Runs the same layer compositing logic as the device (JS port)
  - Shows tweens animating, particles moving, text scrolling
  - "Push to device" sends current state for immediate display
- **Data binding** — connect `{placeholder}` values to:
  - Static test values (for preview)
  - Device self:// endpoints
  - Server API endpoints
  - Custom URLs
- **Icon editor** — 8×8 pixel grid painter
  - Color picker, eraser, fill
  - Frame timeline for animations
  - Import from community icon packs
- **JSON view** — raw JSON editor with syntax highlighting
  - Bidirectional: visual edits update JSON, JSON edits update visual
  - Validation against the layer schema
- **Schedule config** — hours, days, months, date ranges
- **Save** — writes to `data/custom-screens/` as JSON
- **Export** — downloads as standalone JSON file

### 4. Community (`/community`)

- Browse shared screens from a registry
- Preview thumbnails (rendered server-side or client-side)
- Import to local custom screens (one-click)
- Publish your custom screens (creates a Gist or PR)
- Attribution: author, version, description, tags
- Safety: screens are JSON-only (no executable code), validated on import

### 5. Settings (`/settings`)

- **Device config**: brightness, timezone, time format, temp unit, night hours
- **Server config**: screen mode, max screens, working hours, comfort temps
- **WiFi**: current SSID, signal strength, config URL
- **Firmware**: version, build, free RAM, reboot button
- **Notifications**: test send, clear all, view queue
- **Timer**: start/cancel, pomodoro config (work/break durations)
- **Alerts**: view registered conditions, test triggers, cooldown status
- **Beep patterns**: test named patterns, create custom

---

## Preview Renderer

A client-side JavaScript implementation of the thinclock display engine:

- 32×8 canvas (scaled up for visibility)
- Renders all layer types: gradient, pixels/dots, icon sprites, native text, particles
- Animates tweens and particle systems at 30fps
- Used in: rotation thumbnails, editor live preview, community browse

This is a **JS port of the firmware rendering logic** — same layer format, same compositing order, same particle physics. Ensures what you see in the browser matches what appears on the device.

---

## Data Storage

### Server-side (JSON files)

```
data/
├── custom-screens/       # User-created screens
│   ├── my-clock.json
│   └── birthday-scene.json
├── settings.json         # Runtime settings overrides
└── community-cache/      # Cached imported community screens
```

### No database required

- JSON files on disk, read/written by the server
- Settings changes are runtime-only by default (don't modify .env)
- Optional: write-back to `data/settings.json` for persistence across restarts

---

## Community Registry

### Architecture

- **GitHub repository** as the registry (e.g., `thinclock/community-screens`)
- Each screen is a directory with:
  - `screen.json` — the layer definition
  - `meta.json` — author, description, tags, version, preview image
  - `preview.png` — rendered thumbnail (generated)
- **Discovery**: server fetches registry index on demand
- **Import**: downloads `screen.json`, saves to `data/custom-screens/`
- **Publish**: UI generates a Gist or opens a PR to the registry repo

### Safety Model

- Screens are pure JSON — no executable code
- All values are validated against the layer schema on import
- No `data_url` pointing to external servers (or user must explicitly approve)
- Icons are inline hex data (no external image loading)
- Community screens cannot define `alerts` (server-side only)

---

## API Additions Needed

### Server endpoints (new)

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/custom-screens` | GET | List custom screens |
| `/api/custom-screens` | POST | Create new custom screen |
| `/api/custom-screens/:id` | GET | Get custom screen JSON |
| `/api/custom-screens/:id` | PUT | Update custom screen |
| `/api/custom-screens/:id` | DELETE | Delete custom screen |
| `/api/preview/:id` | GET | Render preview image (PNG) |
| `/api/community` | GET | Browse community registry |
| `/api/community/import` | POST | Import a community screen |
| `/api/community/publish` | POST | Publish a custom screen |
| `/api/settings` | GET/PUT | Runtime settings |
| `/api/device/reboot` | POST | Trigger device reboot |

### Firmware endpoints (new)

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/display` | POST | Push layers for immediate render (already stubbed) |
| `/reboot` | POST | Restart device |
| `/config-url` | PUT | Update stored config URL |

---

## Build Phases

### Phase 1: Foundation

- [ ] Dashboard page with device info and active rotation
- [ ] Screens list page (enable/disable native screens)
- [ ] Settings page (view/edit brightness, timezone, etc.)
- [ ] Notification sender form
- [ ] Basic routing between pages

### Phase 2: Preview Engine

- [ ] Client-side 32×8 canvas renderer
- [ ] Render gradient layers
- [ ] Render pixel/dots layers
- [ ] Render native text (port 3×5 and 5×7 fonts to JS)
- [ ] Render icon sprites from hex data
- [ ] Animate tweens
- [ ] Particle system (port physics + rendering)
- [ ] Use previews as thumbnails in rotation list

### Phase 3: Screen Editor

- [ ] Layer stack UI (add/remove/reorder)
- [ ] Layer type selector with config forms
- [ ] Position drag on preview canvas
- [ ] Live preview with animation
- [ ] JSON editor (bidirectional sync)
- [ ] Save/load custom screens
- [ ] "Push to device" for instant preview
- [ ] Icon pixel editor (8×8 grid painter)

### Phase 4: Community

- [ ] Registry browser with search/filter
- [ ] Preview rendering for community screens
- [ ] One-click import
- [ ] Publish flow (Gist creation or PR)
- [ ] Attribution and versioning

### Phase 5: Polish

- [ ] Mobile/tablet responsive layout
- [ ] Drag-to-position layers on touch devices
- [ ] Undo/redo in editor
- [ ] Screen templates (start from a base)
- [ ] Keyboard shortcuts
- [ ] Dark/light theme toggle
- [ ] PWA support (installable, works offline for editing)

---

## Tech Stack

- **Framework**: Hybrids.js (via Clearstack)
- **Rendering**: Canvas 2D API (scaled 32×8 → visible size)
- **State**: Hybrids store (localStorage for drafts, server for persistence)
- **Routing**: Hybrids router
- **Styling**: CSS custom properties, no preprocessor
- **Icons**: Lucide (via Clearstack icon system)
- **No build step**: ES modules, import maps, served directly

---

## Starting Prompt for Next Session

```
Continue building the thinclock web UI. The project uses Clearstack (Hybrids.js, 
no-build, ES modules) at the repo root with the server at src/server.js and 
API modules in src/api/. The firmware is in firmware/.

Current state: Server is running with all API endpoints at /api/*, UI shell 
exists at src/ with Clearstack scaffolding (router, pages, styles, vendor).

Next steps (Phase 1):
1. Build the Dashboard page — fetch /api/active, /api/device-ip, and device 
   /info endpoint. Show device status, active rotation list, quick actions.
2. Build the Screens page — fetch /api/screens, show all with enable/disable 
   toggles, schedule/priority info.
3. Build the Settings page — fetch /api/device-ip then device /info and 
   /sensors. Show editable brightness, timezone, etc.
4. Build the Notify page — form to POST to device /notify with text, color, 
   beep options.

Key constraints:
- Clearstack conventions: ≤150 lines per file, light DOM, JSDoc types, 
  atomic design (atoms/molecules/organisms)
- All state via Hybrids store models with fetch connectors
- No build step — everything runs as-is in the browser
- Device communication goes through /api/device-ip to get the IP, then 
  direct HTTP to the device (CORS enabled on both server and device)

See docs/app-spec/ for full feature spec and phase breakdown.
See docs/clearstack/ for framework conventions and patterns.
```
