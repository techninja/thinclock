"""Config flow for ThinClock — manual entry + zeroconf auto-discovery."""
from __future__ import annotations

import aiohttp
import voluptuous as vol
from homeassistant import config_entries
from homeassistant.components import zeroconf
from homeassistant.helpers.aiohttp_client import async_get_clientsession

DOMAIN = "thinclock"


async def _validate(hass, url: str) -> dict:
    """Hit /api/device/info and return the parsed JSON, or raise."""
    session = async_get_clientsession(hass)
    async with session.get(f"{url}/api/device/info", timeout=aiohttp.ClientTimeout(total=5)) as r:
        if r.status != 200:
            raise ConnectionError
        return await r.json()


class ThinClockConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    VERSION = 1

    def __init__(self):
        self._url: str | None = None
        self._info: dict = {}

    # --- Manual entry ---
    async def async_step_user(self, user_input=None):
        errors = {}
        if user_input is not None:
            url = user_input["url"].rstrip("/")
            try:
                self._info = await _validate(self.hass, url)
                await self.async_set_unique_id(f"thinclock_{self._info.get('ip', url)}")
                self._abort_if_unique_id_configured()
                return self.async_create_entry(
                    title=f"ThinClock ({self._info.get('ip', url)})",
                    data={"url": url},
                )
            except Exception:
                errors["base"] = "cannot_connect"

        return self.async_show_form(
            step_id="user",
            data_schema=vol.Schema({vol.Required("url", default="http://thinclock.local:80"): str}),
            errors=errors,
        )

    # --- Zeroconf auto-discovery ---
    async def async_step_zeroconf(self, discovery_info: zeroconf.ZeroconfServiceInfo):
        """Called by HA when _thinclock._tcp.local. is found on the network."""
        host = discovery_info.host
        port = discovery_info.port or 80
        self._url = f"http://{host}:{port}"

        try:
            self._info = await _validate(self.hass, self._url)
        except Exception:
            return self.async_abort(reason="cannot_connect")

        ip = self._info.get("ip", host)
        await self.async_set_unique_id(f"thinclock_{ip}")
        self._abort_if_unique_id_configured()

        self.context["title_placeholders"] = {"ip": ip}
        return await self.async_step_zeroconf_confirm()

    async def async_step_zeroconf_confirm(self, user_input=None):
        """Ask user to confirm the discovered device."""
        if user_input is not None:
            return self.async_create_entry(
                title=f"ThinClock ({self._info.get('ip', self._url)})",
                data={"url": self._url},
            )
        return self.async_show_form(
            step_id="zeroconf_confirm",
            description_placeholders={
                "ip": self._info.get("ip", self._url),
                "version": self._info.get("version", "?"),
            },
        )
