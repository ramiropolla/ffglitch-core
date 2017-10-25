#!/bin/sh

set -x
set -e

TMPJSON=$(mktemp)
OUTPUT=$(mktemp)

./ffglitch_g -t -hide_banner "$2" $4 -e "$3"
./src/tests/ffglitch/scripts/"$5" "$3" > ${TMPJSON}
./ffglitch_g -t -hide_banner "$2" $4 -a ${TMPJSON} ${OUTPUT}
./ffmpeg_g -hide_banner -i ${OUTPUT} -flags +bitexact -fflags +bitexact -idct simple -f framecrc - > "$1"
