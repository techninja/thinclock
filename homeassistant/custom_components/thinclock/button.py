"""Button entities for ThinClock — left, middle, right physical buttons."""
from __future__ import annotations

import aiohttp
from homeassistant.components.button import ButtonEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.aiohttp_client import async_get_clientsession
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from . import DOMAIN
from .coordinator import ThinClockCoordinator
from .sensor import _device_info

BUTTONS = [
    ("left",   "Button Left",   "mdi:arrow-left-circle",  "left"),
    ("middle", "Button Middle", "mdi:circle",             "select"),
    ("right",  "Button Right",  "mdi:arrow-right-circle", "right"),
]


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback) -> None:
    coordinator: ThinClockCoordinator = hass.data[DOMAIN][entry.entry_id]
    async_add_entities(ThinClockButton(coordinator, entry, key, name, icon, event)
                       for key, name, icon, event in BUTTONS)


class ThinClockButton(CoordinatorEntity, ButtonEntity):
    _attr_has_entity_name = True

    def __init__(self, coordinator, entry, key, name, icon, event):
        super().__init__(coordinator)
        self._url = entry.data["url"]
        self._event = event
        self._attr_name = name
        self._attr_icon = icon
        self._attr_unique_id = f"{entry.entry_id}_btn_{key}"
        self._attr_device_info = _device_info(entry, coordinator)

    async def async_press(self) -> None:
        session = async_get_clientsession(self.hass)
        async with session.post(
            f"{self._url}/api/event",
            json={"event": self._event, "screen": 0},
            timeout=aiohttp.ClientTimeout(total=5),
        ):
            pass
