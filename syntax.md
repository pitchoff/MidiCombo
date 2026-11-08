# MidiCombo File Syntax

This document describes the formats MidiCombo reads and writes:

1. [Chord symbols](#1-chord-symbols)
2. [Chord progressions](#2-chord-progressions): the text in the progression editor and in songs
3. [Song files](#3-song-files): `<midicombo-songs>`
4. [Style files](#4-style-files): `<midicombo-styles>`, including the
   [style editor](#the-style-editor) and how it writes them
5. [Reference tables](#5-reference-tables), including the
   [installed files](#56-installed-files) of the Debian package
6. [MIDI import](#6-midi-import): how `midicombo-import` turns a MIDI file
   into a style file and a song file

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
| `[Name]` | plays the style variation *Name* from this bar on, see [below](#variation-markers) |

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

Errors (unknown chord, `/` or `%` with nothing before it, an unclosed
`[`, empty progression) are shown below the editor with their line number.

### Variation markers

A marker in square brackets chooses the style variation (see
[Variations](#variations)) for the following bars, so a song can have a
verse on the main pattern and a chorus on a variation:

```
[Main]
| G | D | Em | C |
| G | D | C  | % |
[B]
| C | G | D  | Em |
| C | G | D  | %  |
```

| Marker | Meaning |
|---|---|
| `[Main]` | the main pattern |
| `[Name]` | the variation called *Name*. Names may contain spaces (`[One Drop]`). |
| `[auto]` | back to the **Variation** setting in the toolbar |

- A marker on its own line or before the first bar line applies from the
  next bar on. A marker inside a bar (`| [B] C |`) applies to that whole bar.
- A marker stays in effect until the next one. It does not wrap around:
  when the progression starts again, the first bars play what their own
  markers say, and bars before the first marker follow the toolbar setting.
- Names are matched without regard to case.
- If the style has no variation of that name, a warning is shown below
  the editor and those bars follow the toolbar setting. The progression
  view shows the marker on each bar where the variation changes.
- Markers apply in the Chord Progression mode only. While the style
  editor tests a style, the band plays the variation open in the editor.

---

## 3. Song files

A song stores the style, tempo, swing, transpose, the chord progression
and the band settings. **File ▸ Save Song As…** writes one song. A file
can contain any number of songs, and **File ▸ Open Songs…** adds all of
them to the song list and applies the first.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<midicombo-songs version="1">
    <song name="Slow Blues">
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
    </song>
</midicombo-songs>
```

`midicombo-import` writes song files from MIDI files; they use the same
format, with every `<part>` given (see [section 6](#6-midi-import)).

Song files were called preset files in earlier versions. Files with
`<midicombo-presets>` and `<preset>` elements are still read; MidiCombo
writes the new names.

### Elements

| Element | Content | If missing |
|---|---|---|
| `<song name="…">` | display name in the song list. A song with the same name as a loaded one replaces it. | the file name is used |
| `<style>` | style name, a built-in or a loaded style (see [5.4](#54-built-in-styles)) | current style is kept |
| `<tempo>` | BPM, 30–320 | current tempo is kept |
| `<swing>` | percent, 0–100 | the style's swing |
| `<transpose>` | semitones, −11 to 11 | 0 |
| `<variation>` | how the band changes variations, see [below](#variation) | current setting is kept |
| `<progression>` | progression text ([section 2](#2-chord-progressions)), including [variation markers](#variation-markers), **required** | the progression is empty |
| `<band>` | zero or more `<part>` elements | the style's defaults |

`<tempo>`, `<swing>` and `<transpose>` must be integers, otherwise the
file is rejected. Values outside the range are clamped.

### `<part>` attributes

| Attribute | Values |
|---|---|
| `name` | `drums`, `bass`, `piano`, `pad`, `guitar`. Other names are ignored. |
| `enabled` | `true` or `false` (anything other than `true` means off) |
| `program` | GM program number counted from 0 (the user interface shows it counted from 1). For `drums`, a drum kit number ([5.3](#53-drum-kits)). Unknown numbers leave the instrument unchanged. |
| `volume` | 0–127 (MIDI CC 7) |

### `<variation>`

Optional. `<variation select="in-turn" every="8"/>` sets how the band
chooses among the style's variations:

| Attribute | Values |
|---|---|
| `select` | `in-turn`, `random`, `Main`, or the name of a variation |
| `every` | bars between automatic changes, rounded up to whole patterns |

If the style has no variation of that name, the band changes in turn.
Without the element, the current setting stays. Bars with a
[variation marker](#variation-markers) in the progression play the marked
variation instead.

### How a song is applied

1. The style is selected, which sets swing, instruments and the pad switch
   to the style's defaults.
2. Tempo and swing are set, if given.
3. Every `<part>` overrides the style defaults for that part.
4. Transpose and progression are set.

An unknown style name shows a message in the status bar and keeps the current style.

While a song is selected, choosing another style in the toolbar only
changes the patterns. Tempo, swing and the band settings of the song stay
as they are. With no song selected ("— choose a song —"), a style
change applies the style's defaults.

---

## 4. Style files

A style defines what the band plays: a drum pattern, an optional fill, a
bass line and a comp (piano/guitar) pattern on a grid of steps. **File ▸
Load Styles…** adds the styles to the style list. A style with the same
name as an existing one replaces it, which can be used to override a
built-in style. Loaded files are reloaded on the next start. Styles can
also be created and changed in the [style editor](#the-style-editor)
instead of writing the XML by hand.

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
| `<guitar>` | `program` 0–127, `velocity` 1–127, `voicing` `full`, `power` or `high`, pattern as text | program 25, velocity 80, full; no `<guitar>`: the guitar rests |
| `<pad>` | `program` 0–127, `enabled` `true` if the pad should be on by default | program 48, off |
| `<variation name="…">` | alternative patterns, any number, see [Variations](#variations) | none |

`<row>` takes the instrument as `note="38"` (GM percussion note 0–127) or
`drum="snare"` (see [5.1](#51-drum-names)), and the pattern as text.

`program` and `enabled` are only defaults: they are applied when the style
is selected while no song is active, and the user (or a song) can
change them.

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

### Guitar pattern characters

| Char | Plays | Velocity |
|---|---|---|
| `D` | down strum, all strings from the lowest | velocity + 8 |
| `d` | down strum, soft | velocity − 15 |
| `U` | up strum, the top four strings from the highest | velocity − 4 |
| `u` | up strum, soft | velocity − 22 |
| `X` | palm-muted chug: the lowest three strings, half a step long | velocity + 4 |
| `x` | palm-muted chug, soft | velocity − 10 |
| `1`–`9` | a single string: `1` is the lowest note of the voicing. Numbers above the voicing size continue an octave higher, and the notes ring two steps longer. | velocity |
| `-` | lets the strings ring one more step | |
| `.` | rest | |

The strings of a strum follow each other by 7 ms (down) or 5 ms (up), and
each string is a little softer than the one before.

Voicings (`voicing` attribute of `<guitar>`):

| Value | Notes |
|---|---|
| `full` | six-string barre chord with the root between E2 and D#3: root, fifth, octave (or the seventh or sixth of the chord), third, fifth, octave |
| `power` | root, fifth and octave with the root between E2 and D#3 |
| `high` | a three-note chord between C4 and E5, using the inversion closest to the previous chord, for off-beat skanks and funk scratching |

A strummed chord switches to a new chord straight away, like the comp.

### Pad

The pad plays the full chord (between G3 and A5) for as long as it lasts.
It isn't pattern based. It changes with every chord change.

### Phrases, fills and crash

A phrase is the progression length if that is 4 bars or more (8 bars if
the length is a multiple of 8), otherwise 8 bars. In live chord mode it
is always 8 bars. The fill replaces the drums in the last bar of each
phrase, and the crash plays on the first beat of each phrase (on the very
first bar only after a count-in).

### Variations

A `<style>` can contain any number of `<variation>` elements, after the
main pattern. Each is an alternative arrangement on the same grid:

```xml
<variation name="Chorus">
    <drums>
        <row drum="ride">  XxXx XxXx | XxXx XxXx</row>
        <row drum="snare"> ..X. ..X. | ..X. ..X.</row>
        <row drum="kick">  X.xX X.x. | X.xX X.xx</row>
    </drums>
    <bass>RR5R 8R5R | RR5R 8R5A</bass>
    <comp>C--- C--- | C--- C-C-</comp>
</variation>
```

- `name` is required and must be unique within the style. `Main`, `auto`,
  `in-turn` and `random` are reserved (in any case), and the characters
  `[`, `]` and `|` are not allowed, so that every variation can be chosen
  with a [marker in the progression](#variation-markers).
- `<drums>`, `<fill>`, `<bass>`, `<comp>` and `<guitar>` are all optional. A part that
  is missing is taken from the main pattern, so a variation can change the
  bass alone. To silence a part, write rests, because an empty `<bass>` or
  `<drums>` also means "same as main".
- Patterns follow the same rules and lengths as the main pattern. Meter,
  tempo, swing, programs, velocities and voicing are shared, and attributes
  on a variation's `<bass>` and `<comp>` are ignored.

Older versions of MidiCombo skip `<variation>` and play the main pattern.

### Characters outside these tables

Any other character in a pattern is treated as a rest.

### The style editor

**Window ▸ Style Editor…** (Ctrl+E) edits the same data as a `<style>`
element. Every control maps to one part of the syntax:

| Editor | XML |
|---|---|
| Name | `<style name>` |
| Tempo | `<tempo>` |
| Beats per bar, Steps per beat, Bars | `<meter beats steps-per-beat bars>` |
| Swing, 8ths/16ths | `<swing amount unit>` |
| Crash | `<crash drum>` (`none` or a crash, ride, splash or china cymbal) |
| Comp program, velocity, Close/Power chords | `<comp program velocity voicing>` |
| Guitar program, velocity, Full chords/Power chords/High triads | `<guitar program velocity voicing>` |
| Bass program, velocity | `<bass program velocity>` |
| Pad program, On by default | `<pad program enabled>` |
| Groove tab, drum grid | `<drums>`, one `<row>` per grid row |
| Groove tab, Bass, Comp and Guitar tracks | the text of `<bass>`, `<comp>` and `<guitar>` |
| Fill tab | `<fill>`; switched off = no `<fill>` element |

The program lists show the GM names numbered from 1, as in most synth
manuals. The file stores the program counted from 0, so "34 Electric Bass
(finger)" is written as `program="33"`.

**Drum grid.** Each cell is one step of a `<row>`:

| Action | Character |
|---|---|
| Click on an empty step | `x` |
| Click on a set step | `.` |
| Shift-click | `X` |
| Right-click | cycles `x` → `X` → `o` → `g` → `x` (an empty step becomes `X`) |
| Keys `x` `X` `o` `g`, Space, `.`/Del | that character, toggle `x`/`.`, `.` |

Click a row name to change its instrument (the `drum` or `note`
attribute), move it, clear it or remove it. "+ add drum…" adds a row of
rests, and "Other note…" chooses a GM note by number.

**Tracks.** A note is drawn as a bar that starts at its token and runs
over the following `-` steps. Typing a token writes that character, `-`
writes a hold, and `.`, Space or Del write a rest. Deleting a note also
removes its holds. Dragging a note to the right writes `-` up to the
mouse position. Dragging to the left shortens it and turns the freed
steps into `.`. A `-` with no note before it is shown as a dotted line,
because it plays nothing.

**Changing the meter** converts the patterns:

- A step that falls on the new grid keeps its character. Going from 8ths
  to 16ths turns `R-5.` into `R---5-..`.
- On a coarser grid, steps between the new steps are dropped: two beats
  of 16ths `X.x.x...` become `Xx` in quarters.
- In bass, comp and guitar tracks, a new step inside a note becomes `-`, so notes
  keep their length. 8ths to triplets turns `R-5.` into `R--5-.`.
- New beats are rests. New bars repeat the existing bars in order, and
  removed bars or beats are cut off.

The fill is always converted as one bar.

**Variations.** **Pattern** chooses what the grids show. A variation
writes one `<variation>` element, and Own drums, Own bass, Own comp, Own guitar and
"Use its own fill" decide whether that part is written. Switched off, the
part is left out and is taken from the main pattern.

**Test** (Ctrl+T) plays the edited style, in the pattern open in the
editor, without adding it to the style list. **Apply** adds it to the list or replaces the style of the same
name. **Save to File…** writes it to a style file.

**What Save to File writes.** The editor writes a normal style file that
can be edited by hand afterwards. Saving into an existing style file
replaces the style with the same name, or adds it, and keeps the other
styles. Files written by the editor always look like this:

- Every element is written, including `<crash>`, `<swing>` and `<pad>`,
  followed by the variations.
  `<fill>` is left out when the fill is off, `<guitar>` when the guitar
  track has only rests, and the comp's `voicing="power"` is written only
  for power chords. `<guitar>` always carries its `voicing`.
- Drum rows use `drum="…"` when the note has a name in
  [5.1](#51-drum-names), otherwise `note="…"`. No crash is written as
  `drum="none"`.
- Patterns are grouped with spaces by beat, or by two beats for 1 and 2
  steps per beat in even meters, and bars are separated by ` | `. For
  example: `R-RR 5-RA | R-.. 5-A-`.
- The file is rewritten completely. XML comments and the alignment of a
  hand-written file are not kept.

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

Used as `program` of the `drums` part in songs. Availability depends on
the synthesizer and soundfont.

| Program | Kit | Program | Kit |
|---|---|---|---|
| 0 | Standard | 25 | TR-808 |
| 8 | Room | 32 | Jazz |
| 16 | Power | 40 | Brush |
| 24 | Electronic | 48 | Orchestra |

### 5.4 Built-in styles

`Pop Rock`, `Ballad`, `Swing Jazz`, `Bossa Nova`, `Funk`, `Waltz`, `Reggae`,
`Shuffle Blues`, `Minuet` (variations `Moving`, `Lute`), `Sarabande`
(variation `Double`).

Example style files in `examples/styles/` (installed to
`/usr/share/midicombo/styles/`, see [5.6](#56-installed-files)):
`country.xml`, `latin.xml`, `blues-and-folk.xml`, `blues-rock.xml`,
`disco.xml`, `glam-and-sleaze.xml`, `grunge.xml`, `hard-rock.xml`,
`heavy-metal.xml`, `jamaican.xml`, `symphonic-rock.xml`.

### 5.5 MIDI channels

| Channel | Part |
|---|---|
| 1 | piano / comp |
| 2 | bass |
| 3 | pad |
| 4 | MIDI thru (live mode) |
| 5 | guitar |
| 10 | drums |

### 5.6 Installed files

`./make-deb.sh` builds the Debian package `bin/midicombo_<version>_<arch>.deb`
(install it with `sudo apt install ./bin/midicombo_0.1_amd64.deb`).
The package, like `cmake --install` with the prefix `/usr`, installs:

| Path | Content |
|---|---|
| `/usr/bin/midicombo` | the program |
| `/usr/bin/midicombo-import` | makes a style file and a song file from a MIDI file, see [section 6](#6-midi-import) |
| `/usr/share/midicombo/styles/` | the example style files. **File ▸ Load Styles…** starts here until another folder has been used. |
| `/usr/share/doc/midicombo/` | `README.md` and this document, `syntax.md` |
| `/usr/share/applications/midicombo.desktop` | the application menu entry |
| `/usr/share/icons/hicolor/*/apps/midicombo.*` | the icon, 16–256 px and SVG |

The installed example styles are read-only. To change one, load it, edit it
in the style editor and use **Save to File…** to write a copy to your home
directory.

MidiCombo keeps its settings, including the list of loaded style files, in
`~/.config/MidiCombo/MidiCombo.conf`. Removing the package leaves this file
and your own song and style files in place. If a loaded style file no
longer exists, the next start shows a message in the status bar and drops
it from the list.

---

## 6. MIDI import

```
midicombo-import [options] file.mid
```

reads a Standard MIDI File and writes a style file ([section 4](#4-style-files))
and a song file ([section 3](#3-song-files)) that uses it. Load the style
file in MidiCombo first, then the song file. The files are a starting
point: the patterns can be tidied in the style editor and the chords in
the progression editor.

### Options

| Option | Meaning | Default |
|---|---|---|
| `-o`, `--output-dir DIR` | directory for the two files | the current directory |
| `-n`, `--name NAME` | name of the style and of the song | the title in the file, else the file name |
| `-b`, `--bars N` | pattern length, 1–8 bars | 2 |
| `-s`, `--steps N` | steps per beat, a divisor of 12 | detected |
| `--variations N` | most variations besides the main pattern, 0–24 | 2 |
| `--drums CH`, `--bass CH`, `--comp CH`, `--guitar CH`, `--pad CH` | MIDI channel 1–16 of that part, `0` = the file has none | detected |
| `-f`, `--force` | overwrite existing output files | existing files are an error |
| `-q`, `--quiet` | print only errors | a report of what was found |

The exit status is 0 on success, 1 if a file can't be read or written or
an option is unknown, and 2 for a wrong option value or a missing MIDI
file name.

### Output files

The file names come from the MIDI file's name, in lower case with every run
of other characters than letters and digits replaced by `-`:
`Rebel Yell.mid` gives `rebel-yell-style.xml` and `rebel-yell-song.xml`.
Both contain one entry with the same name, so the song finds its style.

### What is read from the MIDI file

| MIDI | Use |
|---|---|
| format 0 and 1 | read. Format 2 and files with SMPTE time code are rejected. |
| note on / note off | notes with their length. A note on with velocity 0 ends a note; notes never released end with their track. |
| program change | the first one of each channel gives the instrument (for channel 10, the drum kit) |
| tempo | the tempo that lasts longest |
| time signature | the first one; later changes are reported and ignored |
| name of the first track | the title, used as the name if it has at least two letters |

Everything else (controllers, pitch bend, lyrics, SysEx) is skipped.

### Meter, tempo and grid

- *n*/8 with *n* = 6, 9, 12 … becomes *n*/3 beats of a dotted quarter
  (`steps-per-beat` 3 or 6); every other meter keeps its numerator as the
  beats, with the denominator's note value as the beat.
- The tempo is converted to beats per minute of that beat and limited to
  30–320.
- The grid is chosen from where the drum, bass, comp and guitar notes
  start: eighths (2 steps) if nearly all fall on eighths, triplets (3) if
  they fit triplets better than sixteenths, else sixteenths (4).
  `--steps` overrides this.
- On an eighth or sixteenth grid, off-beats that come late on average give
  `<swing>`: 0 % for straight, 100 % for triplet timing, in steps of 5 %,
  ignored below 15 %. The unit is 8 on an eighth grid and 16 on a
  sixteenth grid.

### Which channel plays what

Each channel gets one role. Channels set with an option come first; the
others are assigned in this order, each role taking the free channel with
the most notes that qualifies:

| Role | Channel |
|---|---|
| drums | channel 10 |
| bass | program 32–39 (basses) and played below middle C; else the lowest channel that plays mostly single notes, below D3 on average |
| guitar | program 24–31 (guitars), playing chords (1.5 or more notes at a time on average) |
| pad | program 48–55 (strings, choir) or 88–95 (synth pads), playing chords |
| comp | program 0–23 (pianos, mallets, organs), playing chords; else any chord channel with notes shorter than two beats |
| pad | any remaining chord channel with notes of two beats or longer |
| chords only | the other channels that play chords: they help to find the chords but get no pattern |
| melody | channels playing single notes: ignored |

A second drum channel is ignored. If no channel plays chords, the chords
are guessed from the bass.

### Chords and the progression

The song runs from the first to the last bar in which bass or chord
channels play, so a drum count-in or a drum intro is left out.

For every bar the notes of the bass and chord channels are weighted by how
long they sound. The chord that explains them best is chosen from these
qualities: major, minor, `7`, `maj7`, `m7`, `sus4`, `sus2`, `dim`,
`m7b5`, `dim7`, `aug` and `5`:

- chord tones count for it, other notes count against it, a missing third
  or seventh costs more than a missing fifth, and the bass note on the
  first half beat being the root helps;
- `5`, `sus2` and `sus4` win only where the third is really missing;
- the previous chord stays if it explains the notes nearly as well.

A bar gets one chord, or two (one per half, in meters with an even number
of beats above 2), or one per beat, whichever explains the notes best
after a cost for every chord change inside the bar. Bars before the first
chord take that chord. Slash chords are not written; the style's bass
pattern plays the bass line.

The progression has four bars per line, with `/` for a chord repeated
inside a bar. If the style got variations, every section starts with a
[variation marker](#variation-markers) on its own line:

```
[Main]
| B5 | B5 | B5 | B5 |
| F#m | F#m | Gmaj7 | Gmaj7 / D A5 |
[B]
| D | D | G | G Dsus2 |
```

### Main pattern, variations and fill

The song is cut into blocks of `--bars` bars from its first bar; a shorter
rest at the end is not used for patterns. Two blocks are similar when at
least 80 % of their onsets match: each drum note per step, and the
rhythm (not the notes) of bass, comp and guitar. With patterns of two bars
or more, the last bar of a block is left out of this comparison, so that a
fill does not make a block different.

- The block with the most similar blocks starts the **main pattern**
  group; the next ones start the variation groups **B**, **C** …, up to
  `--variations`. A variation needs at least two blocks; one-off blocks
  join the most similar group.
- Each group's patterns come from its most typical block: the one most
  similar to the others.
- A block whose last bar has less than 70 % of its drum onsets in common
  with the group's usual last bar ends in a **fill**. The fill that occurs
  most often becomes `<fill>`; with one-bar patterns there is no fill.
- In a variation, parts that are the same as in the main pattern are left
  out, so they are taken from the main pattern. A variation in which the
  drums rest gets a silent kick row.
- A crash cymbal (notes 49 and 57) on the first step of the pattern is
  removed and played by `<crash drum="crash"/>` at phrase starts instead.
  If the file has no crash on the first step of a bar, `<crash drum="none"/>`
  is written.

### Pattern characters

Notes are put on the nearest step. A note that starts less than half a
step before the pattern belongs to its first step.

**Drums.** One `<row>` per drum note, cymbals (higher notes) first and the
kick last. The velocities are scaled so that the loudest tenth of the
hits reaches 115, then:

| Velocity | Char |
|---|---|
| 108 and up | `X` |
| 80–107 | `x` |
| 55–79 | `o` |
| below 55 | `g` |

**Bass.** The lowest note starting on a step is compared to the root of
the chord at that point:

| Semitones above the root | Char |
|---|---|
| 0, 1 | `R` (`8` if an octave or more above the lowest root in the pattern) |
| 2–5 | `3` |
| 6–8 | `5` |
| 9 | `6` |
| 10, 11 | `7` |

A note other than the root that is a half step from the root of the next,
different chord becomes `A`. A note is held with `-` until it ends or the
next note starts.

**Comp.** Two or more notes on a step are a chord: `C` if they are at
least as loud as the channel's median velocity, `c` if softer, held with
`-` while they sound. A single note becomes `1`–`9`: the number of the
nearest chord tone (`1` root, `2` third, `3` fifth, `4` seventh), plus the
number of chord tones (at most 4) from G4 up.

**Guitar.** Two or more notes on a step are a strum. Short chords (0.6 of a
step or less) below A3 are palm-muted chugs, `X`; the others are `D` or
`U`, by the order of the notes (low to high is a down strum) or, if they
start together, by the position (on the beat or an even step is down).
Strums more than 8 velocity steps below the channel's median are written
in lower case (`x`, `d`, `u`). A single note becomes the string `1`–`6`,
counting from E2 in steps of a fourth. The voicing is `power` for chords of
about three notes below middle C, `high` for small chords from D4 up, and
`full` otherwise. Without a guitar channel, the style has no `<guitar>`.

### Instruments and the song

| Style value | From |
|---|---|
| `program` of `<bass>`, `<comp>`, `<guitar>`, `<pad>` | the program of that channel, else the style defaults 33, 0, 25, 48 |
| `velocity` of `<bass>`, `<comp>`, `<guitar>` | the median velocity of that channel, limited to 30–120 |
| `<pad enabled>` | `true` if a pad channel was found |
| `<comp>`, `<bass>` without a channel | rests |

The song file gets the name, the style, the tempo and the swing, the
transpose 0 and the progression. It has no `<variation>` element; the
markers in the progression choose the variations. `<band>` lists all five
parts: a part is enabled if its channel was found, with that channel's
program (for the drums, the drum kit), and the volume 100.
