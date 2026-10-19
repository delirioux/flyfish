# Developing Flyfish

The working notes behind Flyfish: the agreements, how the work flows, why the code is shaped as it is, where it
stands and what comes next. What Flyfish is and how to play it is in the [README](../README.md).

Status: 18 October 2026. Version 0.1 written and tested on the host; not yet built by CI or tried on an FM-1.

## Agreements

| What | Agreed |
| --- | --- |
| Home | GitHub, `github.com/delirioux/flyfish`, branch `main`; unfinished work on `dev` until it has run on the FM-1. |
| Upstream | A fork of [Tuna](https://github.com/delirioux/tuna) (itself a fork of [Felucca](https://github.com/hugelton/Felucca)). Never push to Tuna or Felucca. Felucca's five engines are a submodule, `firmware/engines`. |
| Name | Ocean animals, as Tuna and Espada: the flying fish, its arpeggios leaping across four octaves. The wordmark says FLYFISH; the piece is WATERFALL. |
| Language | English throughout: code, comments, commits, docs. |
| Licence | GPL-3.0-only. Chopin's notes are public domain; only pitches and rhythms are used, nothing of anyone's MIDI performance or edition (velocities, tempo maps, pedalling): those are often CC BY-SA, which a GPL-3.0 release cannot carry. |
| Commits | Author `delirioux <340923392+delirioux@users.noreply.github.com>`. Work done with Claude ends with the `Co-Authored-By` and `Claude-Session` lines. |
| Repository contents | The firmware, its tests and tools, and its plan (this file). Chat discussions stay out. |
| Controls | The keys only ever play music; settings, bars and pages are on knobs and buttons. |

## How changes reach GitHub

As Tuna and Espada: Claude works in a sandbox that cannot push and delivers git bundles; they are pushed with the
sync script (`FM1-Sync.sh` on Linux in `~/fm1`, the PowerShell script on Windows). A push to `dev` builds a test
package (Actions > the run > Artifacts > `flyfish-firmware`), installed with an installer's "Install a test build".

## Where Flyfish lives

Flyfish's files are `firmware/src/ff_*`, `tests/ff_*`, `tools/ff_*` and `data/`. From Tuna: `tuna_reso` (the
resonator), `tuna_libm.c` and `tuna_sys.h`. Everything else is Felucca's, touched only where Flyfish hooks in
(`felucca.c`, `main.c`, `audio.c`, `panel.c`, `gfx.c`: `FELUCCA_FLYFISH`, where Tuna had `FELUCCA_TUNA`;
`tools/build.py`; `tools/gen_samples.py`, the longer piano).

| File | Does |
| --- | --- |
| `data/op10no1.txt` | the étude: 1,337 notes, one a line, readable and checkable bar by bar |
| `tools/ff_etude.py` | the notes into `ff_etude.inc`: the notes, an index by sixteenth, the pedal changes, the resonator's strings, each wave's crest, each bar's chord |
| `ff_player` | the performance: time, dynamics, rubato, pedal, the loop, WAIT and TAP, key and mode, ROUNDS, LEGATO |
| `ff_ui` | the screen and the controls, in Tuna's frame, on Felucca's `gfx.c` |
| `ff_sounds.h` | the eight sounds on Felucca's engines, and how the cards set a track |
| `ff_fw.c` | the glue: the ISR, the panel, the LEDs, the settings in flash, the splash |
| `tools/ff_logo.py` | the splash logo into `ff_logo.inc` |
| `tests/ff_player_test.c` | the player: every note once and in order, note-offs, the loop and ramp, WAIT, TAP, key, bass, pedal, ROUNDS, LEGATO |
| `tests/ff_ui_test.c` | the screen: the controls, every page in every palette, Tuna's layout lint, PNGs and a GIF |
| `tests/ff_felucca.c` | the étude through Felucca's real engines to a WAV: voices, the limiter, the resonator, the cost |

## Decisions and why

- **The notes from the score, the playing our own.** The notes came out of a MIDI file and were checked against a
  public-domain score; only pitches and rhythms were kept. Held bass notes struck again (the same key repeated
  while held) needed care: a first pass lost fifteen of them.
