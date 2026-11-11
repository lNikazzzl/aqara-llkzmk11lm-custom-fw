"""Add a Zigbee2MQTT OTA index entry for an .ota file.

Everything but the URL and the version window is read from the file itself:
entries for web URLs must carry all fields, Zigbee2MQTT does not open the file.

Our image is offered to devices running the factory firmware (version >= 2),
the factory image to devices running ours (it reports version 1), so one index
switches the firmware in both directions:

    python make_index.py ota/lumi.relay.c2acn01_custom_0.2.0.ota ota/index.json \
        --base-url https://raw.githubusercontent.com/<user>/<repo>/main/ota \
        --min-version 2 --max-version 16777727 --notes "release notes"

    python make_index.py lumi.relay.c2acn01_20211201_v0047.ota ota/index.json \
        --url https://raw.githubusercontent.com/Koenkk/zigbee-OTA/master/images/Lumi/lumi.relay.c2acn01_20211201_v0047.ota \
        --min-version 0 --max-version 1
"""
import argparse
import hashlib
import json
import os
import struct
import sys

UPGRADE_FILE_IDENTIFIER = 0x0BEEF11E


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("ota", help=".ota file to describe")
    p.add_argument("index", help="index.json to create or extend")
    p.add_argument("--base-url", help="directory URL the file is published under")
    p.add_argument("--url", help="full URL of the file (when it is hosted elsewhere)")
    p.add_argument("--min-version", type=int, help="offer only to devices running at least this file version")
    p.add_argument("--max-version", type=int, help="offer only to devices running at most this file version")
    p.add_argument("--notes", default="")
    args = p.parse_args()
    if bool(args.url) == bool(args.base_url):
        p.error("give exactly one of --url and --base-url")

    data = open(args.ota, "rb").read()
    magic, _hver, _hlen, _fc, manufacturer, image_type, file_version = struct.unpack_from("<IHHHHHI", data)
    if magic != UPGRADE_FILE_IDENTIFIER:
        sys.exit(f"{args.ota}: not a Zigbee OTA file")
    total = struct.unpack_from("<I", data, 52)[0]
    if total != len(data):
        sys.exit(f"{args.ota}: header says {total} bytes, file has {len(data)}")

    url = args.url or args.base_url.rstrip("/") + "/" + os.path.basename(args.ota)
    entry = {
        "url": url,
        "fileVersion": file_version,
        "fileSize": len(data),
        "manufacturerCode": manufacturer,
        "imageType": image_type,
        "sha512": hashlib.sha512(data).hexdigest(),
        "modelId": "lumi.relay.c2acn01",
        "manufacturerName": ["LUMI"],
        "otaHeaderString": data[20:52].decode("ascii").rstrip("\0"),
    }
    if args.min_version is not None:
        entry["minFileVersion"] = args.min_version
    if args.max_version is not None:
        entry["maxFileVersion"] = args.max_version
    if args.notes:
        entry["releaseNotes"] = args.notes

    index = json.load(open(args.index)) if os.path.exists(args.index) else []
    index = [e for e in index if e["url"] != url]
    index.append(entry)
    with open(args.index, "w", newline="\n") as f:
        json.dump(index, f, indent=2)
        f.write("\n")
    window = f", for running versions {args.min_version}..{args.max_version}" if args.min_version is not None else ""
    print(f"{args.index}: {os.path.basename(url)} version {file_version:#010x}, {len(data)} bytes{window}")


if __name__ == "__main__":
    main()
