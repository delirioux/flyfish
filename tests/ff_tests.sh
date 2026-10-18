#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Flyfish's host tests: the étude's table and the splash up to date, the player (tests/ff_player_test.c), the screen
# and its layout lint (tests/ff_ui_test.sh), the player strict about warnings and single precision.
#   sh tests/ff_tests.sh        (exit 0: all passed)
set -eu
mkdir -p build/host
python3 tools/ff_etude.py --check || { echo "firmware/src/ff_etude.inc is out of date: python3 tools/ff_etude.py"; exit 1; }
python3 tools/ff_logo.py --check
cc -std=c11 -O2 -Wall -Wextra -Wshadow -Wdouble-promotion -Werror -Ifirmware/src -c firmware/src/ff_player.c \
   -o build/host/ff_player.o
cc -std=c11 -O2 -Wall -Ifirmware/src -o build/host/ff_player_test tests/ff_player_test.c firmware/src/ff_player.c -lm
build/host/ff_player_test
sh tests/ff_ui_test.sh build/ff_ui
echo "ff_tests: all passed"
