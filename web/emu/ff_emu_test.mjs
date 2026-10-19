// SPDX-License-Identifier: GPL-3.0-only
// FLYFISH in Node, as the browser's worklet runs it (build/ff_emu/flyfish.wasm, web/emu/ff_web.c). After Tuna's test.
//   node web/emu/ff_emu_test.mjs build/ff_emu/flyfish.wasm
// Checks: it boots stopped and silent on HOME; PLAY plays the étude (sound, the bars moving, PLAY's LED); the
// pages open; PLAY again stops to silence; a setting changed while stopped reaches the flash.
import fs from "fs";

const bytes = fs.readFileSync(process.argv[2] || "build/ff_emu/flyfish.wasm");
const stub = new Proxy({}, { get: () => () => 0 });   // wasi imports the reactor never calls
const { instance } = await WebAssembly.instantiate(bytes, { wasi_snapshot_preview1: stub });
const w = instance.exports;
const B = { FX: 0, SCL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11, OCTDN: 12, OCTUP: 13 };
const EN = { SELECT: 0, ALGO: 1, PRESETS: 2, K1: 3, K2: 4, K3: 5, K4: 6 };
let fails = 0;
const check = (what, ok) => { console.log(`flyfish emu: ${what.padEnd(60)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
const str = (p) => { const m = new Uint8Array(w.memory.buffer); let s = ""; while (m[p]) s += String.fromCharCode(m[p++]); return s; };

w._initialize();
w.web_nor_erase();
w.web_boot();
let peak = 0;
const run = (sec) => {
  for (let i = 0; i < (sec * 44100) / 512; i++) {
    w.web_render(512);
    for (const v of new Float32Array(w.memory.buffer, w.web_out_l(), 512)) peak = Math.max(peak, Math.abs(v));
  }
};
const press = (b) => { w.web_buttons(1 << b); run(0.05); w.web_buttons(0); run(0.05); };

run(2);
check(`boots Flyfish ${str(w.web_ff_version())} on HOME, stopped`, w.web_ff_page() === 0 && !w.web_playing());
check("silent while stopped", peak < 1e-3);

press(B.PLAY);
peak = 0;
run(8);
check(`PLAY plays (peak ${peak.toFixed(3)}, bar ${w.web_ff_bar()})`, w.web_playing() && peak > 0.02 && peak <= 1 && w.web_ff_bar() >= 4);
check("PLAY's green LED lit", (w.web_lit_buttons() >> 14) & 1);

const pages = [];
for (const b of [B.EDIT, B.SEQ, B.SEQ, B.SEQ, B.ARP, B.GLO, B.FX]) { press(b); pages.push(w.web_ff_page()); }
check(`EDIT SEQ x3 ARP GLO FX open their pages (${pages.join(" ")})`, new Set(pages).size === 7 && !pages.includes(0));
press(B.HOME);
run(0.5);
check("HOME returns", w.web_ff_page() === 0);

press(B.PLAY);
run(4);
peak = 0;
run(1);
check(`PLAY again stops (tail peak ${peak.toFixed(4)})`, !w.web_playing() && peak < 1e-3);

const writes = w.web_flash_writes_count();
w.web_enc(EN.K1, 2);   // RUBATO
run(3);
check("a setting changed while stopped reaches the flash", w.web_flash_writes_count() > writes);

console.log(fails ? `flyfish emu: ${fails} FAILED` : "flyfish emu: all passed");
process.exit(fails ? 1 : 0);
