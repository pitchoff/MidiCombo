#include "Progression.h"

#include <algorithm>
#include <sstream>

namespace mc {

Chord Progression::chordAt(int bar, int tickInBar, int ticksPerBar) const
{
    if (bars.empty() || ticksPerBar <= 0)
        return {};
    bar += tickInBar / ticksPerBar;
    tickInBar %= ticksPerBar;
    const auto &slots = bars[bar % bars.size()];
    if (slots.empty())
        return {};
    const size_t idx = static_cast<size_t>(tickInBar) * slots.size() / ticksPerBar;
    return slots[std::min(idx, slots.size() - 1)];
}

Progression Progression::transposed(int semitones) const
{
    Progression p = *this;
    for (auto &bar : p.bars)
        for (auto &chord : bar)
            chord = chord.transposed(semitones);
    return p;
}

bool Progression::parse(const std::string &text, Progression &out, std::string &error)
{
    out.bars.clear();
    std::istringstream lines(text);
    std::string line;
    int lineNo = 0;

    while (std::getline(lines, line)) {
        ++lineNo;
        const auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos || line[first] == '#')
            continue;

        std::vector<std::string> segments;
        if (line.find('|') == std::string::npos) {
            std::istringstream words(line);
            std::string w;
            while (words >> w)
                segments.push_back(w);
        } else {
            std::string seg;
            std::istringstream parts(line);
            while (std::getline(parts, seg, '|'))
                segments.push_back(seg);
        }

        for (const auto &seg : segments) {
            std::istringstream words(seg);
            std::vector<std::string> tokens;
            std::string tok;
            while (words >> tok)
                tokens.push_back(tok);
            if (tokens.empty())
                continue;

            if (tokens.size() == 1 && tokens[0] == "%") {
                if (out.bars.empty()) {
                    error = "Line " + std::to_string(lineNo) + ": '%' needs a previous bar";
                    return false;
                }
                out.bars.push_back(out.bars.back());
                continue;
            }

            std::vector<Chord> slots;
            for (const auto &t : tokens) {
                if (t == "/" || t == "." || t == "-") {
                    if (!slots.empty())
                        slots.push_back(slots.back());
                    else if (!out.bars.empty())
                        slots.push_back(out.bars.back().back());
                    else {
                        error = "Line " + std::to_string(lineNo) + ": '" + t + "' needs a previous chord";
                        return false;
                    }
                    continue;
                }
                const auto chord = parseChord(t);
                if (!chord) {
                    error = "Line " + std::to_string(lineNo) + ": unknown chord '" + t + "'";
                    return false;
                }
                slots.push_back(*chord);
            }
            out.bars.push_back(std::move(slots));
        }
    }

    if (out.bars.empty()) {
        error = "The progression is empty";
        return false;
    }
    return true;
}

const std::vector<ProgressionPreset> &progressionPresets()
{
    static const std::vector<ProgressionPreset> presets = {
        {"12-Bar Blues in F", "Shuffle Blues", 100,
         "| F7  | Bb7 | F7 | F7 |\n"
         "| Bb7 | Bb7 | F7 | F7 |\n"
         "| C7  | Bb7 | F7 | C7 |\n"},
        {"Jazz ii-V-I in C", "Swing Jazz", 140,
         "| Dm7   | G7 | Cmaj7 | Cmaj7 |\n"
         "| Em7b5 | A7 | Dm7   | G7    |\n"},
        {"Rhythm Changes A (Bb)", "Swing Jazz", 180,
         "| Bbmaj7 Gm7 | Cm7 F7     | Dm7 G7 | Cm7 F7 |\n"
         "| Fm7 Bb7    | Ebmaj7 Ab7 | Dm7 G7 | Cm7 F7 |\n"},
        {"Pop I-V-vi-IV in G", "Pop Rock", 112,
         "| G | D | Em | C |\n"},
        {"Doo-Wop in C", "Ballad", 72,
         "| C | Am | F | G7 |\n"},
        {"Andalusian Cadence (Am)", "Pop Rock", 100,
         "| Am | G | F | E7 |\n"},
        {"Minor Bossa (Am)", "Bossa Nova", 130,
         "| Am7   | Am7 | Dm7 | Dm7 |\n"
         "| Bm7b5 | E7  | Am7 | E7  |\n"},
        {"Funk Vamp in E", "Funk", 100,
         "| E9 | E9 | A9 | E9 |\n"},
        {"Jazz Waltz in F", "Waltz", 132,
         "| Fmaj7 | Dm7 | Gm7 | C7 |\n"},
        {"Reggae in D", "Reggae", 76,
         "| D | G | A | G |\n"},
    };
    return presets;
}

} // namespace mc
