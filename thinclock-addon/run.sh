#!/usr/bin/with-contenv bashio

# Read options from HA add-on config and export as env vars
export PORT=3232
export DEVICE_IP="$(bashio::config 'device_ip')"
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

# Wire up HA supervisor API for the HA adapter
export HA_URL="http://supervisor/core"
export HA_TOKEN="${SUPERVISOR_TOKEN}"

bashio::log.info "Starting ThinClock server on port ${PORT}"
bashio::log.info "Device IP: ${DEVICE_IP:-not set}"

exec node /app/src/server.js
