"""Number entity for ThinClock — brightness."""
from __future__ import annotations

import aiohttp
from homeassistant.components.number import NumberEntity, NumberMode
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from . import DOMAIN
from .coordinator import ThinClockCoordinator
from .sensor import _device_info


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback) -> None:
    coordinator: ThinClockCoordinator = hass.data[DOMAIN][entry.entry_id]
    async_add_entities([ThinClockBrightness(coordinator, entry)])


class ThinClockBrightness(CoordinatorEntity, NumberEntity):
    _attr_has_entity_name = True
    _attr_name = "Brightness"
    _attr_icon = "mdi:brightness-6"
    _attr_native_min_value = 0
    _attr_native_max_value = 100
    _attr_native_step = 1
    _attr_mode = NumberMode.SLIDER

    def __init__(self, coordinator: ThinClockCoordinator, entry: ConfigEntry) -> None:
        super().__init__(coordinator)
        self._url = entry.data["url"]
        self._attr_unique_id = f"{entry.entry_id}_brightness"
        self._attr_device_info = _device_info(entry, coordinator)

    @property
    def native_value(self) -> float | None:
        info = self.coordinator.data.get("info", {}) if self.coordinator.data else {}
        return info.get("brightness")

    async def async_set_native_value(self, value: float) -> None:
        from homeassistant.helpers.aiohttp_client import async_get_clientsession
        session = async_get_clientsession(self.hass)
        async with session.post(
            f"{self._url}/api/device/display",
            json={"brightness": int(value)},
            timeout=aiohttp.ClientTimeout(total=5),
        ):
            pass
        await self.coordinator.async_request_refresh()
