"""Write eeprom_backup.bin back with the tools/eeprom_dump restore firmware.

    python eeprom_restore.py COM3 eeprom_backup.bin eeprom_before_restore.bin

1. saves the current EEPROM to the third file,
2. sends the restore command and waits for the result,
3. dumps the EEPROM again and compares it with the backup.
"""
import os
import sys
import time
import zlib

import serial

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from eeprom_capture import read_dump

COMMAND = b"RESTORE-EEPROM\r\n"


def wait_for(s, words, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        line = s.readline().decode("ascii", "replace").strip()
        if line.startswith(("RESTORE", "BAD")):
            print("  ", line)
        for w in words:
            if line.startswith(w):
                return line
    return None


def main():
    port, backup_path, before_path = sys.argv[1:4]
    backup = open(backup_path, "rb").read()
    s = serial.Serial(port, 115200, timeout=2)

    dump = read_dump(s, time.time() + 60)
    if dump is None:
        sys.exit("no dump from the board (is the restore firmware running?)")
    open(before_path, "wb").write(dump[1])
    print(f"current EEPROM saved to {before_path} ({len(dump[1])} bytes, crc32 {zlib.crc32(dump[1]):08x})")
    if len(dump[1]) != len(backup):
        sys.exit(f"size mismatch: board {len(dump[1])}, backup {len(backup)}")

    # the firmware listens between dumps, right after an END line
    for attempt in range(5):
        if wait_for(s, ["END "], 30) is None:
            sys.exit("lost the dump stream")
        s.write(COMMAND)
        if wait_for(s, ["RESTORE START", "RESTORE REFUSED"], 5):
            break
    else:
        sys.exit("the board did not accept the restore command")

    result = wait_for(s, ["RESTORE OK", "RESTORE FAILED", "RESTORE REFUSED"], 120)
    if result != "RESTORE OK":
        sys.exit(f"restore did not succeed: {result}")

    dump = read_dump(s, time.time() + 60)
    if dump is None:
        sys.exit("no dump after the restore")
    if dump[1] == backup:
        print(f"verified: EEPROM equals {backup_path} (crc32 {zlib.crc32(backup):08x})")
    else:
        diff = [i // 64 for i in range(0, len(backup), 64) if dump[1][i:i + 64] != backup[i:i + 64]]
        sys.exit(f"EEPROM differs from the backup in segments {diff}")


if __name__ == "__main__":
    main()
