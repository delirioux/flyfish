#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Flyfish through Felucca's engines on the host (tests/ff_felucca.c): the étude to a WAV.
#   tests/ff_felucca.sh OUTDIR [SECONDS]     (needs build/gen: run ./build.sh once, or tools/build.py's generate)
set -eu
OUT=${1:?usage: tests/ff_felucca.sh OUTDIR [SECONDS]}
mkdir -p build/host "$OUT"
[ -f build/gen/felucca_tables.h ] || python3 -c "import sys; sys.argv=['x']; sys.path.insert(0,'tools'); import build; build.generate()"
cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/ff_felucca tests/ff_felucca.c firmware/src/ff_player.c \
   firmware/src/tuna_reso.c -lm
build/host/ff_felucca "$OUT" "${2:-0}"
