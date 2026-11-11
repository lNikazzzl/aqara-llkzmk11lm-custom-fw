# Aqara LLKZMK11LM custom firmware

Replacement firmware for the Aqara dual relay module LLKZMK11LM (`lumi.relay.c2acn01`),
MCU NXP JN5169. Goal: our firmware and the factory firmware replace each other over the
air (Zigbee OTA from Zigbee2MQTT), and Zigbee2MQTT keeps seeing an ordinary LLKZMK11LM;
only the firmware version differs. Current release: **0.2.4**.

## Safety

- **The power board is not isolated: terminal L (J4) is wired straight to GND.** On mains
  the whole low-voltage side (MCU module, UART, S1/S2/COM) sits at line potential. Never
  connect the USB-UART adapter (or anything earthed) while the board is on mains.
- Bench tests on the power board: 15–24 V DC, **plus to N, minus to L** (re-engr.ru did
  24 V). Then GND = DC minus and the adapter is safe; connect only GND/TX/RX, not its 3.3 V.
- The MCU module alone runs from the adapter's 3.3 V (no relays, S1/S2 float: internal
  pull-ups are enabled for that).

## Hardware facts

- MCU module LM16-DCC-A-T1 (`aqara_relay_mcu_sch1.pdf`), power board LM16-DCC-B-T2
  (`aqara_relay_sch_pwr_v1.pdf`, http://re-engr.ru/aqara_relay_pwr/). Pin map in `firmware/Source/hw.h`.
- Bistable relays ADW1203HL, two coils each: K1 on/off DIO10/DIO8, K2 on/off DIO11/DIO9,
  2N7002 low-side, active-high 50 ms pulse. Mapping from the schematic labels, **not yet
  checked on the power board** (which relay is L1, on vs off). Coil supply rail not traced.
- Inputs: button DIO17, wall switches S1/S2 DIO15/DIO14 (dry contact to COM, 10k pull-ups
  on the power board, active low). LEDs: blue DIO1, red DIO2 (active low).
- UART0 on DIO6/7 = PROG connector (pin 5 RESET, pin 6 GND), 115200.
- Power supply: non-isolated buck (U2 SC1124) from mains, TPS76333 3.3 V.
- PL7211 power meter on SPI (DO0/DIO18/DIO19/DO1, reset DIO3 with pull-up). Protocol unknown,
  our firmware leaves its pins alone.
- Chip is at **CRP level 1** (customer settings 0x01001510 = 0xDE): the UART bootloader can
  erase and write flash but never read it; RAM write/run are blocked. No encryption, JTAG off.
- MAC comes from the chip's index sector (00:15:8d:00:0a:97:3c:a9 on this board), not from the image.

## Toolchain quirks

- BeyondStudio + JN-SW-4170 build 1840 in `C:\NXP\bstudio_nxp`. Build with `firmware/build.sh`
  (`DEBUG=1` is the default; `DEBUG=0` drops the verbose log).
- msys make 3.81: SDK path must be relative (vpath ignores `/c/...`), no `-j`,
  `.d` rule must `touch $@`, generated `gen/` sources need explicit pattern rules.
- `version.h` collides with the SDK's `ZCIF/Include/version.h`; ours is `app_version.h`.
- `tools/make_zpscfg.py` derives `firmware/Source/app.zpscfg` from the JN-AN-1218 example
  (endpoints/clusters of the factory firmware). Stack GroupTable size must be >=
  `CLD_GROUPS_MAX_NUMBER_OF_GROUPS` or endpoint registration fails.
- `--gc-sections` drops unreferenced `.ro_mac_address`; `app_ota.c` references
  `au8MacAddress` and the Makefile fails the build unless MAC/OTA header/link key are at
  0x40/0x50/0xA0 (where JET writes them and the factory OTA client checks the key).
- `OTA_TIME_INTERVAL_BETWEEN_REQUESTS` must stay undefined: it adds 10–20 s per 48-byte block.
- Releases: bump `APP_VERSION_STRING` (app_version.h) and `OTA_FILE_VERSION` (Makefile,
  0x0100MMmm); `OTA_REPORTED_VERSION` stays 1.

## User decisions

- The verbose log stays in production images.
- The running firmware reports a lowered OTA version (1), so Z2M always shows an update
  (to the factory image); our files carry 0x0100MMmm. Index windows, see below.
- Every firmware switch erases the Zigbee stack records and needs pairing again.
- Only stage 1 (no power metering).
- Code published on GitHub; ask before an erase; pushes of releases have been done right away.

## Firmware design (0.2.4)

- Basic: `LUMI` / `lumi.relay.c2acn01`, appVersion 1, stackVersion 2, hwVersion 19,
  SW build `custom-<version>`. Endpoints as the factory interview: EP1 dev 257 in
  [0,3,4,5,1,2,10,6,16,0x0B04,12] out [25,10]; EP2 dev 257 in [6,16,4,5]. Node descriptor
  manufacturer 4151 (0x1037). Z2M configures no reportings: we report OnOff ourselves.
- Lumi protocol (lumi.c, on raw APS before the ZCL): interlock genBinaryOutput 0xFF06,
  power_outage_memory genBasic 0xFFF0 magic payloads, heartbeat genBasic 0xFF01 every 50 min
  with tags 3 (chip temperature), 5 (power-on count), 100/101 (relays).
- OTA client (app_ota.c, from JN-AN-1218): manufacturer 0x115F, image type 0x0101, header
  string `DR1175r1v1UNENCRYPTED00000JN5169`, server = coordinator, internal storage from
  sector 8, refuses its own image. The factory client accepts our files (outer and embedded
  versions may differ).
- EEPROM: PDM on **63 segments** (0..62). Our settings outside the PDM in segments 192–207
  (app_store.c, round robin with CRC). Segment 127 is factory non-PDM data, never touched.
- PDM segment header: bytes 0-1 = record id. 0xF000.. (APL) and 0xF100.. (NWK, 0xF106
  frame counter bitmap) are the stack in **both** firmwares, but in different formats
  (other SDK build: e.g. a NIB field at 0x2c0 vs 0x2c8). 0x0005/09/0A/0B/0C/1F/21/24/25 are
  Lumi records (0x0024 calibration-like floats). 0xA102 is our OTA state.
- nvm_clean.c: raw-erases the stack (0xF000–0xF1FF) and 0xA1xx segments, Lumi records kept.
  Custom → factory: at `E_CLD_OTA_INTERNAL_COMMAND_RESET_TO_UPGRADE` if the downloaded
  image's embedded version != `APP_OTA_REPORTED_VERSION`. Factory → custom: at start, if the
  CRC of the Lumi record segments differs from the one in app_store (the factory firmware
  rewrites its records on every start, we never write them; first start counts as changed).
  Do not detect by ids 0xF000–0xF003: our stack writes them too.

## Tools and files

- `tools/jn_flash.py COM3 info|flash <bin> --erase-ok` — UART bootloader (BOOT = DO1 to GND at power-up).
- `tools/uart_logger.py COM3 logs/uart.log` — runs in the background, timestamps lines,
  reopens the port. `touch logs/uart.log.pause` releases the port (flasher, EEPROM tools),
  remove the file to resume. Boot lines are lost when the adapter itself is replugged
  (the board starts before the port opens): power-cycle only the board, or flash (the
  bootloader reset keeps the port).
- `tools/eeprom_dump` (`make` read-only dumper, `make RESTORE=1` embeds `eeprom_backup.bin`),
  `tools/eeprom_capture.py`, `tools/eeprom_restore.py COM3 eeprom_backup.bin <save-before>`.
- `tools/ota/make_ota.sh` (JET, called by the Makefile), `tools/ota/make_index.py`.
- `stock_firmware/`: factory OTA v47 (182206 B, same as the official zigbee-OTA file) and
  the image for the UART flasher. Factory layout: MAC 0x40, OTA header 0x50, key 0xA0
  (image offsets; +4 in the flasher file).
- EEPROM snapshots (all contain the **network key — never publish**): `eeprom_backup.bin`
  (factory state, CRC 47203fff), `eeprom_before_restore.bin` (after 0.1.0),
  `eeprom_before_restore_021*.bin` (after 0.2.1), `eeprom_after_stock_crash.bin`.
  `index_sector_0x01001400.bin`: MAC and chip settings.
- `re/`: disassembly work (`stock.img`, `mine.dis`, library listings).
- The factory firmware prints on the same UART (`Lumi_NVM_InitData`, and a stack dump on a crash).

## Publishing

- GitHub `lNikazzzl/aqara-llkzmk11lm-custom-fw` (clone in `..\aqara-llkzmk11lm-custom-fw`),
  SSH with the user's main key. Commits as `lNikazzzl <lNikazzzl@users.noreply.github.com>`.
- Never published: EEPROM dumps, index sector, factory firmware, the NXP SDK/example,
  `app.zpscfg` (.gitignore). Files derived from NXP code carry the NXP notice (NOTICE.md);
  re-add it when copying them into the repo. Own code marked MIT (user has not confirmed
  the licence choice).
- Release: copy sources, put the `.ota` into `ota/`, regenerate `ota/index.json` with
  `make_index.py` (custom entry min 2 / max 16777727; factory entry = official URL, min 0 / max 1).
- Zigbee2MQTT (Home Assistant add-on): `ota.zigbee_ota_override_index_location:
  https://raw.githubusercontent.com/lNikazzzl/aqara-llkzmk11lm-custom-fw/main/ota/index.json`.
  Override entries match first; Z2M answers the device's own periodic query with 0x98.

## Test log

- Factory v47 by wire on the restored EEPROM: rejoined the user's network.
- Factory → custom 0.2.0 over the air worked; 0.2.0 had the gc-sections layout bug (OTA
  client dead), removed. 0.2.1/0.2.2 with 127 PDM segments picked up stale stack records
  (on the network for the firmware, nothing reached the coordinator) → back to 63, re-paired.
- 0.2.3: custom → factory over the air took 18 min, then the factory firmware crashed in
  `ZPS_u64NwkNibGetMappedIeeeAddr` (found by disassembling the factory image) on our stack
  records. The factory PDM writes beyond segment 62 (it used 73..106).
- 0.2.4: first start erased 81 stack segments, paired; a reboot kept the network; custom →
  factory over the air (16 min) erased 57 segments and the factory firmware started
  without crashing.
- On this board Lumi records 0x000C and 0x001F are gone (overwritten while the factory
  firmware was crashing); they are in `eeprom_backup.bin`.

## Open

- Factory firmware after the switch: pairing and relay control from Z2M not confirmed yet.
- Factory → custom 0.2.4 over the air (with the factory client's own block timing).
- Power board on 24 V DC: relay clicks, L1/L2 and on/off mapping, pulse length.
- Optional speed-up: Z2M `ota.image_block_response_delay` / `default_maximum_data_size`
  (from memory, unverified) and a 64-byte block in our client.
- Ideas: LED flash after rejoin; no relay pulse at boot when the state did not change.

## Will not do

- Power metering: `power`, `energy`, `voltage`, `current` (needs the PL7211 protocol).
  These stay empty in Z2M with our firmware.
- Any other Aqara-specific feature.
