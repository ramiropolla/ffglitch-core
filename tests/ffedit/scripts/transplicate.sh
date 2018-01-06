#!/bin/sh

set -x
set -e

TMPJSON=$(mktemp)
OUTPUT=$(mktemp)

./ffedit_g -t -hide_banner "$2" -e ${TMPJSON}
./ffedit_g -t -hide_banner "$2" -a ${TMPJSON} ${OUTPUT}
./ffmpeg_g -hide_banner -flags +bitexact -threads 1 -i ${OUTPUT} -flags +bitexact -fflags +bitexact -idct simple -f framecrc - > "$1"
cmp -s "$2" ${OUTPUT} || { echo "TRANSPLICATION MISMATCH" >> "$1" && false; }
