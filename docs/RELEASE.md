# Flyfish 1.1.1

Chopin's Étude Op. 10 No. 1, the "Waterfall", on the M-VAVE FM-1: played, shaped, practised and moved.

- The whole étude, 79 bars, on a sampled Steinway D (bitKlavier Grand, CC BY 4.0), with a resonator of
  sympathetic strings under the pedal; PLEYEL voices it after Chopin's own piano.
- ROUNDS: once, up to 16 times, or endless through a seam you will not hear (bar 48 into bar 1).
- Perform: RUBATO, TOUCH, ACCENT, PEDAL; a STYLE sets a whole reading (CLASSIC, ROMANTIC, VIRTUOSO, NOCTURNE);
  LEGATO, SOFT, TUNING (equal; Kirnberger III or Vallotti for contrast), PITCH (A = 440, 435, 430).
- Practise: SPEED, a loop, a hand left out for you (FREE, TAP, WAIT), RAMP, CLICK, COUNT; RHYTHM drills, CHORDS,
  and a chart of each bar's best clean speed and wrong keys.
- Move it: KEY, MODE, BASS. The notes are always Chopin's.
- The screen: the waterfall or a one-bar score, the harmony in Roman numerals.
- MIDI out with the sustain and soft pedals, to play a real piano.
- In the browser: the same firmware on [Felucca](https://github.com/hugelton/Felucca)'s emulator.

New in 1.1: newer FM-1 units, whose flash is a UC25HQ80 (JEDEC 0xB36014), now save their settings and practice
chart; before, they ran from RAM only and lost everything at power-off (Felucca's fix for its issue #320).

New in 1.1.1: every key plays one note. Felucca's own keyboard was still on next to Flyfish's: each key also played
its raw note on the first track and out on MIDI, an extra piano note on every tap in TAP and on every key in WAIT. It
now stays silent.

Since 1.0: WIDTH 5 by default (1.0.1), safe across the 27-hour clock wrap (1.0.2), TOUCH 30 by default (1.0.3).

Runs on the FM-1. To go back: Felucca's or [Tuna](https://github.com/delirioux/tuna)'s installer, or the official V15.
