#!/usr/bin/env python3
"""Decode the PL7211 probe reports (firmware 0.2.6, pl7211.c) from the Z2M debug log.

Paste the log lines (or just the byte lists) on stdin, e.g. lines containing
  "65525":{"type":"Buffer","data":[1,0,...]}   (0xFFF5, CFG dump)
  "65526":{"type":"Buffer","data":[...]}       (0xFFF6, measurements)
  python tools/pl7211_decode.py < z2m.log
"""
import re
import sys


def le(b):
    return int.from_bytes(bytes(b), "little")


def cfg(d):
    print(f"probe v{d[0]} flags {d[1]:#04x}")
    for name, part in (("rx rising ", d[2:18]), ("rx falling", d[18:34])):
        print(f"  CFG 0x3800 {name}: " + " ".join(f"{x:02x}" for x in part))
        if len(part) >= 11:
            print(f"    SampleCnt (0x3809) = {le(part[9:11])} (Prolific example 0x07A1 = 1953)")


def data(d):
    v, i, p, cf = (le(d[k:k + 6]) for k in (0, 6, 12, 18))
    zcnt, zstart, zstop = (le(d[k:k + 6]) for k in (24, 30, 36))
    print("  raw: " + " ".join(f"{x:02x}" for x in d))
    print(f"  Vrms   {v / 2**24:10.3f} V")
    print(f"  Irms   {i / 2**30:10.4f} A")
    print(f"  P      {p / 2**24:10.2f} W   (signed: {(p - (1 << 48) if p >> 47 else p) / 2**24:.2f})")
    print(f"  energy {cf * 0.3125 / 1000:10.4f} kWh  (CF count {cf})")
    print(f"  ZccCnt {zcnt}  ZccStart {zstart}  ZccStop {zstop}  (freq needs SampleCnt from CFG)")


text = sys.stdin.read()
found = False
for attr, body in re.findall(r'"(6552[56])"\s*:\s*\{[^}]*?"data"\s*:\s*\[([0-9,\s]*)\]', text):
    found = True
    d = [int(x) for x in body.replace(" ", "").split(",") if x]
    print(f"--- {'0xFFF5 CFG' if attr == '65525' else '0xFFF6 data'} ({len(d)} bytes)")
    (cfg if attr == "65525" else data)(d)
if not found:
    print("no 65525/65526 reports found in the input")
