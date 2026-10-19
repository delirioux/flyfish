#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# FLYFISH in the browser: firmware/src/ff_fw.c and Flyfish's modules on Felucca's browser build (web/emu/ff_web.c),
# compiled to WebAssembly, plus Felucca's page and worklet with Flyfish's name on them (after Tuna's tuna_build.sh).
#   web/emu/ff_build.sh   ->  build/ff_emu/{index.html, worklet.js, flyfish.wasm, flyfish-logo.png, fonts/}
# Needs build/gen (the generated headers: tools/build.py's generate step) and either Emscripten (emcc) or clang
# with wasm-ld and a WASI sysroot: WASI_SYSROOT=dir (include/wasm32-wasip1, lib/wasm32-wasip1 and
# lib/wasm32-unknown-wasip1/libclang_rt.builtins.a; the npm package @yowasp/clang carries one in
# gen/llvm-resources.tar; crt1-reactor.o gives the _initialize the worklet calls). The module has no imports the page must answer but WASI's, which the worklet stubs:
# nothing of Flyfish's or Felucca's runtime reaches them.
set -e
cd "$(dirname "$0")/../.."
[ -f build/gen/felucca_tables.h ] || { echo "flyfish emu: build/gen first (tools/build.py generate)"; exit 1; }
VERSION=$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/felucca.c)
FFV=$(sed -n 's/^#define FF_VERSION "\([^"]*\)".*/\1/p' firmware/src/ff_version.h)
OUT=build/ff_emu
mkdir -p "$OUT/fonts" "$OUT/obj"
MODS="ff_player tuna_reso tuna_libm"
if command -v emcc >/dev/null 2>&1; then
    SRCS=""
    for m in $MODS; do SRCS="$SRCS firmware/src/$m.c"; done
    # shellcheck disable=SC2086
    emcc -O2 -ffp-contract=off -std=gnu99 -w -DFELUCCA_SLICE=0 "-DFELUCCA_VERSION=\"$VERSION\"" -Ibuild/gen -Ifirmware/src \
        --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 -sFILESYSTEM=0 \
        -o "$OUT/flyfish.wasm" web/emu/ff_web.c $SRCS
else
    R=${WASI_SYSROOT:?"flyfish emu: no emcc; set WASI_SYSROOT to a WASI sysroot for clang"}
    SHIM="$OUT/shim"
    mkdir -p "$SHIM/sys"
    # (the host tests' fork / pipe / wait, which hostsim.c names and the browser never calls)
    printf '#pragma once\n#include <sys/types.h>\npid_t waitpid(pid_t, int *, int);\npid_t fork(void);\nint pipe(int[2]);\n#define WEXITSTATUS(s) (((s) >> 8) & 0xff)\n#define WIFEXITED(s) (((s) & 0x7f) == 0)\n' > "$SHIM/sys/wait.h"
    CF="--target=wasm32-wasip1 -nodefaultlibs -isystem $SHIM -isystem $R/include/wasm32-wasip1 -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src"
    # shellcheck disable=SC2086
    clang $CF -std=gnu99 -D_WASI_EMULATED_PROCESS_CLOCKS -D_WASI_EMULATED_SIGNAL -DFELUCCA_SLICE=0 "-DFELUCCA_VERSION=\"$VERSION\"" \
        -c web/emu/ff_web.c -o "$OUT/obj/ff_web.o"
    OBJS="$OUT/obj/ff_web.o"
    for m in $MODS; do
        # shellcheck disable=SC2086
        clang $CF -std=c11 -fno-builtin -c "firmware/src/$m.c" -o "$OUT/obj/$m.o"
        OBJS="$OBJS $OUT/obj/$m.o"
    done
    # shellcheck disable=SC2086
    wasm-ld --no-entry --export-dynamic --gc-sections -z stack-size=1048576 --initial-memory=33554432 \
        -L"$R/lib/wasm32-wasip1" "$R/lib/wasm32-wasip1/crt1-reactor.o" $OBJS -lc -lm "$R/lib/wasm32-unknown-wasip1/libclang_rt.builtins.a" -o "$OUT/flyfish.wasm"
fi
# Felucca's worklet, its module given WASI stubs; Felucca's page with Flyfish's name, its own saves
sed -e 's|WebAssembly.instantiate(m.wasm, {})|WebAssembly.instantiate(m.wasm, { wasi_snapshot_preview1: new Proxy({}, { get: () => () => 0 }) })|' \
    -e 's|^// Felucca in an AudioWorklet: felucca.wasm|// Flyfish (and Felucca) in an AudioWorklet: flyfish.wasm|' web/emu/worklet.js > "$OUT/worklet.js"
grep -q "wasi_snapshot_preview1" "$OUT/worklet.js" || { echo "flyfish emu: the worklet's instantiate line moved"; exit 1; }
sed -e "s/@VERSION@/$VERSION/g" -e "s/@PREVIEW@/ (Flyfish $FFV)/g" \
    -e 's|<title>Felucca</title>|<title>Waterfall Flying Fish</title>|' \
    -e 's|content="Felucca, the multi-engine synth firmware for the FM-1, running in your browser: the same code the instrument runs."|content="Flyfish, the Waterfall etude on the FM-1, running in your browser: the same code the instrument runs."|' \
    -e 's|const STORE = "felucca-emu-flash"|const STORE = "flyfish-emu-flash"|' -e 's|"felucca-emu-knobs"|"flyfish-emu-knobs"|' -e 's|"felucca-emu-knobmode"|"flyfish-emu-knobmode"|' \
    -e 's|fetch("felucca.wasm")|fetch("flyfish.wasm")|' -e 's|felucca.wasm: |flyfish.wasm: |' \
    -e 's|  Felucca by Leo Kuroshita, Hügelton Instruments|  Flyfish on Felucca · Felucca by Leo Kuroshita, Hügelton Instruments · the Steinway: bitKlavier Grand, Princeton (CC BY 4.0)|' \
    -e 's|  <nav><a href="../installer/">Install</a><a href="../editor/">Editor</a><a href="https://github.com/hugelton/Felucca">Source</a></nav>|  <nav><a href="../install/">Install</a><a href="https://github.com/delirioux/flyfish">Flyfish source</a><a href="https://github.com/hugelton/Felucca">Felucca source</a></nav>|' \
    -e 's|Felucca is not affiliated with or endorsed by them.|Flyfish and Felucca are not affiliated with or endorsed by them.|' \
    -e 's|<link rel="icon" href="[^"]*">|<link rel="icon" href="flyfish-logo.png">|' \
    -e 's|^<div class="device" id="device">|<img src="flyfish-logo.png" alt="Flyfish" width="128" height="128" style="display:block;margin:0 auto 12px;border-radius:50%">\n&|' \
    web/emu/index.html > "$OUT/index.html"
cp web/emu/fonts/DotGothic16-subset.woff web/emu/fonts/OFL.txt "$OUT/fonts/"
cp docs/flyfish-logo.png "$OUT/"
grep -q 'src="flyfish-logo.png"' "$OUT/index.html" || { echo "flyfish emu: the page's device line moved"; exit 1; }
echo "flyfish emu: $OUT ($(wc -c < "$OUT/flyfish.wasm" | tr -d ' ') B wasm, Flyfish $FFV on Felucca $VERSION)"
