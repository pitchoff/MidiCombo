#pragma once

#include "Chord.h"

#include <optional>
#include <vector>

namespace mc {

// Finds the chord that best explains a set of held MIDI notes.
// Every (root, quality) pair is scored: matching tones count positive,
// extra notes and missing tones negative (a missing fifth only a little),
// and having the root in the bass gives a bonus that resolves ambiguous
// sets such as C6 / Am7.
class ChordDetector {
public:
    std::optional<Chord> detect(const std::vector<int> &midiNotes) const;
};

} // namespace mc
