"""Config flow for ThinClock integration."""
from __future__ import annotations

import aiohttp
import voluptuous as vol
from homeassistant import config_entries
from homeassistant.const import CONF_HOST, CONF_PORT

DOMAIN = "thinclock"


class ThinClockConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    """Handle a config flow for ThinClock."""

    VERSION = 1

    async def async_step_user(self, user_input=None):
        """Handle the initial step — user enters device IP."""
        errors = {}

        if user_input is not None:
            host = user_input[CONF_HOST]
            port = user_input.get(CONF_PORT, 80)

            # Validate connection
            try:
                async with aiohttp.ClientSession() as session:
                    async with session.get(
                        f"http://{host}:{port}/status", timeout=aiohttp.ClientTimeout(total=5)
                    ) as resp:
                        if resp.status == 200:
                            data = await resp.json()
                            await self.async_set_unique_id(f"thinclock_{host}")
                            self._abort_if_unique_id_configured()
                            return self.async_create_entry(
                                title=f"ThinClock ({host})",
                                data={CONF_HOST: host, CONF_PORT: port},
                            )
                        errors["base"] = "cannot_connect"
            except Exception:
                errors["base"] = "cannot_connect"

        return self.async_show_form(
            step_id="user",
            data_schema=vol.Schema({
                vol.Required(CONF_HOST): str,
                vol.Optional(CONF_PORT, default=80): int,
            }),
            errors=errors,
        )
