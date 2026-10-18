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
| `ff_player` | the performance: time, dynamics, rubato, pedal, the loop, WAIT and TAP, the variations, key and mode |
| `ff_ui` | the screen and the controls, in Tuna's frame, on Felucca's `gfx.c` |
| `ff_sounds.h` | the eight sounds on Felucca's engines, and how the cards set a track |
| `ff_fw.c` | the glue: the ISR, the panel, the LEDs, the settings in flash, the splash |
| `tools/ff_logo.py` | the splash logo into `ff_logo.inc` |
| `tests/ff_player_test.c` | the player: every note once and in order, note-offs, the loop and ramp, WAIT, TAP, the variations, key, bass, pedal |
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
- **One piano, both hands on one track.** With a track for each hand, a key struck by one hand while the other's
  still rang (the left hand's C4 under the right's, a sixteenth apart) sounded twice: two strings for one key, a
  doubled bass. On one track Felucca restarts the voice of the same note, as a piano's key. RIGHT and LEFT are the
  hands' velocities; WIDTH spreads the voices left and right. The keys (playing along) keep their own track.
- **A deep bass.** The held bass well above the figures, RIGHT at 75 and LEFT at 100, its Steinway notes kept 3.1 s
  (a two-bar whole note at 176 lasts 2.7 s). Measured on the host render: the bass below 160 Hz 2 dB nearer the rest
  than in 0.2, sounding in 99 % of the quarter seconds (was 88 %); no note twice, no voice stolen.
- **A Steinway.** GRAND is a Steinway D: Princeton's bitKlavier Grand (Piano Bar mic image, CC BY 4.0), 13 notes
  from C1 to C7 every six semitones (a note never moves more than three semitones; Felucca's Kawai: five notes, six),
  velocity layer 12 of 16, kept by register: 3.1 s in the deep bass down to 0.7 s at the top, where a Steinway's high notes
  have died away anyway. 271 KB of ADPCM (Felucca's sets: 143 KB). Without Tuna's flute and sax, their indexes are
  aliases of the piano. The second layer (v8) is downloaded but not used: Felucca's SAMPLE engine has no velocity
  layers; the matrix's VEL -> AMP does the dynamics.
- **The variations keep Chopin's harmony.** Each half bar's own notes give the pool; a figure walks it on the right
  hand's rhythm. The random numbers are mulberry32, as in the browser simulator.
- **The keys only play.** FREE plays along, TAP makes the player the rhythm, WAIT holds the music for the right
  note (any octave, since the right hand reaches E7 and the keys stop at G5); the key LEDs show the note to find,
  else the right hand running across the keys.
- **No libm, no doubles**: floats on the FPU (`-mcpu=r3 -mfprev1`), `sinf` and `lrintf` from `tuna_libm.c`.
- **Settings save themselves while stopped**, two seconds after the last change, in Felucca's first project
  object (the flash is the code's: writing it while playing would stall the audio).

## Measured on the host

- The performance at the score's tempo: 110 s (114 s rendered with the tail); every note sounded and damped.
- Through Felucca's engines (GRAND, PEDAL SCORE): the 8 voices always busy, 434 of 1,337 notes cut short by newer
  ones, peak -4.5 dBFS, nothing clipped. The mix costs about the same per sample as Tuna's piano pieces on this
  host; the real number comes from the FM-1.

## Plan

1. **On the FM-1.** First CI build (the flash and RAM report), install on `dev`, check: the splash, the keys and
   knobs, the timing at 176, the CPU with the resonator, the settings surviving a power cycle.
2. **Steinway, done in 0.2.** Next, if the flash allows: longer notes, a second velocity layer (the SAMPLE
   engine would need velocity zones), the set's release resonance and pedal noises.
3. **The browser.** Flyfish in Felucca's browser emulator (`web/emu`, as Tuna's Try page), and a site with the
   installer.
4. **The simulator in step.** The browser simulator predates the firmware: it lacks the fifteen re-struck bass
   notes, and its sound names differ (WATER, GLASS, VIBES there; KALIMBA, MARIMBA, SINE KEY here).
5. **More pieces?** Only if Flyfish earns it: Op. 10 has eleven more.
