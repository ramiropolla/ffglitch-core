#!/bin/sh

set -x
set -e

./ffedit_g -t -hide_banner "$2" > "$1"
