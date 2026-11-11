# Aqara LLKZMK11LM custom firmware

Replacement firmware for the Aqara dual relay module LLKZMK11LM (`lumi.relay.c2acn01`),
MCU NXP JN5169. Goal: our firmware and the factory firmware can replace each other
over the air (Zigbee OTA from Zigbee2MQTT), and Zigbee2MQTT keeps seeing an ordinary
LLKZMK11LM — only the firmware version differs.

## Hardware facts

- MCU module LM16-DCC-A-T1 (`aqara_relay_mcu_sch1.pdf`), power board LM16-DCC-B-T2
  (schematic: http://re-engr.ru/aqara_relay_pwr/). Pin map in `firmware/Source/hw.h`.
- Bistable relays ADW1203HL, two coils each: K1 on/off DIO10/DIO8, K2 on/off DIO11/DIO9,
  active-high 50 ms pulse. Mapping is from the schematic, not yet confirmed by ear.
- Inputs: button DIO17, wall switches S1/S2 DIO15/DIO14 (dry contact, pull-ups on the power board).
- LEDs: blue DIO1, red DIO2 (active low). UART0 on DIO6/7 = PROG connector.
- PL7211 power meter on SPI (DO0/DIO18/DIO19/DO1, reset DIO3). Protocol unknown.
- Chip is at **CRP level 1** (customer settings 0x01001510 = 0xDE): UART bootloader can
  erase and write flash but never read it; RAM write/run are blocked too. No encryption.
  JTAG disabled.

## Toolchain quirks

- BeyondStudio + JN-SW-4170 build 1840 in `C:\NXP\bstudio_nxp`. Build with `firmware/build.sh`.
- msys make 3.81: SDK path must be relative (vpath ignores `/c/...`), no `-j`,
  `.d` rule must `touch $@`, generated `gen/` sources need explicit pattern rules.
- `version.h` collides with the SDK's `ZCIF/Include/version.h`; ours is `app_version.h`.
- `tools/make_zpscfg.py` derives `firmware/Source/app.zpscfg` from the JN-AN-1218 example.
  Stack GroupTable size must be >= `CLD_GROUPS_MAX_NUMBER_OF_GROUPS` or endpoint registration fails.

## Flashing and backups

- Bootloader: hold BOOT (DO1 to GND) while powering up. `tools/jn_flash.py COM3 info|flash`.
  Erasing needs `--erase-ok`; always ask the user before an erase.
- `stock_firmware/`: factory OTA image v47 (Lumi 0x115F, image type 0x0101, header string
  `DR1175r1v1UNENCRYPTED00000JN5169`, embedded OTA header at image offset 0x54) and the
  same image prepared for the UART flasher.
- `eeprom_backup.bin`: full EEPROM (255 x 64 B) taken after the factory firmware was erased.
  **Contains the Zigbee network key — never publish it.** Factory data uses segments 0–127,
  128–254 were empty. `index_sector_0x01001400.bin`: MAC and chip settings.
- `tools/eeprom_dump/` + `tools/eeprom_capture.py`: read-only EEPROM dumper firmware.

## Done

- Toolchain verified; NXP example builds.
- Factory firmware erased; EEPROM backed up; our firmware flashed by wire.
- Firmware v0.1.0: Zigbee 3.0 router, OnOff on EP1/EP2, relay pulse driver, button
  (short = toggle L1 / start pairing, 5 s = leave + reset), S1/S2 inputs, LEDs, state
  restore after power cut. It inherited the factory network data from EEPROM and is
  controllable from Zigbee2MQTT (verified on the bench without the power board).

## Stage 1 (code written, not yet tested on the board): interchangeable with the factory firmware

Firmware 0.2.0 implements the list below. Build outputs in `firmware/Build/`:
`AqaraRelay_JN5169_flash.bin` (UART flasher, embedded OTA version 1) and
`AqaraRelay_JN5169.ota` (OTA file version 0x01000200). Image layout checked against the
factory image: MAC at 0x40, OTA header at 0x50, link key `ZigBeeAlliance09` at 0xA0.
Open question for the test: the factory image has the same version outside and inside
(47/47), ours differs (0x01000200/1); unknown whether the factory client checks that.
`tools/eeprom_dump` built with `make RESTORE=1` + `tools/eeprom_restore.py` writes
`eeprom_backup.bin` back (also saves the current EEPROM first).

Factory interview data (Z2M database): EP1 dev 257 in [0,3,4,5,1,2,10,6,16,0x0B04,12]
out [25,10]; EP2 dev 257 in [6,16,4,5]; Basic appVersion 1, stackVersion 2, hwVersion 19,
dateCode 09-20-2018; node descriptor manufacturer 4151 (0x1037); no configured reportings.

- Report Basic manufacturer `LUMI`, model `lumi.relay.c2acn01` so Z2M uses its LLKZMK11LM definition.
- OTA client matching the factory one (manufacturer 0x115F, image type 0x0101, same header
  string, internal flash storage); OTA file built with JET with a version above 47.
- Version policy (user decision): the running firmware reports a **lowered** current file
  version so Z2M offers the factory image; "update available" stays visible. Custom images
  are offered via `ota.zigbee_ota_override_index_location` or a per-request `url`.
- EEPROM shared with the factory firmware: no PDM wipe, PDM on segments 0–126 (segment 127 is non-PDM factory data),
  our own data outside PDM in segments 192–207 (app_store.c), stack table sizes matching the
  factory ones. Restore `eeprom_backup.bin` before testing (needs a small restore firmware).
- Lumi attributes Z2M uses: interlock (genBinaryOutput 0xFF06), power_outage_memory
  (genBasic 0xFFF0 magic payloads), heartbeat genBasic 0xFF01 with device_temperature (tag 3,
  internal sensor) and power_outage_count (tag 5).

## Will not do

- Power metering: `power`, `energy`, `voltage`, `current` (needs the PL7211 protocol).
  These stay empty in Z2M with our firmware.
- Any other Aqara-specific feature not listed in stage 1.
