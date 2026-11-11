#!/bin/sh
# Build the firmware with the BeyondStudio toolchain. Extra arguments go to make,
# e.g. ./build.sh DEBUG=1   or   ./build.sh clean
NXP=${NXP:-/c/NXP/bstudio_nxp}
export PATH="$NXP/sdk/Tools/ba-elf-ba2-r36379/bin:$NXP/msys/bin:$PATH"
cd "$(dirname "$0")/Build" && make "$@"
