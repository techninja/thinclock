"""Sensor entities for ThinClock — temperature, humidity, light."""
from __future__ import annotations

from homeassistant.components.sensor import SensorDeviceClass, SensorEntity, SensorStateClass
from homeassistant.config_entries import ConfigEntry
from homeassistant.const import LIGHT_LUX, PERCENTAGE, UnitOfTemperature
from homeassistant.core import HomeAssistant
from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from . import DOMAIN
from .coordinator import ThinClockDeviceCoordinator

SENSORS = [
    ("temperature", "Temperature", SensorDeviceClass.TEMPERATURE, UnitOfTemperature.FAHRENHEIT, "sensors", SensorStateClass.MEASUREMENT),
    ("humidity",    "Humidity",    SensorDeviceClass.HUMIDITY,    PERCENTAGE,                   "sensors", SensorStateClass.MEASUREMENT),
    ("light",       "Light",       SensorDeviceClass.ILLUMINANCE, LIGHT_LUX,                    "sensors", SensorStateClass.MEASUREMENT),
]


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback) -> None:
    coordinator: ThinClockDeviceCoordinator = hass.data[DOMAIN][entry.entry_id]
    async_add_entities(ThinClockSensor(coordinator, entry, key, name, device_class, unit, data_key, state_class)
                       for key, name, device_class, unit, data_key, state_class in SENSORS)


class ThinClockSensor(CoordinatorEntity, SensorEntity):
    _attr_has_entity_name = True

    def __init__(self, coordinator, entry, key, name, device_class, unit, data_key, state_class):
        super().__init__(coordinator)
        self._key = key
        self._data_key = data_key
        self._attr_name = name
        self._attr_device_class = device_class
        self._attr_native_unit_of_measurement = unit
        self._attr_state_class = state_class
        self._attr_unique_id = f"{entry.entry_id}_{key}"
        self._attr_device_info = _device_info(entry, coordinator)

    @property
    def native_value(self):
        val = self.coordinator.data.get(self._data_key, {}).get(self._key)
        return val if val else None


def _device_info(entry, coordinator) -> DeviceInfo:
    info = coordinator.data.get("info", {}) if coordinator.data else {}
    return DeviceInfo(
        identifiers={(DOMAIN, entry.entry_id)},
        name=f"ThinClock ({entry.data.get('device_ip', '?')})",
        manufacturer="ThinClock",
        model=entry.data.get("chip", info.get("chip", "ESP32")),
        sw_version=entry.data.get("version") or info.get("version"),
        configuration_url=entry.data.get("external_url") or f"http://{entry.data.get('device_ip')}",
    )
