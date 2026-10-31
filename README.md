# MidiCombo

A background band for musicians on Linux: drums, bass, piano/comp and pad,
played through the ALSA sequencer. Qt 6 user interface.

## Modes

**Chord Progression**: type a progression (or pick a preset) and the band
loops it.

```
| Dm7 | G7 | Cmaj7 | % |        # '%' repeats the previous bar
| Bbmaj7 Gm7 | Cm7 F7 |          # two chords share a bar evenly
| C / / G7 |                     # '/' repeats the previous chord (3+1 beats)
```

Supported qualities: `C Cm Cdim Caug Csus2 Csus4 C5 C7 Cmaj7 Cm7 Cm7b5 Cdim7
CmMaj7 C6 Cm6 C7sus4 Cadd9 C9 Cmaj9 Cm9 C7#5 C7b9`, plus slash chords (`C/E`).

**Live MIDI Chords**: connect a keyboard. The notes you play in the chord
zone (by default below C4) are analysed, and the band follows the detected
chord. The chord stays latched after you release the keys. MIDI thru can
forward your playing to channel 4 so you can hear yourself.

## Preset files

**File ▸ Save Preset As…** (Ctrl+S) stores the current song (style, tempo,
swing, transpose, progression and band settings) as XML. **File ▸ Open
Presets…** (Ctrl+O) loads a file. Its presets are added to the preset
list and the first one is applied. A file can hold several presets:

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

Everything except `<progression>` is optional. A missing value keeps the
style default, so hand-written files can be short. While a preset is
selected, switching the style only changes the patterns and keeps the
preset's swing and band settings. `program` is the GM
program number counted from 0 (for drums, the drum kit number).

## Styles

Pop Rock, Ballad, Swing Jazz, Bossa Nova, Funk, Waltz (3/4), Reggae,
Shuffle Blues. They are defined as step patterns in `src/Style.cpp`.
More styles come from style files or the style editor.

### Variations

A style can have variations: alternative drum, bass and comp patterns, so
the band doesn't repeat the same bars for the whole song. All built-in
styles have a variation **B**, and every example style has a named one,
such as **Walking**, **Montuno**, **Breakdown**, **Half-Time** or **Chorus**.

**Variation** in the toolbar chooses what the band plays:

- **Auto: in turn** (the default) goes through the main pattern and the
  variations in order, every 8 bars. **every … bars** sets the distance,
  rounded up to whole patterns.
- **Auto: random** picks a different one each time.
- **Main** or a variation's name stays on that pattern.

The label next to it shows what is playing, and a change takes effect at
the next bar. Presets store this setting. Styles without variations
disable the control.

### Style editor

**Window ▸ Style Editor…** (Ctrl+E) opens a separate window to change a
style or create a new one, and to hear the result while you edit. The
main window stays usable, so the band can keep playing.

The window, from top to bottom:

- **Edit** chooses the style to load: any built-in or loaded style.
  **New** starts from a simple rock beat, and **Revert** discards the
  changes since the last load or apply. Unapplied changes are shown with
  `*` in the title, and you're asked before they are discarded.
- **Style**: name, tempo, beats per bar, steps per beat (quarters, eighths,
  triplets, sixteenths, sextuplets or 12), number of bars, swing, and the
  crash cymbal on phrase starts.
- **Instruments**: program and velocity for comp and bass, close or power
  chords for the comp, and the pad program and whether the pad is on by
  default.
- **Pattern** chooses what the grids show: the main pattern or a variation.
  **Add Variation** copies the pattern shown, so you only change what
  should differ. **Rename…** and **Remove** act on the open variation.
  Switch off **Own drums**, **Own bass** or **Own comp** to let that part
  play the main pattern; the Fill tab has the same switch.
- **Groove** tab: the drum grid with the bass and comp tracks below it,
  step by step in line. Bars are marked by heavy lines and bold numbers.
- **Fill** tab: a one-bar drum grid played on the last bar of each phrase.
  Switching it on starts it from the first bar of the groove.

Editing the patterns:

- **Drums** are a grid with one row per instrument. Click a step to set or
  clear it, shift-click for an accent, right-click to cycle the velocity
  (normal, accent, soft, ghost), drag to paint. With the keyboard, the
  arrow keys move, `x` `X` `o` `g` set a step, Space toggles and `.` or Del
  clears. Click an instrument name to change, move, clear or remove it;
  "+ add drum…" adds a row (kit pieces, a percussion submenu, or any GM
  note).
- **Bass** and **comp** are tracks: each note is a bar that lasts until
  the next note or rest. Click a step and type a token (bass
  `R 3 5 6 7 8 A`, comp `C c 1`–`9`), `-` to hold, `.` or Del for a rest;
  the cursor moves on as you type. Drag a note to change its length,
  double-click to add or remove a note, right-click for the list of tokens.
- Changing the meter keeps the steps that fit the new grid, notes keep
  their length, and extra bars repeat the existing ones. Going to a coarser
  grid drops the steps in between; **Revert** brings them back.

Trying and keeping a style:

- **Test** (Ctrl+T) plays the edited style with the band right away, with
  its tempo, swing and instruments, without changing the style list or the
  main window's settings. Edits are heard from the next bar. Pressing Test
  again, Stop in the main window, or closing the editor goes back to the
  selected style; if Test started the band, it stops it too. Test plays
  the pattern open in the editor and follows **Pattern**. The chords
  come from the current mode, so it works with the progression or a live
  keyboard.
