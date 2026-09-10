#!/usr/bin/with-contenv bashio

# Read options from HA add-on config and export as env vars
export PORT=3232
# Resolve real host IP via Supervisor API (reachable from inside container)
export SERVER_HOST=$(curl -s -H "Authorization: Bearer ${SUPERVISOR_TOKEN}" \
  http://supervisor/network/info | \
  grep -o '"address":\["[0-9./]*' | grep -o '[0-9]*\.[0-9]*\.[0-9]*\.[0-9]*' | head -1)
bashio::log.info "Server host IP: ${SERVER_HOST:-not found}"
echo "http://${SERVER_HOST}:${PORT}" > /config/.thinclock_server
export TIMEZONE="$(bashio::config 'timezone')"
export BRIGHTNESS="$(bashio::config 'brightness')"
export BRIGHTNESS_NIGHT="$(bashio::config 'brightness_night')"
export NIGHT_HOURS="$(bashio::config 'night_hours')"
export TIME_FORMAT="$(bashio::config 'time_format')"
export TEMP_UNIT="$(bashio::config 'temp_unit')"
export SCREEN_BLOCKLIST="$(bashio::config 'screen_blocklist')"
export MAX_SCREENS="$(bashio::config 'max_screens')"
export ALLOW_BEEPING="$(bashio::config 'allow_beeping')"
export WIFI_SSID="$(bashio::config 'wifi_ssid')"
export WIFI_PASS="$(bashio::config 'wifi_pass')"
export OWM_API_KEY="$(bashio::config 'owm_api_key')"
export OWM_CITY="$(bashio::config 'owm_city')"

# Wire up HA websocket — supervisor proxy requires this exact URL + SUPERVISOR_TOKEN
export HA_URL="http://supervisor/core"
export HA_TOKEN="${SUPERVISOR_TOKEN}"

bashio::log.info "Starting ThinClock server on port ${PORT}"
bashio::log.info "Device IP: ${DEVICE_IP:-not set}"

exec node /app/src/server.js
