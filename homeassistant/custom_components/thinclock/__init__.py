"""ThinClock integration for Home Assistant."""
from __future__ import annotations

import hashlib
import logging

from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.aiohttp_client import async_get_clientsession
from homeassistant.helpers.storage import Store

from .coordinator import ThinClockDeviceCoordinator

_LOGGER = logging.getLogger(__name__)
DOMAIN = "thinclock"
PLATFORMS = ["sensor", "select", "number", "button", "event"]
CARD_URL = "/local/thinclock-card.js"
_card_registered = False


async def _register_lovelace_card(hass: HomeAssistant) -> None:
    """Write card JS URL into lovelace_resources storage if not already present."""
    store = Store(hass, 1, "lovelace_resources")
    data = await store.async_load() or {"items": []}
    items = data.setdefault("items", [])
    if not any(i.get("url") == CARD_URL for i in items):
        items.append({
            "id": hashlib.md5(CARD_URL.encode()).hexdigest(),
            "url": CARD_URL,
            "type": "module",
        })
        await store.async_save(data)
        _LOGGER.info("[thinclock] registered Lovelace card resource")


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    global _card_registered
    session = async_get_clientsession(hass)
    coordinator = ThinClockDeviceCoordinator(
        hass, session,
        server_url=entry.data["server_url"],
        device_ip=entry.data["device_ip"],
    )
    await coordinator.async_config_entry_first_refresh()
    hass.data.setdefault(DOMAIN, {})[entry.entry_id] = coordinator
    await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)

    if not _card_registered:
        await _register_lovelace_card(hass)
        _card_registered = True

    @callback
    def _on_screen_changed(event) -> None:
        hass.async_create_task(coordinator.async_refresh())

    entry.async_on_unload(
        hass.bus.async_listen("thinclock_screen_changed", _on_screen_changed)
    )

    return True


async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    unloaded = await hass.config_entries.async_unload_platforms(entry, PLATFORMS)
    if unloaded:
        hass.data[DOMAIN].pop(entry.entry_id, None)
    return unloaded
