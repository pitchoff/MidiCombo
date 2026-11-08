#pragma once

#include "Chord.h"

#include <string>
#include <vector>

namespace mc {

// A looping chord progression. Each bar holds one or more equally long
// chord slots ("| C G |" = two beats each in 4/4).
struct Progression {
    std::vector<std::vector<Chord>> bars;
    // The style variation to play in each bar, same size as `bars`:
    // "Main", a variation name, or empty to follow the variation setting.
    std::vector<std::string> variations;

    bool empty() const { return bars.empty(); }
    int size() const { return static_cast<int>(bars.size()); }

    // tickInBar may run past the bar end; it then wraps into the next bars.
    Chord chordAt(int bar, int tickInBar, int ticksPerBar) const;
    // The variation requested for a bar (bar loops like in chordAt).
    const std::string &variationAt(int bar) const;
    // The variation names used, each once, in order of appearance.
    std::vector<std::string> variationNames() const;
    Progression transposed(int semitones) const;

    // Text format: bars separated by '|', chords separated by spaces.
    // "/" or "." repeats the previous chord, "%" repeats the previous bar.
    // A line without '|' is read as one chord per bar. Lines starting
    // with '#' are comments. "[Name]" plays the style variation Name from
    // its bar on, "[Main]" the main pattern, "[auto]" follows the variation
    // setting again.
    static bool parse(const std::string &text, Progression &out, std::string &error);
};

// A song that comes with the program.
struct BuiltinSong {
    const char *name;
    const char *style;
    int tempo;
    const char *text;
};

const std::vector<BuiltinSong> &builtinSongs();

} // namespace mc
