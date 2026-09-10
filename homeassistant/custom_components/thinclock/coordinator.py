"""DataUpdateCoordinator for ThinClock."""
from __future__ import annotations

import logging
from datetime import timedelta

import aiohttp
from homeassistant.core import HomeAssistant
from homeassistant.helpers.update_coordinator import DataUpdateCoordinator, UpdateFailed

_LOGGER = logging.getLogger(__name__)
SCAN_INTERVAL = timedelta(seconds=30)


class ThinClockDeviceCoordinator(DataUpdateCoordinator):
    """Polls the thinclock server for a specific device's data."""

    def __init__(self, hass: HomeAssistant, session: aiohttp.ClientSession, server_url: str, device_ip: str) -> None:
        self.session = session
        self.server_url = server_url.rstrip("/")
        self.device_ip = device_ip
        super().__init__(hass, _LOGGER, name=f"thinclock_{device_ip}", update_interval=SCAN_INTERVAL)

    async def _async_update_data(self) -> dict:
        try:
            async with self.session.get(f"http://{self.device_ip}/sensors", timeout=aiohttp.ClientTimeout(total=5)) as r:
                sensors = await r.json() if r.status == 200 else {}
            async with self.session.get(f"http://{self.device_ip}/info", timeout=aiohttp.ClientTimeout(total=5)) as r:
                info = await r.json() if r.status == 200 else {}
            async with self.session.get(f"http://{self.device_ip}/status", timeout=aiohttp.ClientTimeout(total=5)) as r:
                status = await r.json() if r.status == 200 else {}
            async with self.session.get(f"{self.server_url}/api/active", timeout=aiohttp.ClientTimeout(total=5)) as r:
                active = await r.json() if r.status == 200 else []
            async with self.session.get(f"{self.server_url}/api/config/", timeout=aiohttp.ClientTimeout(total=5)) as r:
                cfg = await r.json() if r.status == 200 else {}
                config_screens = cfg.get("screens", [])
        except Exception as e:
            raise UpdateFailed(f"Cannot reach thinclock server: {e}") from e

        return {"sensors": sensors, "info": info, "status": status, "active": active, "config_screens": config_screens}
