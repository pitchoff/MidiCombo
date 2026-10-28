# MidiCombo File Syntax

This document describes the formats MidiCombo reads:

1. [Chord symbols](#1-chord-symbols)
2. [Chord progressions](#2-chord-progressions): the text in the progression editor and in presets
3. [Preset files](#3-preset-files): `<midicombo-presets>`
4. [Style files](#4-style-files): `<midicombo-styles>`
5. [Reference tables](#5-reference-tables)

Both XML formats are UTF-8, currently version `1`. Files with a higher
`version` are rejected. Unknown elements are skipped, so newer files with
extra elements still load in older versions.

---

## 1. Chord symbols

```
<root><accidentals><quality>[/<bass>]
```

| Part | Syntax | Examples |
|---|---|---|
| root | `A`–`G` (lower case accepted) | `C`, `g` |
| accidentals | any number of `#` (sharp) or `b` (flat) | `F#`, `Bb`, `Ebb` |
| quality | one of the spellings below, case-sensitive | `m7`, `maj7`, `sus` |
| bass | `/` followed by a note name (slash chord) | `C/E`, `Am7/G` |

| Quality | Accepted spellings | Tones (semitones above root) |
|---|---|---|
| major | *(empty)*, `M`, `maj` | 0 4 7 |
| minor | `m`, `min`, `-` | 0 3 7 |
| diminished | `dim`, `o`, `°` | 0 3 6 |
| augmented | `aug`, `+` | 0 4 8 |
| suspended 2nd | `sus2` | 0 2 7 |
| suspended 4th | `sus4`, `sus` | 0 5 7 |
| power chord | `5` | 0 7 |
| dominant 7th | `7`, `dom7` | 0 4 7 10 |
| major 7th | `maj7`, `M7`, `Maj7`, `^7`, `^` | 0 4 7 11 |
| minor 7th | `m7`, `min7`, `-7` | 0 3 7 10 |
| half-diminished | `m7b5`, `ø`, `ø7`, `h7`, `-7b5` | 0 3 6 10 |
| diminished 7th | `dim7`, `o7`, `°7` | 0 3 6 9 |
| minor-major 7th | `mMaj7`, `mM7`, `m(maj7)`, `-maj7` | 0 3 7 11 |
| sixth | `6` | 0 4 7 9 |
| minor sixth | `m6`, `min6`, `-6` | 0 3 7 9 |
| 7 sus 4 | `7sus4`, `7sus` | 0 5 7 10 |
| add 9 | `add9`, `add2` | 0 4 7 14 |
| dominant 9th | `9` | 0 4 7 10 14 |
| major 9th | `maj9`, `M9` | 0 4 7 11 14 |
| minor 9th | `m9`, `min9`, `-9` | 0 3 7 10 14 |
| augmented 7th | `7#5`, `aug7`, `+7` | 0 4 8 10 |
| 7 flat 9 | `7b9` | 0 4 7 10 13 |

Chords are always displayed with the first spelling and the root names
`C C# D Eb E F F# G Ab A Bb B`.

---

## 2. Chord progressions

A progression is plain text. It loops while the band plays.

| Syntax | Meaning |
|---|---|
| `\|` | bar line. Empty bars (e.g. leading or trailing `\|`) are ignored. |
| whitespace | separates chords inside a bar. All chords in a bar get the same length. |
| `/`, `.` or `-` | repeats the previous chord (also across a bar line) |
| `%` (alone in a bar) | repeats the whole previous bar |
| line without `\|` | every chord is its own bar |
| line starting with `#` | comment |

```
# 12-bar blues
| F7  | Bb7 | F7 | %  |
| Bb7 | %   | F7 | %  |
| C7  | Bb7 | F7 | C7 |

# 2+2 beats, then 3+1 beats in 4/4
| Dm7 G7 | Cmaj7 / / A7 |
# four bars
C Am F G
```

Comments must be on their own line. A `#` after a chord is read as a
chord and gives an "unknown chord" error.

Errors (unknown chord, `/` or `%` with nothing before it, empty
progression) are shown below the editor with their line number.

---

## 3. Preset files

A preset stores one song: the style, tempo, swing, transpose, the chord
progression and the band settings. **File ▸ Save Preset As…** writes one
preset. A file can contain any number of presets, and **File ▸ Open
Presets…** adds all of them to the preset list and applies the first.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<midicombo-presets version="1">
    <preset name="Slow Blues">
        <style>Shuffle Blues</style>
        <tempo>72</tempo>
        <swing>0</swing>
        <transpose>0</transpose>
        <progression>| F7 | Bb7 | F7 | F7 |
| Bb7 | Bb7 | F7 | F7 |
| C7 | Bb7 | F7 | C7 |</progression>
        <band>
            <part name="drums" enabled="true" program="0" volume="100"/>
            <part name="bass" enabled="true" program="33" volume="100"/>
            <part name="piano" enabled="true" program="0" volume="100"/>
            <part name="pad" enabled="false" program="16" volume="90"/>
        </band>
    </preset>
</midicombo-presets>
```

### Elements

| Element | Content | If missing |
|---|---|---|
| `<preset name="…">` | display name in the preset list. A preset with the same name as a loaded one replaces it. | the file name is used |
| `<style>` | style name, a built-in or a loaded style (see [5.4](#54-built-in-styles)) | current style is kept |
| `<tempo>` | BPM, 30–320 | current tempo is kept |
| `<swing>` | percent, 0–100 | the style's swing |
| `<transpose>` | semitones, −11 to 11 | 0 |
| `<progression>` | progression text ([section 2](#2-chord-progressions)), **required** | the progression is empty |
| `<band>` | zero or more `<part>` elements | the style's defaults |

`<tempo>`, `<swing>` and `<transpose>` must be integers, otherwise the
file is rejected. Values outside the range are clamped.

### `<part>` attributes

| Attribute | Values |
|---|---|
| `name` | `drums`, `bass`, `piano`, `pad`. Other names are ignored. |
| `enabled` | `true` or `false` (anything other than `true` means off) |
| `program` | GM program number counted from 0 (the user interface shows it counted from 1). For `drums`, a drum kit number ([5.3](#53-drum-kits)). Unknown numbers leave the instrument unchanged. |
| `volume` | 0–127 (MIDI CC 7) |

### How a preset is applied

1. The style is selected, which sets swing, instruments and the pad switch
   to the style's defaults.
2. Tempo and swing are set, if given.
3. Every `<part>` overrides the style defaults for that part.
4. Transpose and progression are set.

An unknown style name shows a message in the status bar and keeps the current style.

---

## 4. Style files

A style defines what the band plays: a drum pattern, an optional fill, a
bass line and a comp (piano/guitar) pattern on a grid of steps. **File ▸
Load Styles…** adds the styles to the style list. A style with the same
name as an existing one replaces it, which can be used to override a
built-in style. Loaded files are reloaded on the next start.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<midicombo-styles version="1">
    <style name="Country Two-Step">
        <meter beats="4" steps-per-beat="2" bars="2"/>
        <tempo>160</tempo>
        <swing amount="20" unit="8"/>
        <crash drum="crash"/>
        <drums>
            <row drum="closed-hat">XoXo XoXo | XoXo XoXo</row>
            <row drum="snare">     ..X. ..X. | ..X. ..X.</row>
            <row note="36">        X... X... | X... X...</row>
        </drums>
        <fill>
            <row drum="snare">..x. xxXX</row>
            <row drum="kick"> X... X...</row>
        </fill>
        <bass program="33" velocity="100">R-.. 5-.. | R-.. 5-A-</bass>
        <comp program="25" velocity="80" voicing="close">..C. ..C. | ..C. ..C.</comp>
        <pad program="22" enabled="false"/>
    </style>
</midicombo-styles>
```

### Elements

| Element | Attributes / content | Default |
|---|---|---|
| `<style name="…">` | name shown in the style list, **required** | |
| `<meter>` | `beats` 1–16: beats per bar | 4 |
| | `steps-per-beat`: grid resolution, must divide 12 (1, 2, 3, 4, 6, 12) | 2 |
| | `bars` 1–16: pattern length in bars | 1 |
| `<tempo>` | suggested tempo in BPM, 30–320 | 120 |
| `<swing>` | `amount` 0–100 percent (100 = triplet feel) | 0 |
| | `unit` `8` (swing eighths) or `16` (swing sixteenths) | 8 |
| `<crash>` | `drum="…"` or `note="…"`: cymbal on the first beat of every phrase. `drum="none"` or `note="0"` turns it off. | crash (49) |
| `<drums>` | `<row>` elements, see below | no drums |
| `<fill>` | `<row>` elements, exactly **one bar** long. Replaces the drums in the last bar of every phrase. | no fills |
| `<bass>` | `program` 0–127 (default instrument), `velocity` 1–127, pattern as text | program 33, velocity 96, rests |
| `<comp>` | `program` 0–127, `velocity` 1–127, `voicing` `close` or `power`, pattern as text | program 0, velocity 78, close, rests |
| `<pad>` | `program` 0–127, `enabled` `true` if the pad should be on by default | program 48, off |

`<row>` takes the instrument as `note="38"` (GM percussion note 0–127) or
`drum="snare"` (see [5.1](#51-drum-names)), and the pattern as text.

`program` and `enabled` are only defaults: they are applied when the style
is selected, and the user (or a preset) can change them.

### The step grid

- Every pattern in `<drums>`, `<bass>` and `<comp>` has exactly
  **beats × steps-per-beat × bars** steps. `<fill>` rows have **beats ×
  steps-per-beat** steps.
- Whitespace and `|` inside a pattern are ignored. Use them to show beats and bars.
- `steps-per-beat="2"` is an eighth-note grid, `3` eighth-note triplets
  (12/8, 6/8 and shuffles), `4` sixteenths.
- For compound meters, count the dotted quarter as the beat: 6/8 is
  `beats="2" steps-per-beat="3"`, 12/8 is `beats="4" steps-per-beat="3"`.

A wrong length is reported with its counts, for example
*"Style starting at line 12: Samba: bass pattern has 15 steps, expected 16"*.

### Drum pattern characters

| Char | Meaning | Velocity |
|---|---|---|
| `X` | accent | 118 |
| `x` | normal | 95 |
| `o` | soft | 70 |
| `g` | ghost note | 42 |
| `.` | rest | |

Velocities vary by ±5 for a human feel. Drum notes are released after one step.

### Bass pattern characters

| Char | Plays |
|---|---|
| `R` | root, or the bass note of a slash chord |
| `3` | third (for sus chords the 4th/2nd, for power chords the fifth) |
| `5` | fifth (diminished or augmented if the chord has one) |
| `6` | major sixth |
| `7` | seventh of the chord (minor 7th if the chord has none, diminished 7th for dim7) |
| `8` | octave above the root |
| `A` | chromatic approach: a half step below or above the root (or slash bass) of the chord that follows this note, whichever is closer to the previous bass note |
| `-` | holds the previous note one more step |
| `.` | rest |

A note lasts until the next step that isn't `-`. Roots are placed between
A1 and G#2 (MIDI 33–44), other tones above the root. The first note of a
bar is played 8 velocity steps louder.

### Comp pattern characters

| Char | Plays | Velocity |
|---|---|---|
| `C` | full chord, accented | velocity + 8 |
| `c` | full chord, soft | velocity − 15 |
| `1`–`9` | a single chord tone: `1` is the lowest note of the voicing, `2` the next, and so on. Numbers above the voicing size continue an octave higher. The notes ring two steps longer and overlap, for arpeggios. | velocity |
| `-` | holds the previous chord one more step | |
| `.` | rest | |

Voicings (`voicing` attribute of `<comp>`):

| Value | Notes |
|---|---|
| `close` | close-position chord of up to four notes starting from E3, using the inversion closest to the previous chord. Five-note chords drop the fifth. |
| `power` | root, fifth and octave with the root between E2 and D#3, for distorted guitars |

If the chord changes while bass or comp notes are still sounding, they
switch to the new chord straight away (arpeggio comps just stop).

### Pad

The pad plays the full chord (between G3 and A5) for as long as it lasts.
It isn't pattern based. It changes with every chord change.

### Phrases, fills and crash

A phrase is the progression length if that is 4 bars or more (8 bars if
the length is a multiple of 8), otherwise 8 bars. In live chord mode it
is always 8 bars. The fill replaces the drums in the last bar of each
phrase, and the crash plays on the first beat of each phrase (on the very
first bar only after a count-in).

### Characters outside these tables

Any other character in a pattern is treated as a rest.

---

## 5. Reference tables

### 5.1 Drum names

| Name | Note | Name | Note | Name | Note |
|---|---|---|---|---|---|
| `kick` | 36 | `crash` | 49 | `high-conga` | 63 |
| `side-stick` | 37 | `high-tom` | 50 | `low-conga` | 64 |
| `snare` | 38 | `ride` | 51 | `high-timbale` | 65 |
| `clap` | 39 | `china` | 52 | `low-timbale` | 66 |
| `electric-snare` | 40 | `ride-bell` | 53 | `high-agogo` | 67 |
| `floor-tom` | 41 | `tambourine` | 54 | `low-agogo` | 68 |
| `closed-hat` | 42 | `splash` | 55 | `cabasa` | 69 |
| `high-floor-tom` | 43 | `cowbell` | 56 | `shaker` | 70 |
| `pedal-hat` | 44 | `crash-2` | 57 | `short-guiro` | 73 |
| `low-tom` | 45 | `ride-2` | 59 | `long-guiro` | 74 |
| `open-hat` | 46 | `high-bongo` | 60 | `claves` | 75 |
| `mid-tom` | 47 | `low-bongo` | 61 | `high-woodblock` | 76 |
| `high-mid-tom` | 48 | `mute-conga` | 62 | `low-woodblock` | 77 |
| | | | | `mute-triangle` | 80 |
| | | | | `triangle` | 81 |

Other GM percussion notes can be used with `note="…"`.

### 5.2 Common GM programs (counted from 0)

| Program | Instrument | Program | Instrument |
|---|---|---|---|
| 0 | Acoustic Grand Piano | 32 | Acoustic Bass |
| 4 | Electric Piano 1 | 33 | Electric Bass (finger) |
| 7 | Clavinet | 34 | Electric Bass (pick) |
| 16 | Drawbar Organ | 36 | Slap Bass 1 |
| 18 | Rock Organ | 38 | Synth Bass 1 |
| 22 | Harmonica | 48 | String Ensemble 1 |
| 24 | Acoustic Guitar (nylon) | 49 | String Ensemble 2 |
| 25 | Acoustic Guitar (steel) | 52 | Choir Aahs |
| 27 | Electric Guitar (clean) | 61 | Brass Section |
| 29 | Overdriven Guitar | 89 | Pad 2 (warm) |
| 30 | Distortion Guitar | | |

All 128 programs are listed in the instrument menus of the app (counted from 1 there).

### 5.3 Drum kits

Used as `program` of the `drums` part in presets. Availability depends on
the synthesizer and soundfont.

| Program | Kit | Program | Kit |
|---|---|---|---|
| 0 | Standard | 25 | TR-808 |
| 8 | Room | 32 | Jazz |
| 16 | Power | 40 | Brush |
| 24 | Electronic | 48 | Orchestra |

### 5.4 Built-in styles

`Pop Rock`, `Ballad`, `Swing Jazz`, `Bossa Nova`, `Funk`, `Waltz`, `Reggae`,
`Shuffle Blues`.

Example style files in `examples/styles/`: `country.xml`, `latin.xml`,
`blues-and-folk.xml`, `disco.xml`, `hard-rock.xml`, `heavy-metal.xml`,
`symphonic-rock.xml`.

### 5.5 MIDI channels

| Channel | Part |
|---|---|
| 1 | piano / comp |
| 2 | bass |
| 3 | pad |
| 4 | MIDI thru (live mode) |
| 10 | drums |
