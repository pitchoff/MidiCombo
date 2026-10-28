#pragma once

#include "Chord.h"

#include <string>
#include <vector>

namespace mc {

// A looping chord progression. Each bar holds one or more equally long
// chord slots ("| C G |" = two beats each in 4/4).
struct Progression {
    std::vector<std::vector<Chord>> bars;

    bool empty() const { return bars.empty(); }
    int size() const { return static_cast<int>(bars.size()); }

    // tickInBar may run past the bar end; it then wraps into the next bars.
    Chord chordAt(int bar, int tickInBar, int ticksPerBar) const;
    Progression transposed(int semitones) const;

    // Text format: bars separated by '|', chords separated by spaces.
    // "/" or "." repeats the previous chord, "%" repeats the previous bar.
    // A line without '|' is read as one chord per bar. Lines starting
    // with '#' are comments.
    static bool parse(const std::string &text, Progression &out, std::string &error);
};

struct ProgressionPreset {
    const char *name;
    const char *style;
    int tempo;
    const char *text;
};

const std::vector<ProgressionPreset> &progressionPresets();

} // namespace mc
