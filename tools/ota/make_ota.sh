#!/bin/sh
# Turn the linked firmware into
#   <name>_flash.bin  for the UART flasher (embedded OTA header, reported version)
#   <name>.ota        Zigbee OTA file for Zigbee2MQTT (file version above the factory one)
# the same way NXP JN-AN-1218 builds its OTA client images with JET.
#
# make_ota.sh <in.bin> <out dir> <name> <manufacturer> <image type> <header string>
#             <reported version> <file version>
set -e
IN=$1; OUT=$2; NAME=$3; MANUF=$4; TYPE=$5; HDR=$6; REPORTED=$7; FILEVER=$8
JET=${JET:-/c/NXP/bstudio_nxp/sdk/JN-SW-4170/Tools/OTAUtils/JET.exe}
HERE=$(cd "$(dirname "$0")" && pwd)
WORK=$OUT/ota_work

rm -rf "$WORK"
mkdir -p "$WORK"
cp "$IN" "$WORK/in.bin"
cp "$HERE/serialisation.txt" "$HERE/MACAddr_Dummy.txt" "$HERE/LinkKey_HA.txt" "$WORK/"
cd "$WORK"

# MAC placeholder and link key into the image, unencrypted (all-ones key)
"$JET" -m combine -f in.bin -x serialisation.txt -v 4 -g 1 -k 0xffffffffffffffffffffffffffffffff \
       -u "$MANUF" -t "$TYPE" -j "$HDR" > jet.log
COMBINED=$(ls output*.bin)

# embedded OTA header: what the running firmware reports as its version
"$JET" -m otamerge --embed_hdr -c "$COMBINED" -o flash.bin -v 4 -n "$REPORTED" \
       -u "$MANUF" -t "$TYPE" -j "$HDR" >> jet.log

# OTA file around it with the version offered to the factory firmware
"$JET" -m otamerge --ota -c flash.bin -o image.ota -p 1 -v 4 -n "$FILEVER" \
       -u "$MANUF" -t "$TYPE" -j "$HDR" >> jet.log

cp flash.bin "../${NAME}_flash.bin"
cp image.ota "../${NAME}.ota"
ls -l "../${NAME}_flash.bin" "../${NAME}.ota"
