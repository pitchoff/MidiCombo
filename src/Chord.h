#pragma once

#include <optional>
#include <string>
#include <vector>

namespace mc {

enum class Quality {
    Major, Minor, Dim, Aug, Sus2, Sus4, Power,
    Dom7, Maj7, Min7, HalfDim7, Dim7, MinMaj7, Six, Min6, Dom7Sus4,
    Add9, Dom9, Maj9, Min9, Aug7, Dom7b9,
    Count
};

struct QualityInfo {
    const char *suffix;               // canonical suffix used for display
    std::vector<int> intervals;       // semitones above the root, ascending
    std::vector<const char *> aliases; // accepted spellings when parsing
    int complexity;                   // used to prefer simple chords on ties
};

const QualityInfo &qualityInfo(Quality q);

struct Chord {
    int root = -1;                    // pitch class 0..11, -1 = no chord
    Quality quality = Quality::Major;
    int bass = -1;                    // slash bass pitch class, -1 = root

    bool isValid() const { return root >= 0; }
    std::string name() const;
    const std::vector<int> &intervals() const { return qualityInfo(quality).intervals; }

    // Intervals (semitones) used by the bass line generator.
    int third() const;
    int fifth() const;
    int seventh() const;

    Chord transposed(int semitones) const;

    bool operator==(const Chord &o) const
    {
        return root == o.root && (root < 0 || (quality == o.quality && bass == o.bass));
    }
    bool operator!=(const Chord &o) const { return !(*this == o); }
};

std::optional<Chord> parseChord(const std::string &text);
std::string pitchClassName(int pc);
std::string noteName(int midiNote);   // 60 -> "C4"
int parseNoteName(const std::string &text); // "C4" -> 60, -1 on error

} // namespace mc
