"""Sensor entities for ThinClock — temperature, humidity, light."""
from __future__ import annotations

from homeassistant.components.sensor import SensorEntity, SensorDeviceClass, SensorStateClass
from homeassistant.const import UnitOfTemperature, PERCENTAGE, LIGHT_LUX
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from . import DOMAIN
from .coordinator import ThinClockCoordinator

SENSORS = [
    ("temperature", "Temperature", SensorDeviceClass.TEMPERATURE, UnitOfTemperature.FAHRENHEIT, "sensors"),
    ("humidity",    "Humidity",    SensorDeviceClass.HUMIDITY,    PERCENTAGE,                   "sensors"),
    ("light",       "Light",       SensorDeviceClass.ILLUMINANCE, LIGHT_LUX,                    "sensors"),
]


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback) -> None:
    coordinator: ThinClockCoordinator = hass.data[DOMAIN][entry.entry_id]
    async_add_entities(ThinClockSensor(coordinator, entry, key, name, device_class, unit, data_key)
                       for key, name, device_class, unit, data_key in SENSORS)


class ThinClockSensor(CoordinatorEntity, SensorEntity):
    _attr_state_class = SensorStateClass.MEASUREMENT
    _attr_has_entity_name = True

    def __init__(self, coordinator, entry, key, name, device_class, unit, data_key):
        super().__init__(coordinator)
        self._key = key
        self._data_key = data_key
        self._attr_name = name
        self._attr_device_class = device_class
        self._attr_native_unit_of_measurement = unit
        self._attr_unique_id = f"{entry.entry_id}_{key}"
        self._attr_device_info = _device_info(entry, coordinator)

    @property
    def native_value(self):
        return self.coordinator.data.get(self._data_key, {}).get(self._key)


def _device_info(entry, coordinator):
    info = coordinator.data.get("info", {}) if coordinator.data else {}
    return {
        "identifiers": {(DOMAIN, entry.entry_id)},
        "name": "ThinClock",
        "manufacturer": "thinclock",
        "model": info.get("chip", "ESP32"),
        "sw_version": info.get("version"),
        "configuration_url": entry.data["url"],
    }
