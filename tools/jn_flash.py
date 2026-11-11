"""Flash tool for the NXP JN5169 ROM UART bootloader (protocol per NXP JN-AN-1003).

    python jn_flash.py COM3 info
    python jn_flash.py COM3 flash firmware.bin --erase-ok

The chip must be in bootloader mode (BOOT/DO1 held low while it powers up or
resets). This board is at CRP level 1: internal flash can be erased and
written over UART but never read back, so a flash erase cannot be undone.
"""
import argparse
import struct
import sys
import time

import serial

GET_CHIP_ID = 0x32
SELECT_FLASH = 0x2C
READ_FLASH_ID = 0x25
FLASH_ERASE = 0x07
FLASH_PROGRAM = 0x09
RAM_READ = 0x1F
RESET = 0x14

JN5169_CHIP_ID = 0x0100B686
JN5169_IMAGE_HEADER = 0x0F03000B    # .version word the SDK linker puts before the image
INTERNAL_FLASH = 0x08
FLASH_SIZE = 512 * 1024
CHUNK = 128

STATUS = {0x00: "OK", 0xFF: "not supported", 0xFE: "write fail", 0xFD: "invalid response",
          0xFC: "CRC error", 0xFB: "assert fail", 0xFA: "user interrupt", 0xF9: "read fail",
          0xF8: "TST error", 0xF7: "auth error", 0xF6: "no response"}


class BootloaderError(Exception):
    pass


class Bootloader:
    def __init__(self, port, baud=38400):
        self.s = serial.Serial(port, baud, timeout=1)
        self.s.reset_input_buffer()

    def xfer(self, typ, payload=b"", timeout=1.0):
        body = bytes([len(payload) + 2, typ]) + payload
        cs = 0
        for b in body:
            cs ^= b
        self.s.timeout = timeout
        self.s.write(body + bytes([cs]))
        hdr = self.s.read(1)
        if not hdr:
            raise BootloaderError(f"no response to command 0x{typ:02x} (is the chip in bootloader mode?)")
        rest = self.s.read(hdr[0])
        if len(rest) != hdr[0]:
            raise BootloaderError(f"short response to command 0x{typ:02x}")
        cs = hdr[0]
        for b in rest[:-1]:
            cs ^= b
        if cs != rest[-1]:
            raise BootloaderError(f"bad checksum in response to command 0x{typ:02x}")
        rtype, data = rest[0], rest[1:-1]
        if rtype != typ + 1:
            raise BootloaderError(f"command 0x{typ:02x} rejected (response type 0x{rtype:02x})")
        return data

    def checked(self, typ, payload=b"", timeout=1.0):
        data = self.xfer(typ, payload, timeout)
        if data[0] != 0:
            raise BootloaderError(f"command 0x{typ:02x} failed: {STATUS.get(data[0], hex(data[0]))}")
        return data[1:]

    def chip_id(self):
        data = self.checked(GET_CHIP_ID)
        return struct.unpack(">I", data[:4])[0]

    def ram_read(self, addr, length):
        return self.checked(RAM_READ, struct.pack("<IH", addr, length))


def cmd_info(bl, args):
    print(f"chip id   : 0x{bl.chip_id():08x}")
    print(f"MAC       : {bl.ram_read(0x01001580, 8).hex(':')}")
    cfg = bl.ram_read(0x01001510, 4)[3]
    crp = {0: 2, 1: 1, 2: 2, 3: 0}[(cfg >> 4) & 3]
    print(f"settings  : 0x{cfg:02x} (CRP level {crp})")


def cmd_flash(bl, args):
    image = open(args.file, "rb").read()
    header = struct.unpack(">I", image[:4])[0]
    if header != JN5169_IMAGE_HEADER:
        sys.exit(f"{args.file}: not a JN5169 image (header 0x{header:08x})")
    image = image[4:]
    if len(image) > FLASH_SIZE:
        sys.exit(f"{args.file}: image is larger than the flash")
    if image[:12] != bytes.fromhex("123456781122334455667788"):
        sys.exit(f"{args.file}: boot image record magic missing")

    chip = bl.chip_id()
    if chip != JN5169_CHIP_ID:
        sys.exit(f"unexpected chip id 0x{chip:08x}")
    if not args.erase_ok:
        sys.exit("refusing to erase the flash without --erase-ok (the current firmware cannot be read back)")

    bl.checked(SELECT_FLASH, bytes([INTERNAL_FLASH]) + struct.pack("<I", 0))

    print("erasing flash ...")
    bl.checked(FLASH_ERASE, timeout=20.0)

    start = time.time()
    for off in range(0, len(image), CHUNK):
        chunk = image[off:off + CHUNK]
        bl.checked(FLASH_PROGRAM, struct.pack("<I", off) + chunk, timeout=2.0)
        if (off // CHUNK) % 64 == 0 or off + CHUNK >= len(image):
            done = min(off + CHUNK, len(image))
            print(f"\rwriting {done}/{len(image)} bytes", end="", flush=True)
    print(f"\nwritten in {time.time() - start:.0f} s")

    if not args.no_reset:
        # the reset response may be lost while the chip restarts
        try:
            bl.xfer(RESET)
        except BootloaderError:
            pass
        print("reset sent; release BOOT so the new firmware starts")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("port")
    sub = p.add_subparsers(dest="cmd", required=True)
    sub.add_parser("info", help="show chip id, MAC and protection level")
    f = sub.add_parser("flash", help="erase the flash and write a .bin")
    f.add_argument("file")
    f.add_argument("--erase-ok", action="store_true", help="confirm that the flash may be erased")
    f.add_argument("--no-reset", action="store_true")
    args = p.parse_args()

    bl = Bootloader(args.port)
    try:
        {"info": cmd_info, "flash": cmd_flash}[args.cmd](bl, args)
    except BootloaderError as e:
        sys.exit(f"error: {e}")


if __name__ == "__main__":
    main()
