#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The étude for the firmware: data/op10no1.txt (Chopin's notes, public domain) -> firmware/src/ff_etude.inc.

  python3 tools/ff_etude.py            write firmware/src/ff_etude.inc
  python3 tools/ff_etude.py --check    exit 1 when it is not up to date
  python3 tools/ff_etude.py --chords   print each bar's chord as the screen names it

What it writes (flash, about 10 KB):
  FF_NOTES[]        every note: its sixteenth (0 .. 79 x 16 - 1), length in sixteenths, MIDI note and hand
  FF_FIRST[]        for each sixteenth, the first note at or after it (the player walks the table by step)
  FF_PEDAL[]        where the pedal changes: where a held bass note starts (Chopin's Ped. marks follow the bass)
  FF_RESO[][6]      for each pedal, the strings the resonator rings: the bass and the harmony's tones
  FF_TOP[]          each two-bar wave's highest right-hand note (its sixteenth): the wave's crest, accented
  FF_CHORD[]        each bar's chord when its notes make one cleanly: root, quality, bass (0xFF: none shown)
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "data/op10no1.txt"
OUT = ROOT / "firmware/src/ff_etude.inc"
BARS = 79
STEPS = BARS * 16
PC = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "G#": 8, "A": 9, "A#": 10, "B": 11}
NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
QUALITIES = [("MAJOR", (0, 4, 7)), ("MINOR", (0, 3, 7)), ("7", (0, 4, 7, 10)), ("MAJ7", (0, 4, 7, 11)),
             ("M7", (0, 3, 7, 10)), ("DIM7", (0, 3, 6, 9)), ("M7B5", (0, 3, 6, 10)), ("7B9", (0, 1, 4, 7, 10)),
             ("DIM", (0, 3, 6)), ("AUG", (0, 4, 8)), ("9", (0, 2, 4, 7, 10))]


def read():
    notes = []
    for ln, line in enumerate(SRC.read_text(encoding="utf-8").splitlines(), 1):
        line = line.strip()
        if line.startswith("#"):
            continue
        if not line:
            continue
        m = re.fullmatch(r"(\d+)\.(\d+)\s+([RL])\s+([A-G]#?)(-?\d)\s+(\d+)", line)
        if not m:
            raise SystemExit(f"{SRC}:{ln}: cannot read {line!r}")
        bar, six, hand, name, octv, length = m.groups()
        bar, six, length = int(bar), int(six), int(length)
        if not (1 <= bar <= BARS and 0 <= six < 16 and 1 <= length <= 64):
            raise SystemExit(f"{SRC}:{ln}: out of range")
        midi = (int(octv) + 1) * 12 + PC[name]
        notes.append(((bar - 1) * 16 + six, length, midi, 0 if hand == "R" else 1))
    notes.sort(key=lambda n: (n[0], n[3], n[2]))
    return notes


def chord_of(notes, bar):
    """the bar's chord, when its notes make one exactly (else None): (root, quality, bass)"""
    s0 = (bar - 1) * 16
    held = [n for n in notes if n[3] == 1 and n[1] >= 8 and n[0] <= s0 < n[0] + n[1]]
    for span in (16, 12, 8):
        ev = [n for n in notes if s0 <= n[0] < s0 + span and not (n[3] == 1 and n[1] >= 8)] + held
        if not ev:
            return None
        pcs = sorted({n[2] % 12 for n in ev})
        bass = min(n[2] for n in ev) % 12
        best = None
        for r in range(12):
            for q, (_, iv) in enumerate(QUALITIES):
                if sorted((r + i) % 12 for i in iv) == pcs:
                    score = (2 if r == bass else 0) + (1 if len(iv) == 3 else 0)
                    if best is None or score > best[0]:
                        best = (score, r, q)
        if best:
            return best[1], best[2], bass
    return None


def chord_name(c):
    r, q, b = c
    qn = QUALITIES[q][0]
    s = NAMES[r] + (" " + qn if qn in ("MAJOR", "MINOR") else qn)
    return s + (" / " + NAMES[b] if b != r else "")