- **Apply** (Ctrl+Return) puts the style into the style list and selects
  it, so the band plays it at once. Apply it under a new name to make a
  copy; using the name of another style replaces that style after asking.
- **Save to File…** applies the style and writes it to a style file,
  which is loaded again on the next start. Saving into an existing style
  file keeps the other styles in it, but the file is rewritten, so XML
  comments are lost.
- Applied but unsaved styles, including changed built-in styles, last
  until you quit.

A typical session: pick a style that is close to what you want under
**Edit**, give it a new name, press **Test**, change the grid and tracks
until it sounds right, then **Save to File…**.

How the editor maps to the XML, and what exactly it writes, is described
in [syntax.md](syntax.md#the-style-editor).

### Style files

More styles can be loaded from XML with **File ▸ Load Styles…** (Ctrl+L).
Loaded files are reloaded automatically on the next start. **File ▸ Forget
Loaded Styles** goes back to the built-in set. A loaded style with the same
name as an existing one replaces it, so built-in styles can be overridden.

`examples/styles/` contains Country Two-Step, Country Train Beat, Cha-Cha,
Samba, 12/8 Slow Blues, 6/8 Folk, Disco, Hard Rock, Heavy Metal, Heavy Metal
(Double Bass) and Symphonic Rock (installed to `share/midicombo/styles`).

```xml
<midicombo-styles version="1">
    <style name="Country Two-Step">
        <meter beats="4" steps-per-beat="2" bars="2"/>
        <tempo>160</tempo>
        <swing amount="20" unit="8"/>        <!-- percent; unit 8 or 16 -->
        <crash drum="crash"/>                <!-- drum="none": no crash -->
        <drums>
            <row drum="closed-hat">XoXo XoXo | XoXo XoXo</row>
            <row drum="snare">     ..X. ..X. | ..X. ..X.</row>
            <row note="36">        X... X... | X... X...</row>
        </drums>
        <fill>                               <!-- optional, exactly one bar -->
            <row drum="snare">..x. xxXX</row>
        </fill>
        <bass program="33" velocity="100">R-.. 5-.. | R-.. 5-A-</bass>
        <comp program="25" velocity="80"> ..C. ..C. | ..C. ..C.</comp>
        <pad program="22" enabled="false"/>
    </style>
</midicombo-styles>
```

- Every pattern has `beats × steps-per-beat × bars` steps. `steps-per-beat`
  must divide 12: 2 = eighths, 3 = triplets (12/8, 6/8), 4 = sixteenths.
  Whitespace and `|` are ignored.
- Drums: `X` accent, `x` normal, `o` soft, `g` ghost, `.` rest. A row names
  its instrument with `note="38"` or `drum="snare"`. The names are kick,
  side-stick, snare, clap, closed-hat, pedal-hat, open-hat, low/mid/high-tom,
  floor-tom, crash, ride, ride-bell, splash, china, tambourine, cowbell,
  high/low-bongo, high/low/mute-conga, high/low-timbale, high/low-agogo,
  cabasa, shaker, short/long-guiro, claves, high/low-woodblock and triangle.
  The full list is in `src/StyleFile.cpp`.
- Bass: `R` root, `3` `5` `6` `7` `8` chord degrees, `A` chromatic approach
  to the next chord, `-` hold, `.` rest.
- Comp: `C`/`c` full chord loud/soft, `1`–`9` single chord tones (arpeggio),
  `-` hold, `.` rest. `<comp voicing="power">` plays power chords (root,
  fifth, octave in guitar range) instead of close-position chords.
- `program` is the GM program number counted from 0. A missing `<bass>` or
  `<comp>` makes that part rest.
- Errors are reported with the line number, e.g. *"bass pattern has 7 steps,
  expected 8"*.

## Build

```
sudo apt install cmake g++ qt6-base-dev libasound2-dev
cmake -S . -B build
cmake --build build -j
ctest --test-dir build      # chord parser/detector tests
./build/midicombo
```

`cmake --install build` (default prefix `/usr/local`) installs the program,
the example styles, a desktop entry and the application icon, so MidiCombo
appears in the application menu. The icon source is `icons/midicombo.svg`;
the PNG sizes are rendered from it with
`inkscape midicombo.svg -w N -h N -o midicombo-N.png`.

## Sound

MidiCombo produces MIDI, not audio. Start a General MIDI synth, e.g.:

```
fluidsynth -a pipewire -m alsa_seq -is /usr/share/sounds/sf2/FluidR3_GM.sf2
```

then choose it as **Output** in the app (press **Refresh** if it is not listed).
The app selects FluidSynth/TiMidity automatically on the first start.

MIDI channels: 1 piano/comp, 2 bass, 3 pad, 4 MIDI thru, 10 drums.

## Source layout

| File | Purpose |
|---|---|
| `Chord`, `ChordDetector` | chord model, parsing, detection from held notes |
| `Progression` | progression text format and presets |
| `Style` | band style patterns |
| `BandEngine` | real-time sequencer thread (12 ticks per beat, swing, voice leading) |
| `MidiEngine` | ALSA sequencer client, ports and connections |
| `MainWindow`, `Widgets` | Qt user interface |
| `StyleEditor` | style editor window: drum grid, step tracks, test and apply |
| `StyleFile`, `PresetFile` | reading and writing the XML style and preset files |
| `icons/` | application icon (SVG source, rendered PNGs) and desktop entry |