- **Dynamics from the markings.** f at the start, cresc. at 33, f 35, dimin. 36, cresc. 42, f 45, dim. 48, the
  coda's hairpins, dim. into the end; each two-bar wave rising louder and falling softer; Chopin's accents on each
  group's first note. Felucca's SAMPLE engine takes no velocity, so the matrix's last slot carries VEL to AMP. The
  levels sit the loudest bars just under Felucca's master limiter: above it the dynamics were flattened.
- **Rubato breathes with the waves**: pushing up, a breath at the crest, easing at the wave's end; the last two
  bars broaden; the fermata holds.
- **The pedal: a long release, a held bass and a resonator.** Felucca has 8 voices for everything; a pedalled
  arpeggio wants twenty. The figures let go of their keys at once and ring on in a long release (the pedal's REL),
  so when the voices run out a released figure gives way (voice.c takes released voices first, and never a track's
  lowest held note). Only the held bass keeps its key down, to the next pedal change: it always sounds. The pedal's
  ring comes from Tuna's resonator, its strings tuned to the bass and harmony of each pedal (from the score,
  `FF_RESO`).
- **One piano, split by pitch, in stereo.** With a track for each hand, a key struck by one hand while the other's
  still rang (the left hand's C4 under the right's, a sixteenth apart) sounded twice: two strings for one key. The
  piano is now split by pitch instead: below middle C one track, panned left; from C4 up another, panned right, as
  heard from the bench. A note always plays on one track, so Felucca restarts its voice when it is struck again, and
  the arpeggios sweep from left to right and back. WIDTH sets how far apart; RIGHT and LEFT are the hands'
  velocities. (Felucca's PAN is -64 .. 63 around 0: 0.1 and 0.2 put 64 and up in it, everything hard right.)
  The samples stay mono: stereo would take twice the flash.
- **WIDTH 5 by default (1.0.1).** The arpeggios cross middle C all the time, so with the two halves far apart every
  crossing hopped from one side to the other: on the FM-1 that wobble sounded like a honky-tonk. Two tracks cannot pan
  by pitch smoothly, so the halves stay close.
- **A deep bass.** The held bass well above the figures, (0.2: RIGHT at 75 and LEFT at 100; 0.3: both 65), its Steinway notes kept 3.1 s
  (a two-bar whole note at 176 lasts 2.7 s). Measured on the host render: the bass below 160 Hz 2 dB nearer the rest
  than in 0.2, sounding in 99 % of the quarter seconds (was 88 %); no note twice, no voice stolen.
- **A Steinway.** GRAND is a Steinway D: Princeton's bitKlavier Grand (Piano Bar mic image, CC BY 4.0), 13 notes
  from C1 to C7 every six semitones (a note never moves more than three semitones; Felucca's Kawai: five notes, six),
  velocity layer 12 of 16, kept by register: 3.1 s in the deep bass down to 0.7 s at the top, where a Steinway's high notes
  have died away anyway. 271 KB of ADPCM (Felucca's sets: 143 KB). Without Tuna's flute and sax, their indexes are
  aliases of the piano. The second layer (v8) is downloaded but not used: Felucca's SAMPLE engine has no velocity
  layers; the matrix's VEL -> AMP does the dynamics.
- **Only Chopin's notes.** 0.1 .. 0.2 had VARY: generated right-hand figures on each half bar's notes. They did not
  sound like Chopin and 0.3 removed them; KEY, MODE and BASS move or thin the score's own notes. The saved record
  keeps their four bytes (`unused_vary`) so older settings still load.
- **The keys only play.** FREE plays along, TAP makes the player the rhythm, WAIT holds the music for the right
  note (any octave, since the right hand reaches E7 and the keys stop at G5); the key LEDs show the note to find,
  else the right hand running across the keys.
- **Practising as teachers do.** RHYTHM changes the sixteenths' lengths in pairs (3:1 or 1:3, the bar's length
  kept) or moves the accent within each group; CHORDS gives each group of four as one chord on its first
  sixteenth (each pitch once, the held bass at its own step), so WAIT asks for the hand's shape.
- **The progress chart.** Only WAIT with a hand left out counts: a bar played from its line to its last sixteenth
  with no wrong key keeps the best SPEED it was played at; each wrong key counts against its bar. Its own record
  ("FFP1") in Felucca's second project object, saved when the player stops (never while playing).
- **Temperaments as cents, tuned to A.** EQUAL is the default. Kirnberger III and Vallotti (18th-century well
  temperaments, a contrast rather than a claim about Chopin's tuner; 0.4's first build also had Werckmeister III, a
  Baroque one, removed as out of place) as offsets from equal
  temperament, shifted so A stays at 0 (the PITCH stays the pitch). They reach each voice as its `fine` pitch
  (1/4096 of the phase increment); Felucca's SAMPLE engine ignored `fine` (it steps in 1/16 semitones), so
  `eng_sample.c` now applies it, which also makes TUNE's residual below 1/16 semitone exact on samples. PITCH is
  Felucca's TUNE (cents). The resonator's strings follow both (`tr_cents` in `tuna_reso.c`). Measured on the host:
  C4 in Werckmeister III at 430 sat 25 cents under equal at 440 (expected 28; the window mixes partials).
- **STYLE presets, then the player's own.** A STYLE writes tempo, rubato, touch, accent, pedal and legato; turning
  any of them by hand makes it OWN again, so the card never claims a reading that is not playing.
- **PLEYEL is a voicing.** The same Steinway samples, darker (the insert's low-pass 16 lower), a 38 % quicker decay
  and a lighter VEL -> AMP: in the spirit of an 1840s Pleyel, not a recording of one. SOFT (una corda) is velocity
  x 0.82 and the low-pass 12 lower.
- **The score on 240 pixels.** One bar on two staves, 6 px between lines; a group of four shifts as one under 8va
  or 15ma (above C6) or 8vb (below E2), as the printed score does, so no ledger tower climbs out of the panel.
- **The pedals over MIDI.** CC64 is lifted where Flyfish's pedal changes and put down 30 ms later (a legato pedal
  a piano on the far end can follow), 64 for HALF; CC67 follows SOFT. Temperament and pitch are not sent (that
  would take MIDI Tuning messages most pianos ignore).
- **No libm, no doubles**: floats on the FPU (`-mcpu=r3 -mfprev1`), `sinf` and `lrintf` from `tuna_libm.c`.
- **Settings save themselves while stopped**, two seconds after the last change, in Felucca's first project
  object (the flash is the code's: writing it while playing would stall the audio).

## Measured on the host

- The performance at the score's tempo: 110 s (114 s rendered with the tail); every note sounded and damped.
- Through Felucca's engines (GRAND, PEDAL SCORE): the 8 voices always busy, 434 of 1,337 notes cut short by newer
  ones, peak -4.5 dBFS, nothing clipped. The mix costs about the same per sample as Tuna's piano pieces on this
  host; the real number comes from the FM-1.

## Flash

The app's code and constants run from flash (XIP, `firmware/app.ld`): 568 KB. The Steinway takes 243 KB of it
(22.6 s of 4-bit ADPCM at 22.05 kHz). This container cannot fetch JieLi's toolchain, so 1.0's size was estimated by
compiling the same sources for RV32IMFC at -Os: 182 KB of code, 345 KB of constants (the Steinway's 243 among them)
and 22 KB for the player and the resonator, 549 KB in all (KB = 1,024 bytes): 19 KB under the 568, by another ISA's
measure, which the AC79's code can be bigger or smaller than. The CI's link is the real test (it fails if the XIP
region overflows) and its "Sizes" step lists what takes the room. If it does not fit: shorter piano notes with a
looped tail (the pedal and REL then decide how long they ring), then Felucca's parts Flyfish does not use.

## Plan

1. **On the FM-1.** The CI link and its sizes; then on the device: the splash, the keys and knobs, the timing at
   176, the CPU with the resonator, the settings and the practice chart surviving a power cycle, the MIDI pedals on
   a real piano.
2. **The piano.** If the flash allows: longer notes or a looped tail, a second velocity layer (the SAMPLE engine
   would need velocity zones), a soundboard body, a RESONANCE card.
3. **The browser.** Flyfish in Felucca's browser emulator (`web/emu`), as Tuna's Try page.
4. **More pieces?** Only if Flyfish earns it: Op. 10 has eleven more.
