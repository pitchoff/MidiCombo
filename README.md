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
style default, so hand-written files can be short. `program` is the GM
program number counted from 0 (for drums, the drum kit number).

## Styles

Pop Rock, Ballad, Swing Jazz, Bossa Nova, Funk, Waltz (3/4), Reggae,
Shuffle Blues. Styles are defined as step patterns in `src/Style.cpp`; the
format is documented in `src/Style.h`.

## Build

```
sudo apt install cmake g++ qt6-base-dev libasound2-dev
cmake -S . -B build
cmake --build build -j
ctest --test-dir build      # chord parser/detector tests
./build/midicombo
```

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
