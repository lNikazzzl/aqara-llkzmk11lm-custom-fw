# Custom firmware for the Aqara dual relay LLKZMK11LM

Open firmware for the Aqara Wireless Relay Controller (2 channels), model
LLKZMK11LM / `lumi.relay.c2acn01`, built on the NXP JN5169 Zigbee 3.0 SDK. It
presents itself to Zigbee2MQTT as the stock device and can be exchanged with the
factory firmware over the air in both directions.

> **Experimental.** The firmware has run on a bench board without the power
> stage; the OTA path between the factory and this firmware is not yet verified.
> Flashing by wire erases the factory firmware, and the chip is read-protected
> (CRP level 1), so the original **cannot be backed up from the chip**. Keep the
> factory OTA image from the Zigbee2MQTT OTA index. Not affiliated with Aqara,
> Lumi or NXP. You do this at your own risk.

## What works

- Zigbee 3.0 router, joins like the stock device (or keeps the stored network).
- Two On/Off endpoints driving the bistable relays, state restored after a power cut.
- Wall switch inputs S1/S2 and the on-board button (hold 5 s to leave the network).
- Zigbee2MQTT LLKZMK11LM definition: `interlock`, `power_outage_memory`,
  `power_outage_count`, `device_temperature`.
- OTA client compatible with the factory one (manufacturer `0x115F`, image type `0x0101`).

## What does not

- `power`, `energy`, `voltage`, `current`: the PL7211 power meter protocol is not
  known, so these stay empty.

## Updating from Zigbee2MQTT

Add the index to the Zigbee2MQTT `configuration.yaml` and restart it:

```yaml
ota:
  zigbee_ota_override_index_location: https://raw.githubusercontent.com/lNikazzzl/aqara-llkzmk11lm-custom-fw/main/ota/index.json
```

The firmware reports a deliberately low version, so Zigbee2MQTT keeps offering the
factory image as an update; use the OTA page, or the `update` request, to switch.
To return to the factory firmware, remove the index (or use the downgrade
request) and update from the official index.

## Building

Needs NXP BeyondStudio (JN-SW-4141), the JN-SW-4170 SDK and the JN-AN-1218
example application, from nxp.com (free account). See `firmware/README.md`.

```
python tools/make_zpscfg.py <JN-AN-1218>/Common_Light/Source/app_gp_notl.zpscfg firmware/Source/app.zpscfg
firmware/build.sh
```

Output: `firmware/Build/AqaraRelay_JN5169.ota` (Zigbee2MQTT) and
`..._flash.bin` (UART flasher, `tools/jn_flash.py`).
`tools/ota/make_index.py` writes the index entry for a new `.ota`.

## Layout

| Path | |
|---|---|
| `firmware/` | the firmware, `hw.h` has the pin map |
| `tools/` | flasher, EEPROM dump and restore, OTA image and index tools |
| `ota/` | released `.ota` files and `index.json` |
| `CLAUDE.md` | working notes: hardware facts, decisions, what is and is not done |

Hardware reference: schematics of the module boards at
<http://re-engr.ru/aqara_relay_mcu/> and <http://re-engr.ru/aqara_relay_pwr/>.

## Licence

The code written for this project is MIT licensed. Parts derived from NXP software
carry the NXP licence, which allows use on NXP microcontrollers only; see `NOTICE.md`.
