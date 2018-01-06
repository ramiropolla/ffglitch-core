#!/bin/sh

set -x
set -e

./ffglitch_g -t -hide_banner "$2" > "$1"
