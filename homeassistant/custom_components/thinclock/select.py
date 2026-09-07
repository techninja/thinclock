"""Select entity for ThinClock — current screen."""
from __future__ import annotations

import aiohttp
from homeassistant.components.select import SelectEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from . import DOMAIN
from .coordinator import ThinClockDeviceCoordinator
from .sensor import _device_info


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback) -> None:
    coordinator: ThinClockDeviceCoordinator = hass.data[DOMAIN][entry.entry_id]
    async_add_entities([ThinClockScreenSelect(coordinator, entry)])


class ThinClockScreenSelect(CoordinatorEntity, SelectEntity):
    _attr_has_entity_name = True
    _attr_name = "Current Screen"
    _attr_icon = "mdi:television-play"

    def __init__(self, coordinator: ThinClockDeviceCoordinator, entry: ConfigEntry) -> None:
        super().__init__(coordinator)
        self._url = entry.data["server_url"]
        self._attr_unique_id = f"{entry.entry_id}_screen"
        self._attr_device_info = _device_info(entry, coordinator)

    @property
    def entity_picture(self) -> str | None:
        data = self.coordinator.data or {}
        status = data.get("status", {}) or {}
        screen_id = status.get("screen_id", "")
        if screen_id:
            return f"{self._url}/api/preview/{screen_id}.gif"
        return None

    @property
    def options(self) -> list[str]:
        screens = self.coordinator.data.get("config_screens", []) if self.coordinator.data else []
        return [s["name"] for s in screens if isinstance(s, dict) and s.get("name")]

    @property
    def current_option(self) -> str | None:
        data = self.coordinator.data or {}
        status = data.get("status", {}) or {}
        screens = data.get("config_screens", [])
        screen_id = status.get("screen_id", "")
        if screen_id:
            match = next((s for s in screens if s.get("id") == screen_id), None)
            if match:
                return match.get("name")
        idx = status.get("screen", 0)
        if screens and 0 <= idx < len(screens):
            return screens[idx].get("name")
        return None

    async def async_select_option(self, option: str) -> None:
        from homeassistant.helpers.aiohttp_client import async_get_clientsession
        screens = self.coordinator.data.get("config_screens", []) if self.coordinator.data else []
        match = next((s for s in screens if s.get("name") == option), None)
        if not match:
            return
        ip = (self.coordinator.data.get("info", {}) or {}).get("ip")
        if not ip:
            return
        session = async_get_clientsession(self.hass)
        async with session.post(f"http://{ip}/screen",
                                json={"index": screens.index(match)},
                                timeout=aiohttp.ClientTimeout(total=5)):
            pass
        await self.coordinator.async_request_refresh()
