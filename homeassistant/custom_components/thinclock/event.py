"""Event entity for ThinClock button presses."""
from __future__ import annotations

from homeassistant.components.event import EventDeviceClass, EventEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from . import DOMAIN
from .sensor import _device_info

BUTTON_EVENT_MAP = {
    "ldr_cover": "gesture",
}


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback) -> None:
    coordinator = hass.data[DOMAIN][entry.entry_id]
    entity = ThinClockButtonEvent(entry, coordinator)
    async_add_entities([entity])

    @callback
    def _on_button(event) -> None:
        entity.fire(event.data.get("button", ""))

    entry.async_on_unload(
        hass.bus.async_listen("thinclock_button", _on_button)
    )


class ThinClockButtonEvent(EventEntity):
    _attr_has_entity_name = True
    _attr_name = "Button"
    _attr_device_class = EventDeviceClass.BUTTON
    _attr_event_types = ["left", "left_long", "right", "right_long", "select", "select_long", "gesture"]

    def __init__(self, entry, coordinator) -> None:
        self._attr_unique_id = f"{entry.entry_id}_button"
        self._attr_device_info = _device_info(entry, coordinator)

    def fire(self, button: str) -> None:
        mapped = BUTTON_EVENT_MAP.get(button, button)
        if mapped not in self._attr_event_types:
            return
        self._trigger_event(mapped)
        self.async_write_ha_state()
