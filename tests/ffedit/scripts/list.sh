#!/bin/sh

set -x
set -e

./ffedit_g -t -hide_banner -i "$2" > "$1"
