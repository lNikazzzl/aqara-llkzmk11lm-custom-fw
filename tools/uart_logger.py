"""Keep the board's UART log in a file, forever.

    python uart_logger.py COM3 logs/uart.log

Reopens the port whenever it disappears (adapter unplugged, board repowered).
Every line gets a timestamp. While the file <log>.pause exists the port is
released, so the flasher or the EEPROM tools can use it.
"""
import os
import sys
import time

import serial


def stamp():
    return time.strftime("%H:%M:%S")


def main():
    port, path = sys.argv[1], sys.argv[2]
    baud = int(sys.argv[3]) if len(sys.argv) > 3 else 115200
    pause = path + ".pause"
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)

    s = None
    line = b""
    state = None    # last state written to the log, to avoid repeating it

    def note(f, text):
        nonlocal state
        if state != text:
            state = text
            f.write(f"{stamp()} --- {text} ---\n".encode())
            f.flush()

    with open(path, "ab") as f:
        note(f, f"logger started on {port} at {baud}")
        while True:
            if os.path.exists(pause):
                if s is not None:
                    s.close()
                    s = None
                note(f, "paused, port released")
                time.sleep(0.3)
                continue
            try:
                if s is None:
                    s = serial.Serial(port, baud, timeout=0.5)
                    note(f, "port open")
                data = s.read(4096)
            except (serial.SerialException, OSError):
                if s is not None:
                    try:
                        s.close()
                    except Exception:
                        pass
                    s = None
                note(f, "port lost, waiting")
                time.sleep(0.3)
                continue

            if not data:
                continue
            state = None
            line += data
            while b"\n" in line:
                text, line = line.split(b"\n", 1)
                f.write(stamp().encode() + b" " + text.rstrip(b"\r") + b"\n")
            if len(line) > 512:     # binary noise without line ends
                f.write(stamp().encode() + b" " + line + b"\n")
                line = b""
            f.flush()


if __name__ == "__main__":
    main()
