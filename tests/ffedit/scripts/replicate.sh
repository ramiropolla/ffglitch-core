#!/bin/sh

set -x
set -e

OUTPUT=$(mktemp)

./ffedit_g -t -hide_banner "$2" ${OUTPUT}
./ffmpeg_g -hide_banner -flags +bitexact -threads 1 -i ${OUTPUT} -flags +bitexact -fflags +bitexact -idct simple -f framecrc - > "$1"
cmp -s "$2" ${OUTPUT} || { echo "REPLICATION MISMATCH" >> "$1" && false; }