def build():
    notes = read()
    first, k = [], 0
    for s in range(STEPS + 1):
        while k < len(notes) and notes[k][0] < s:
            k += 1
        first.append(k)
    pedal = sorted({n[0] for n in notes if n[3] == 1 and n[1] >= 8})
    if not pedal or pedal[0] != 0:
        pedal = [0] + pedal
    reso = []
    for i, p in enumerate(pedal):
        end = pedal[i + 1] if i + 1 < len(pedal) else STEPS
        ev = [n for n in notes if p <= n[0] < end]
        bass = min((n[2] for n in ev), default=36)
        while bass < 36:                          # (the resonator's lines reach 43 Hz: up an octave)
            bass += 12
        pcs = []
        for n in sorted(ev, key=lambda n: (n[0], n[2])):
            if n[2] % 12 not in pcs and n[2] % 12 != bass % 12:
                pcs.append(n[2] % 12)
        strings = [bass, bass + 12]
        for pc in pcs[:4]:
            strings.append(60 + pc)
        reso.append((strings + [-1] * 6)[:6])
    tops = []
    for w in range(0, STEPS, 32):
        rh = [n for n in notes if n[3] == 0 and w <= n[0] < w + 32]
        tops.append(max(rh, key=lambda n: (n[2], -n[0]))[0] if rh else 0xFFFF)
    chords = [chord_of(notes, b) for b in range(1, BARS + 1)]
    return notes, first, pedal, reso, tops, chords


def text():
    notes, first, pedal, reso, tops, chords = build()
    L = ["/* SPDX-License-Identifier: GPL-3.0-only */",
         "/* generated by tools/ff_etude.py from data/op10no1.txt (Chopin's notes, public domain): do not edit */",
         f"#define FF_BARS {BARS}",
         f"#define FF_STEPS {STEPS}",
         f"#define FF_NOTE_N {len(notes)}",
         f"#define FF_PEDAL_N {len(pedal)}",
         f"#define FF_WAVES {len(tops)}",
         "typedef struct { uint16_t step; uint8_t len, nh; } ff_note;   /* nh: the MIDI note, bit 7 the left hand */",
         "static const ff_note FF_NOTES[FF_NOTE_N] = {"]
    row = []
    for s, ln, n, h in notes:
        row.append(f"{{{s},{ln},{n | h << 7}}}")
        if len(row) == 8:
            L.append("    " + ", ".join(row) + ",")
            row = []
    if row:
        L.append("    " + ", ".join(row) + ",")
    L.append("};")
    L.append("static const uint16_t FF_FIRST[FF_STEPS + 1] = {")
    for i in range(0, len(first), 16):
        L.append("    " + ", ".join(str(v) for v in first[i:i + 16]) + ",")
    L.append("};")
    L.append("static const uint16_t FF_PEDAL[FF_PEDAL_N] = {")
    for i in range(0, len(pedal), 16):
        L.append("    " + ", ".join(str(v) for v in pedal[i:i + 16]) + ",")
    L.append("};")
    L.append("static const int8_t FF_RESO[FF_PEDAL_N][6] = {")
    for r in reso:
        L.append("    {" + ", ".join(str(v) for v in r) + "},")
    L.append("};")
    L.append("static const uint16_t FF_TOP[FF_WAVES] = {")
    for i in range(0, len(tops), 16):
        L.append("    " + ", ".join(str(v) for v in tops[i:i + 16]) + ",")
    L.append("};")
    L.append("/* each bar's chord: root, quality (FF_QUALITY), bass; 0xFF: none shown */")
    L.append("static const char *const FF_QUALITY[] = {" + ", ".join(f'"{q}"' for q, _ in QUALITIES) + "};")
    L.append("static const uint8_t FF_CHORD[FF_BARS][3] = {")
    for i in range(0, BARS, 8):
        L.append("    " + ", ".join("{%d, %d, %d}" % c if c else "{255, 255, 255}" for c in chords[i:i + 8]) + ",")
    L.append("};")
    return "\n".join(L) + "\n"


def main():
    if "--chords" in sys.argv:
        _, _, _, _, _, chords = build()
        for b, c in enumerate(chords, 1):
            print(b, chord_name(c) if c else "-")
        return 0
    t = text()
    if "--check" in sys.argv:
        return 0 if OUT.exists() and OUT.read_text() == t else 1
    OUT.write_text(t)
    print(f"{OUT.relative_to(ROOT)}: {len(build()[0])} notes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
