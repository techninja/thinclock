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


def _register_lovelace_card(hass: HomeAssistant) -> None:
    """Register card JS with the frontend module loader."""
    from homeassistant.components.frontend import async_register_extra_js_url
    async_register_extra_js_url(hass, CARD_URL)
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
        _register_lovelace_card(hass)
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
