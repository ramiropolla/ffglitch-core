#!/bin/sh

set -x
set -e

TMPREF=$(mktemp)
TMPJSON=$(mktemp)
OUTPUT=$(mktemp)

./ffgac_g -hide_banner -flags +bitexact -threads 1 -i "$2" -flags +bitexact -fflags +bitexact -idct simple -fps_mode passthrough -f framecrc - > ${TMPREF}
./ffedit_g -t -hide_banner -i "$2" $4 -e ${TMPJSON}
./ffedit_g -t -hide_banner -i "$2" $4 -a ${TMPJSON} -o ${OUTPUT}
./ffedit_g -t -hide_banner -i ${OUTPUT} $4 -e "$3"
./ffgac_g -hide_banner -flags +bitexact -threads 1 -i ${OUTPUT} -flags +bitexact -fflags +bitexact -idct simple -fps_mode passthrough -f framecrc - > "$1"
sed -i -n '/"filename"/!p' ${TMPJSON}
sed -i -n '/"filename"/!p' "$3"
sed -i -n '/"sha1sum"/!p' ${TMPJSON}
sed -i -n '/"sha1sum"/!p' "$3"
cmp -s ${TMPREF} "$1" || { echo "TRANSPLICATION MISMATCH (ref)" >> "$1" && false; }
cmp -s ${TMPJSON} "$3" || { echo "TRANSPLICATION MISMATCH (json)" >> "$1" && false; }
