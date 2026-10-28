#pragma once

#include <string>
#include <vector>

namespace mc {

// One drum instrument (GM note on channel 10) and its step pattern.
// Step characters: 'X' accent, 'x' normal, 'o' soft, 'g' ghost, '.' rest.
struct DrumRow {
    int note;
    std::string steps;
};

// A band style. All patterns use the same step grid (stepsPerBeat) and
// span `bars` bars, except `fill`, which is exactly one bar long.
//
// Bass steps: 'R' root (or slash bass), '3' third, '5' fifth, '6' sixth,
//   '7' seventh, '8' octave, 'A' chromatic approach to the next chord,
//   '-' hold the previous note, '.' rest.
// Comp steps: 'C' chord accented, 'c' chord soft, '1'..'9' single chord
//   tone (arpeggio, wraps into the next octave), '-' hold, '.' rest.
struct Style {
    std::string name;
    int beatsPerBar = 4;
    int stepsPerBeat = 2;   // must divide 12
    int bars = 1;
    int tempo = 120;
    double swing = 0.0;     // 0 = straight, 1 = full triplet swing
    int swingUnit = 8;      // swing 8th notes (8) or 16th notes (16)
    int crashNote = 49;     // crash at phrase starts, 0 = none

    std::vector<DrumRow> drums;
    std::vector<DrumRow> fill;
    std::string bass;
    std::string comp;
    bool powerChords = false; // comp plays root, fifth and octave in guitar range

    int pianoProgram = 0;
    int bassProgram = 33;
    int padProgram = 48;
    bool padDefault = false;
    int bassVelocity = 96;
    int compVelocity = 78;

    int stepsPerBar() const { return beatsPerBar * stepsPerBeat; }
    int stepCount() const { return stepsPerBar() * bars; }
};

const std::vector<Style> &builtinStyles();
// Removes the whitespace and '|' that patterns may contain for readability.
Style normalizedStyle(Style style);
bool validateStyle(const Style &style, std::string &error);

} // namespace mc
