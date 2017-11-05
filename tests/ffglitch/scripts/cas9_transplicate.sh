#!/bin/sh

set -x
set -e

TMPJSON=$(mktemp)
OUTPUT=$(mktemp)

./ffglitch_g -t -hide_banner "$2" -e ${TMPJSON}
./ffglitch_g -t -hide_banner "$2" -a ${TMPJSON} ${OUTPUT}
./ffmpeg_g -hide_banner -flags +bitexact -i ${OUTPUT} -flags +bitexact -fflags +bitexact -idct simple -f framecrc - > "$1"
cmp -s "$2" ${OUTPUT} || { echo "TRANSPLICATION MISMATCH" >> "$1" && false; }
