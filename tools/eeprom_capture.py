"""Capture the EEPROM dump printed by tools/eeprom_dump firmware.

    python eeprom_capture.py COM3 eeprom_backup.bin

Waits for two identical dumps with a valid CRC and saves the raw bytes.
"""
import sys
import time
import zlib

import serial


def read_dump(s, deadline):
    """Return (segment_len, bytes) of the next complete dump, or None on timeout."""
    while time.time() < deadline:
        line = s.readline().decode("ascii", "replace").strip()
        if not line.startswith("EEP "):
            continue
        nseg, seglen = (int(x, 16) for x in line[4:].split())
        data = bytearray()
        for idx in range(nseg):
            line = s.readline().decode("ascii", "replace").strip()
            if not line.startswith(f"S{idx:04x}:"):
                print(f"unexpected line: {line!r}")
                break
            try:
                data += bytes.fromhex(line[6:])
            except ValueError:
                # garbage on the line, e.g. while the board powers up
                print(f"corrupt line for segment {idx}, waiting for the next dump")
                break
        else:
            end = s.readline().decode("ascii", "replace").strip()
            if end.startswith("END ") and int(end[4:], 16) == zlib.crc32(data) and len(data) == nseg * seglen:
                return seglen, bytes(data)
            print(f"bad dump trailer: {end!r}")
    return None


def main():
    port, out = sys.argv[1], sys.argv[2]
    s = serial.Serial(port, 115200, timeout=2)
    deadline = time.time() + 60
    prev = None
    while True:
        dump = read_dump(s, deadline)
        if dump is None:
            sys.exit("no valid dump within 60 s")
        seglen, data = dump
        print(f"got {len(data)} bytes ({len(data) // seglen} segments x {seglen}), crc32 {zlib.crc32(data):08x}")
        if data == prev:
            break
        prev = data
    open(out, "wb").write(data)
    # a blank segment is one byte value repeated
    used = sum(1 for i in range(0, len(data), seglen) if len(set(data[i:i + seglen])) > 1)
    print(f"saved {out}: {used} of {len(data) // seglen} segments are not blank")


if __name__ == "__main__":
    main()
