"""Write a Zigbee2MQTT OTA index entry for an .ota file.

    python make_index.py ota/lumi.relay.c2acn01_custom_0.2.0.ota \
        https://raw.githubusercontent.com/<user>/<repo>/main/ota/ ota/index.json "release notes"

Everything but the URL and notes is read from the file itself. Entries for web
URLs must carry all fields: Zigbee2MQTT does not open the file to look them up.
"""
import hashlib
import json
import os
import struct
import sys

UPGRADE_FILE_IDENTIFIER = 0x0BEEF11E


def main():
    path, base_url, out = sys.argv[1:4]
    notes = sys.argv[4] if len(sys.argv) > 4 else ""
    data = open(path, "rb").read()

    magic, _hver, hlen, _fc, manufacturer, image_type, file_version = struct.unpack_from("<IHHHHHI", data)
    if magic != UPGRADE_FILE_IDENTIFIER:
        sys.exit(f"{path}: not a Zigbee OTA file")
    total = struct.unpack_from("<I", data, 52)[0]
    if total != len(data):
        sys.exit(f"{path}: header says {total} bytes, file has {len(data)}")
    header_string = data[20:52].decode("ascii").rstrip("\0")

    entry = {
        "url": base_url.rstrip("/") + "/" + os.path.basename(path),
        "fileVersion": file_version,
        "fileSize": len(data),
        "manufacturerCode": manufacturer,
        "imageType": image_type,
        "sha512": hashlib.sha512(data).hexdigest(),
        "modelId": "lumi.relay.c2acn01",
        "manufacturerName": ["LUMI"],
        "otaHeaderString": header_string,
    }
    if notes:
        entry["releaseNotes"] = notes

    index = []
    if os.path.exists(out):
        # keep older entries of other versions
        index = [e for e in json.load(open(out)) if e.get("fileVersion") != file_version]
    index.append(entry)
    with open(out, "w", newline="\n") as f:
        json.dump(index, f, indent=2)
        f.write("\n")
    print(f"{out}: version {file_version:#010x}, {len(data)} bytes, type {image_type:#06x}, manufacturer {manufacturer:#06x}")


if __name__ == "__main__":
    main()
