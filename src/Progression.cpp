#include "Progression.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace mc {

namespace {

constexpr char MarkerTag = '\x01';

std::string lower(std::string s)
{
    for (auto &c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trimmed(const std::string &s)
{
    const auto first = s.find_first_not_of(" \t");
    if (first == std::string::npos)
        return {};
    return s.substr(first, s.find_last_not_of(" \t") - first + 1);
}

// Replaces every "[name]" with a word of its own (MarkerTag + index into
// `markers`), so that names may contain spaces.
bool extractMarkers(const std::string &line, std::string &out, std::vector<std::string> &markers,
                    std::string &error)
{
    out.clear();
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == ']') {
            error = "unexpected ']'";
            return false;
        }
        if (line[i] != '[') {
            out += line[i];
            continue;
        }
        const auto end = line.find_first_of("]|[", i + 1);
        if (end == std::string::npos || line[end] != ']') {
            error = "missing ']'";
            return false;
        }
        std::string name = trimmed(line.substr(i + 1, end - i - 1));
        if (name.empty()) {
            error = "'[]' needs a variation name";
            return false;
        }
        const std::string key = lower(name);
        if (key == "auto")
            name.clear();
        else if (key == "main")
            name = "Main";
        out += ' ';
        out += MarkerTag;
        out += std::to_string(markers.size());
        out += ' ';
        markers.push_back(name);
        i = end;
    }
    return true;
}

} // namespace

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

const std::string &Progression::variationAt(int bar) const
{
    static const std::string none;
    if (variations.empty() || bar < 0)
        return none;
    return variations[bar % variations.size()];
}

std::vector<std::string> Progression::variationNames() const
{
    std::vector<std::string> names;
    for (const auto &v : variations)
        if (!v.empty() && std::find(names.begin(), names.end(), v) == names.end())
            names.push_back(v);
    return names;
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
    out.variations.clear();
    std::istringstream lines(text);
    std::string rawLine, line;
    std::vector<std::string> markers;
    std::string variation; // the variation of the following bars
    int lineNo = 0;

    while (std::getline(lines, rawLine)) {
        ++lineNo;
        const auto first = rawLine.find_first_not_of(" \t\r");
        if (first == std::string::npos || rawLine[first] == '#')
            continue;
        if (!extractMarkers(rawLine, line, markers, error)) {
            error = "Line " + std::to_string(lineNo) + ": " + error;
            return false;
        }

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
            while (words >> tok) {
                // A marker anywhere in a bar applies to the whole bar.
                if (tok[0] == MarkerTag)
                    variation = markers[std::stoul(tok.substr(1))];
                else
                    tokens.push_back(tok);
            }
            if (tokens.empty())
                continue;

            if (tokens.size() == 1 && tokens[0] == "%") {
                if (out.bars.empty()) {
                    error = "Line " + std::to_string(lineNo) + ": '%' needs a previous bar";
                    return false;
                }
                out.bars.push_back(out.bars.back());
                out.variations.push_back(variation);
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
            out.variations.push_back(variation);
        }
    }

    if (out.bars.empty()) {
        error = "The progression is empty";
        return false;
    }
    return true;
}

const std::vector<BuiltinSong> &builtinSongs()
{
    static const std::vector<BuiltinSong> songs = {
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
        {"Pop Verse and Chorus in G", "Pop Rock", 112,
         "[Main]\n"
         "| G  | D | Em | C |\n"
         "| G  | D | C  | % |\n"
         "[B]\n"
         "| C  | G | D  | Em |\n"
         "| C  | G | D  | %  |\n"},
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
        // Minuet in G major, BWV Anh. 114, from the Notebook for Anna Magdalena
        // Bach (now attributed to Christian Petzold). Both halves are repeated;
        // the second one walks the bass.
        {"Minuet in G Major (Bach)", "Minuet", 112,
         "[Main]\n"
         "| G | G  | C  | G  | Am | G  | D G / | D |\n"
         "| G | G  | C  | G  | Am | G  | D7    | G |\n"
         "| G | G  | C  | G  | Am | G  | D G / | D |\n"
         "| G | G  | C  | G  | Am | G  | D7    | G |\n"
         "[Moving]\n"
         "| G | D  | Em | A  | D  | Em | A7    | D |\n"
         "| G | C  | G  | D7 | G  | C  | G D7 / | G |\n"
         "| G | D  | Em | A  | D  | Em | A7    | D |\n"
         "| G | C  | G  | D7 | G  | C  | G D7 / | G |\n"},
    };
    return songs;
}

} // namespace mc
