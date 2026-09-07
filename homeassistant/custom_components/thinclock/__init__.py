"""ThinClock integration for Home Assistant."""
from __future__ import annotations

import logging
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.aiohttp_client import async_get_clientsession

from .coordinator import ThinClockDeviceCoordinator

_LOGGER = logging.getLogger(__name__)
DOMAIN = "thinclock"
PLATFORMS = ["sensor", "select", "number", "button", "event"]
CARD_URL = "/local/thinclock-card.js"


async def _register_lovelace_card(hass: HomeAssistant) -> None:
    """Persist the card JS in lovelace_resources storage so it survives reloads."""
    import hashlib
    from homeassistant.helpers.storage import Store
    store = Store(hass, 1, "lovelace_resources")
    data = await store.async_load() or {"items": []}
    items = data.setdefault("items", [])
    if not any(i.get("url") == CARD_URL for i in items):
        card_id = hashlib.md5(CARD_URL.encode()).hexdigest()
        items.append({"id": card_id, "url": CARD_URL, "type": "module"})
        await store.async_save(data)
        _LOGGER.info("[thinclock] registered Lovelace card resource")



async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    session = async_get_clientsession(hass)
    coordinator = ThinClockDeviceCoordinator(
        hass, session,
        server_url=entry.data["server_url"],
        device_ip=entry.data["device_ip"],
    )
    await coordinator.async_config_entry_first_refresh()
    hass.data.setdefault(DOMAIN, {})[entry.entry_id] = coordinator
    await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)

    # Register Lovelace card (once, not per entry)
    if not hass.data[DOMAIN].get('_card_registered'):
        await _register_lovelace_card(hass)
        hass.data[DOMAIN]['_card_registered'] = True

    @callback
    def _on_screen_changed(event) -> None:
        hass.async_create_task(coordinator.async_request_refresh())

    entry.async_on_unload(
        hass.bus.async_listen('thinclock_screen_changed', _on_screen_changed)
    )
    entry.async_on_unload(
        hass.bus.async_listen('thinclock_button', _on_screen_changed)
    )

    return True


async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    unloaded = await hass.config_entries.async_unload_platforms(entry, PLATFORMS)
    if unloaded:
        hass.data[DOMAIN].pop(entry.entry_id, None)
    return unloaded
