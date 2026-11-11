# Third-party software

## NXP

`firmware/Source/` contains code derived from, or copied from, NXP software:

- the JN-AN-1218 "ZigBee 3.0 Light Bulbs" example application,
- the JN-SW-4170 ZigBee 3.0 SDK for JN516x.

Files carrying an NXP notice at the top (`app_main.c`, `app_node.c`, `app_zcl.c`,
`app_ota.c`, `app_device.c`, `zcl_options.h`, `bdb_options.h`, `irq_JN516x.S`) are
covered by the NXP licence, which grants use "solely and exclusively on NXP
products". The notice must be kept on every copy or partial copy. The release
binaries (`ota/*.ota`) link NXP SDK libraries and are for NXP JN5169 devices only.

The SDK and the example are not part of this repository; download them from nxp.com.
`tools/make_zpscfg.py` derives the stack configuration from the example on your machine.

## Everything else

The remaining sources (`relay.c`, `inputs.c`, `led.c`, `lumi.c`, `app_store.c`,
`hw.h`, `tools/`) are by the project author, released under the MIT licence:

> Permission is hereby granted, free of charge, to any person obtaining a copy of
> this software and associated documentation files (the "Software"), to deal in the
> Software without restriction, including without limitation the rights to use,
> copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
> Software, and to permit persons to whom the Software is furnished to do so,
> subject to the following conditions: The above copyright notice and this
> permission notice shall be included in all copies or substantial portions of the
> Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
> OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
> WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
> CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Not affiliated

Aqara and Lumi are trademarks of their owners. This project is independent and
does not contain or distribute their firmware.
