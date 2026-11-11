# Custom firmware for Aqara LLKZMK11LM (JN5169)

Zigbee 3.0 router with two On/Off endpoints, one per relay. Built on the NXP
JN-SW-4170 SDK (build 1840) using the start-up code of the JN-AN-1218 example.

## Build

Needs BeyondStudio (JN-SW-4141) and the JN-SW-4170 SDK in `C:\NXP\bstudio_nxp`, and
the JN-AN-1218 example from nxp.com, from which the stack configuration is derived once:

    python ../tools/make_zpscfg.py <JN-AN-1218>/Common_Light/Source/app_gp_notl.zpscfg Source/app.zpscfg

The Makefile expects the repository at the same depth as the SDK path in
`SDK_BASE_DIR` (relative, because msys make 3.81 ignores `/c/...` in `vpath`);
override it on the command line otherwise.

    ./build.sh              # Build/AqaraRelay_JN5169_flash.bin and Build/AqaraRelay_JN5169.ota
    ./build.sh DEBUG=1      # verbose log of every module
    ./build.sh INPUT_ROCKER_MODE=0   # wall switch inputs as push buttons
    ./build.sh clean

## Flash

Hold BOOT (DO1) low while powering the board, then:

    python ../tools/jn_flash.py COM3 info
    python ../tools/jn_flash.py COM3 flash Build/AqaraRelay_JN5169_flash.bin --erase-ok

The chip is at CRP level 1: flash can be erased and written over UART but not
read, so the factory firmware cannot be backed up. To go back, take the factory
OTA image for `lumi.relay.c2acn01` from the Zigbee2MQTT OTA index and update over
the air (or write its payload by wire with the same tool).

The debug log comes out on the same UART (PROG TxD/RxD) at 115200 baud.

## Behaviour

| Input | Action |
|---|---|
| On-board button, short press | toggle relay 1; if not joined, look for a network |
| On-board button, hold 5 s | leave the network and factory reset |
| S1 / S2 wall switch | toggle relay 1 / 2 (rocker mode: on every change) |

| LED | Meaning |
|---|---|
| blue slow blink | looking for a network (gives up after ~10 min) |
| blue on 3 s | joined |
| blue fast blink | Identify |
| red fast blink | factory reset in progress |
| red on 3 s | no network found |

Relay state is restored after a power cut (StartUpOnOff, default "previous").

## Pin map

See `Source/hw.h`. Relays: DIO10/DIO8 (K1 on/off), DIO11/DIO9 (K2 on/off).
Not implemented yet: PL7211 power metering (SPI on DO0/DIO18/DIO19/DO1).
