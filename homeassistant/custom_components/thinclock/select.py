"""Select entity for ThinClock — current screen."""
from __future__ import annotations

import aiohttp
from homeassistant.components.select import SelectEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from . import DOMAIN
from .coordinator import ThinClockCoordinator
from .sensor import _device_info


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback) -> None:
    coordinator: ThinClockCoordinator = hass.data[DOMAIN][entry.entry_id]
    async_add_entities([ThinClockScreenSelect(coordinator, entry)])


class ThinClockScreenSelect(CoordinatorEntity, SelectEntity):
    _attr_has_entity_name = True
    _attr_name = "Current Screen"
    _attr_icon = "mdi:television-play"

    def __init__(self, coordinator: ThinClockCoordinator, entry: ConfigEntry) -> None:
        super().__init__(coordinator)
        self._url = entry.data["url"]
        self._attr_unique_id = f"{entry.entry_id}_screen"
        self._attr_device_info = _device_info(entry, coordinator)

    @property
    def options(self) -> list[str]:
        screens = self.coordinator.data.get("screens", []) if self.coordinator.data else []
        return [s["id"] for s in screens if isinstance(s, dict)]

    @property
    def current_option(self) -> str | None:
        active = self.coordinator.data.get("active", []) if self.coordinator.data else []
        return active[0]["id"] if active else None

    async def async_select_option(self, option: str) -> None:
        from homeassistant.helpers.aiohttp_client import async_get_clientsession
        session = async_get_clientsession(self.hass)
        async with session.post(f"{self._url}/api/screens/{option}/enable",
                                timeout=aiohttp.ClientTimeout(total=5)):
            pass
        await self.coordinator.async_request_refresh()
