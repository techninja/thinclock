"""DataUpdateCoordinator for ThinClock."""
from __future__ import annotations

import logging
from datetime import timedelta

import aiohttp
from homeassistant.core import HomeAssistant
from homeassistant.helpers.update_coordinator import DataUpdateCoordinator, UpdateFailed

_LOGGER = logging.getLogger(__name__)
SCAN_INTERVAL = timedelta(seconds=30)


class ThinClockCoordinator(DataUpdateCoordinator):
    """Polls the thinclock server for device + sensor data."""

    def __init__(self, hass: HomeAssistant, session: aiohttp.ClientSession, url: str) -> None:
        self.session = session
        self.url = url.rstrip("/")
        super().__init__(hass, _LOGGER, name="thinclock", update_interval=SCAN_INTERVAL)

    async def _async_update_data(self) -> dict:
        try:
            async with self.session.get(f"{self.url}/api/device/info", timeout=aiohttp.ClientTimeout(total=5)) as r:
                info = await r.json() if r.status == 200 else {}
            async with self.session.get(f"{self.url}/api/device/sensors", timeout=aiohttp.ClientTimeout(total=5)) as r:
                sensors = await r.json() if r.status == 200 else {}
            async with self.session.get(f"{self.url}/api/active", timeout=aiohttp.ClientTimeout(total=5)) as r:
                active = await r.json() if r.status == 200 else []
            async with self.session.get(f"{self.url}/api/screens", timeout=aiohttp.ClientTimeout(total=5)) as r:
                screens = await r.json() if r.status == 200 else []
        except Exception as e:
            raise UpdateFailed(f"Cannot reach thinclock server: {e}") from e

        return {"info": info, "sensors": sensors, "active": active, "screens": screens}
