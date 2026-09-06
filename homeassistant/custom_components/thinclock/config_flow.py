"""Config flow for ThinClock — per-device setup via zeroconf."""
from __future__ import annotations

import aiohttp
import voluptuous as vol
from homeassistant import config_entries
from homeassistant.components import zeroconf
from homeassistant.helpers.aiohttp_client import async_get_clientsession

DOMAIN = "thinclock"
SERVER_INTERNAL = "http://local-thinclock:3232"  # reachable from add-on only, used to get real IP


async def _get_device_info(hass, ip: str) -> dict:
    session = async_get_clientsession(hass)
    async with session.get(f"http://{ip}/info", timeout=aiohttp.ClientTimeout(total=5)) as r:
        if r.status != 200:
            raise ConnectionError
        return await r.json()


async def _get_server_url(hass) -> str:
    """Read the real server URL written by the add-on to the HA config volume."""
    try:
        path = hass.config.path(".thinclock_server")
        url = await hass.async_add_executor_job(lambda: open(path).read().strip())
        if url:
            return url
    except Exception:
        pass
    return SERVER_INTERNAL


async def _approve_device(hass, server_url: str, ip: str, name: str) -> None:
    session = async_get_clientsession(hass)
    async with session.post(
        f"{server_url}/api/devices/{ip}/approve",
        json={"name": name},
        timeout=aiohttp.ClientTimeout(total=5),
    ) as r:
        if r.status != 200:
            raise ConnectionError


async def _push_config_url(hass, device_ip: str, server_url: str) -> None:
    """Push the config URL directly to the device so it can start fetching."""
    session = async_get_clientsession(hass)
    config_url = f"{server_url}/api/config"
    async with session.post(
        f"http://{device_ip}/config_url",
        json={"config_url": config_url},
        timeout=aiohttp.ClientTimeout(total=5),
    ) as r:
        if r.status != 200:
            raise ConnectionError


class ThinClockConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    VERSION = 1

    def __init__(self):
        self._device_ip: str | None = None
        self._device_info: dict = {}

    # --- Manual entry ---
    async def async_step_user(self, user_input=None):
        errors = {}
        if user_input is not None:
            ip = user_input["ip"].strip()
            try:
                self._device_info = await _get_device_info(self.hass, ip)
                self._device_ip = ip
                await self.async_set_unique_id(f"thinclock_{ip}")
                self._abort_if_unique_id_configured()
                return await self.async_step_confirm()
            except Exception:
                errors["base"] = "cannot_connect"

        return self.async_show_form(
            step_id="user",
            data_schema=vol.Schema({vol.Required("ip"): str}),
            errors=errors,
        )

    # --- Zeroconf: device discovered (_thinclock._tcp) ---
    async def async_step_zeroconf(self, discovery_info: zeroconf.ZeroconfServiceInfo):
        if discovery_info.type != "_thinclock._tcp.local.":
            return self.async_abort(reason="not_supported")

        ip = discovery_info.host
        try:
            self._device_info = await _get_device_info(self.hass, ip)
        except Exception:
            return self.async_abort(reason="cannot_connect")

        self._device_ip = ip
        await self.async_set_unique_id(f"thinclock_{ip}")
        self._abort_if_unique_id_configured()

        self.context["title_placeholders"] = {
            "ip": ip,
            "chip": self._device_info.get("chip", "ESP32"),
        }
        return await self.async_step_confirm()

    async def async_step_confirm(self, user_input=None):
        if user_input is not None:
            name = f"ThinClock ({self._device_ip})"
            server_url = await _get_server_url(self.hass)
            try:
                await _approve_device(self.hass, server_url, self._device_ip, name)
            except Exception:
                pass
            try:
                await _push_config_url(self.hass, self._device_ip, server_url)
            except Exception:
                pass
            return self.async_create_entry(
                title=name,
                data={
                    "device_ip": self._device_ip,
                    "server_url": server_url,
                    "chip": self._device_info.get("chip", "ESP32"),
                    "version": self._device_info.get("version", ""),
                },
            )
        return self.async_show_form(
            step_id="confirm",
            description_placeholders={
                "ip": self._device_ip,
                "chip": self._device_info.get("chip", "ESP32"),
                "version": self._device_info.get("version", "?"),
            },
        )
