#!/bin/sh

set -x
set -e

OUTPUT=$(mktemp)

./ffedit_g -t -hide_banner -i "$2" $4 -s ./src/tests/ffedit/scripts/"$5" -o ${OUTPUT}
./ffmpeg_g -hide_banner -flags +bitexact -threads 1 -i ${OUTPUT} -flags +bitexact -fflags +bitexact -idct simple -f framecrc - > "$1"
./ffedit_g -t -hide_banner -i ${OUTPUT} $4 -e "$3"
sed -i -n '/"filename"/!p' "$3"
