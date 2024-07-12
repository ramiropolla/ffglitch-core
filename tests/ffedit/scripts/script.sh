#!/bin/sh

set -x
set -e

TMPJSON=$(mktemp)
OUTPUT=$(mktemp)

./ffedit_g -t -hide_banner -i "$2" $4 -e "$3"
./src/tests/ffedit/scripts/"$5" "$3" > ${TMPJSON}
./ffedit_g -t -hide_banner -i "$2" $4 -a ${TMPJSON} -o ${OUTPUT}
./ffgac_g -hide_banner -flags +bitexact -threads 1 -i ${OUTPUT} -flags +bitexact -fflags +bitexact -idct simple -fps_mode passthrough -f framecrc - > "$1"
