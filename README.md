# Flyfish

<p align="center"><img src="docs/flyfish-logo.png" alt="Flyfish" width="256"></p>

[![License: GPL-3.0-only](https://img.shields.io/badge/license-GPL--3.0--only-blue.svg)](LICENSE)

One piece on the M-VAVE FM-1: Chopin's Étude in C major, Op. 10 No. 1, the "Waterfall". Flyfish plays it, lets you
shape it, practise it and move it. Built on [Tuna](https://github.com/delirioux/tuna) and
[Felucca](docs/FELUCCA.md).

## Why this piece

Because of the film *Youri*. It opens with this étude, and Flyfish began the evening after watching it: to have
that music in a pocket synth, to play it, to learn it, to make it fall like water.

> **Version 0.1, in development: not yet tried on an FM-1.** Built for fun and with no warranty: Felucca's or
> Tuna's installer, or the official V15, takes the FM-1 back.

<p align="center"><img src="docs/flyfish-home.gif" alt="HOME: the étude falling as droplets" width="240"></p>

## Four ways to hear one piece

- **Play.** Press PLAY: all 79 bars, from the first C to the last.
- **Perform.** RUBATO lets the pulse breathe with each two-bar wave; TOUCH sets how far the dynamics move;
  ACCENT brings out Chopin's accents and each wave's crest; PEDAL is the score's (a new pedal where the bass
  changes), HALF, DRY or WASH. The pedal's ring is a resonator of sympathetic strings tuned to the harmony.
- **Practise.** SPEED down to 25 %, a loop of any bars (REC), RAMP to speed up a little every round, a CLICK and a
  bar's count-in. Leave a hand out (YOU) and play it yourself: FREE along with the music, TAP (each key plays the
  next note: you are the rhythm) or WAIT (the music waits for the right note, any octave; the keys light up).
  Practise it the way teachers drill it: RHYTHM in dotted pairs (LONG-SHORT, SHORT-LONG) or with the accent on the
  2nd, 3rd or 4th note of each group, and CHORDS, each group of four as one chord, to learn the hand's shapes before
  the run. In WAIT, a chart keeps each bar's best clean SPEED and its wrong keys, from SPEED 25 % to 100 %.
- **Move it.** The notes are always Chopin's. Move them to another KEY, play them in MINOR, DORIAN or LYDIAN,
  change the BASS, and let it go ROUNDS: 1 to 16 times or INF (endless), through a seam you will not hear (bar 48
  runs into bar 1 as it runs into the recapitulation, which is bar 1 again).
- **Chopin's piano.** A STYLE sets a whole reading at once: CLASSIC, ROMANTIC, VIRTUOSO or NOCTURNE. TUNING plays it
  in equal temperament or in the well temperaments of Chopin's time (Werckmeister III, Kirnberger III, Vallotti), at
  A = 440, 435 (the diapason normal of 1859) or 430 Hz; SOFT is the una corda; PLEYEL voices the Steinway after the
  piano Chopin played. The screen names the harmony in Roman numerals (V7/V, viio7/ii), and can show the bar as a
  score (MENU: VIEW).

Nine sounds on Felucca's engines: GRAND (a Steinway D, sampled), TINE EP, HARP, CELESTA, MUSIC BOX, KALIMBA, MARIMBA,
SINE KEY, PLEYEL (the Steinway voiced darker, quicker and lighter: a voicing, not a recording of a Pleyel); three spaces of reverb (CATHEDRAL at 25 % to start); ten colours.

## The controls

| | |
| --- | --- |
| SELECT | tempo (the score: ♩ = 176) |
| MASTER | volume |
| PRESETS | the sound, on any page |
| ALGORITHM | a bar to go to (playing: at the next bar line) |
| KNOB 1-4 | the four cards on the screen |
| PLAY | start / stop |
| REC | the loop FROM .. TO on and off |
| OCT- / OCT+ | a bar back / on |
| HOME | the waterfall (or the score); hold it: the menu (COLOR LOOK VIEW PITCH) |
| EDIT | sound: SOUND BRIGHT DECAY WIDTH |
| SEQ | practice: SPEED FROM TO YOU; again: MODE RAMP CLICK COUNT; again: RHYTHM CHORDS SHOW ERASE (the chart; turned twice) |
| ARP | variations: KEY MODE BASS ROUNDS |
| GLO | mix: RIGHT LEFT REVERB SPACE |
| FX | piano: STYLE, LEGATO (how far each note of the figures overlaps the next: 0 none, 100 three notes; best heard with PEDAL DRY), SOFT, TUNING |
| keys | music only: playing along, tapping, finding the note |

MIDI out: the right hand on channel 1, the left on 2, the keys on 3; the sustain pedal (CC64, lifted where the bass
changes and put down again 30 ms later) and the soft pedal (CC67) on 1 and 2, so Flyfish can play a real piano. The
temperament and pitch stay in Flyfish's own sound. The settings save themselves while stopped.

## The notes

[`data/op10no1.txt`](data/op10no1.txt) holds the étude as Chopin wrote it: 1,337 notes, pitches and rhythms only,
one a line, checked against a public-domain score. The dynamics, the pedal and the rubato are Flyfish's own reading
of the score's markings (f, cresc., dim., the hairpins, the accents, Ped.), not anyone's recording.

## Building

As Tuna (see [BUILDING.md](BUILDING.md)), with `FELUCCA_FLYFISH=1 ./build.sh`. GitHub Actions builds every push to
`main` and `dev` and keeps the package as the run's artifact `flyfish-firmware`; install it with an installer's
"Install a test build" (Tuna's site has one). `sh tests/ff_tests.sh` runs the host tests;
`sh tests/ff_felucca.sh OUTDIR` renders the étude through Felucca's engines to a WAV.

## Credits and licence

GPL-3.0-only. Flyfish by delirioux, written with Claude; a fork of Tuna (delirioux), a fork of Felucca by Leo
Kuroshita, Hügelton Instruments. Chopin's notes are in the public domain; the piano is a Steinway D from Princeton's
bitKlavier Grand Sample Library by Daniel Trueman (CC BY 4.0). See [CREDITS.md](CREDITS.md) and [LICENSING.md](LICENSING.md).
