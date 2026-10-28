#include "Chord.h"

#include <cctype>

namespace mc {

namespace {

// Order must match enum Quality.
const std::vector<QualityInfo> &qualityTable()
{
    static const std::vector<QualityInfo> table = {
        {"",      {0, 4, 7},          {"", "M", "maj"}, 0},
        {"m",     {0, 3, 7},          {"m", "min", "-"}, 0},
        {"dim",   {0, 3, 6},          {"dim", "o", "°"}, 2},
        {"aug",   {0, 4, 8},          {"aug", "+"}, 2},
        {"sus2",  {0, 2, 7},          {"sus2"}, 1},
        {"sus4",  {0, 5, 7},          {"sus4", "sus"}, 1},
        {"5",     {0, 7},             {"5"}, 1},
        {"7",     {0, 4, 7, 10},      {"7", "dom7"}, 1},
        {"maj7",  {0, 4, 7, 11},      {"maj7", "M7", "Maj7", "^7", "^"}, 1},
        {"m7",    {0, 3, 7, 10},      {"m7", "min7", "-7"}, 1},
        {"m7b5",  {0, 3, 6, 10},      {"m7b5", "ø", "ø7", "h7", "-7b5"}, 2},
        {"dim7",  {0, 3, 6, 9},       {"dim7", "o7", "°7"}, 2},
        {"mMaj7", {0, 3, 7, 11},      {"mMaj7", "mM7", "m(maj7)", "-maj7"}, 3},
        {"6",     {0, 4, 7, 9},       {"6"}, 1},
        {"m6",    {0, 3, 7, 9},       {"m6", "min6", "-6"}, 2},
        {"7sus4", {0, 5, 7, 10},      {"7sus4", "7sus"}, 2},
        {"add9",  {0, 4, 7, 14},      {"add9", "add2"}, 2},
        {"9",     {0, 4, 7, 10, 14},  {"9"}, 2},
        {"maj9",  {0, 4, 7, 11, 14},  {"maj9", "M9"}, 3},
        {"m9",    {0, 3, 7, 10, 14},  {"m9", "min9", "-9"}, 3},
        {"7#5",   {0, 4, 8, 10},      {"7#5", "aug7", "+7"}, 3},
        {"7b9",   {0, 4, 7, 10, 13},  {"7b9"}, 3},
    };
    return table;
}

const char *const kPitchClassNames[12] = {
    "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};

bool hasInterval(const std::vector<int> &ivs, int semis)
{
    for (int iv : ivs)
        if (iv == semis)
            return true;
    return false;
}

bool parsePitchClass(const std::string &s, size_t &pos, int &pc)
{
    static const int base[7] = {9, 11, 0, 2, 4, 5, 7}; // A..G
    if (pos >= s.size())
        return false;
    const char c = std::toupper(static_cast<unsigned char>(s[pos]));
    if (c < 'A' || c > 'G')
        return false;
    pc = base[c - 'A'];
    ++pos;
    while (pos < s.size() && (s[pos] == '#' || s[pos] == 'b')) {
        pc += s[pos] == '#' ? 1 : -1;
        ++pos;
    }
    pc = ((pc % 12) + 12) % 12;
    return true;
}

std::string trimmed(const std::string &s)
{
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos)
        return {};
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

} // namespace

const QualityInfo &qualityInfo(Quality q)
{
    return qualityTable()[static_cast<size_t>(q)];
}

std::string pitchClassName(int pc)
{
    return kPitchClassNames[((pc % 12) + 12) % 12];
}

std::string noteName(int midiNote)
{
    return pitchClassName(midiNote) + std::to_string(midiNote / 12 - 1);
}

int parseNoteName(const std::string &text)
{
    const std::string s = trimmed(text);
    size_t pos = 0;
    int pc = 0;
    if (!parsePitchClass(s, pos, pc) || pos >= s.size())
        return -1;
    try {
        size_t used = 0;
        const int octave = std::stoi(s.substr(pos), &used);
        if (pos + used != s.size())
            return -1;
        const int note = (octave + 1) * 12 + pc;
        return note >= 0 && note <= 127 ? note : -1;
    } catch (...) {
        return -1;
    }
}

std::string Chord::name() const
{
    if (!isValid())
        return {};
    std::string n = pitchClassName(root) + qualityInfo(quality).suffix;
    if (bass >= 0 && bass != root)
        n += "/" + pitchClassName(bass);
    return n;
}

int Chord::third() const
{
    const auto &ivs = intervals();
    for (int t : {4, 3, 5, 2})
        if (hasInterval(ivs, t))
            return t;
    return 7; // power chord: no third, use the fifth
}

int Chord::fifth() const
{
    const auto &ivs = intervals();
    for (int f : {7, 6, 8})
        if (hasInterval(ivs, f))
            return f;
    return 7;
}

int Chord::seventh() const
{
    const auto &ivs = intervals();
    for (int s : {10, 11})
        if (hasInterval(ivs, s))
            return s;
    if (quality == Quality::Dim7)
        return 9;
    return 10; // blues-style flat seventh
}

Chord Chord::transposed(int semitones) const
{
    Chord c = *this;
    if (c.root >= 0)
        c.root = ((c.root + semitones) % 12 + 12) % 12;
    if (c.bass >= 0)
        c.bass = ((c.bass + semitones) % 12 + 12) % 12;
    return c;
}

std::optional<Chord> parseChord(const std::string &text)
{
    const std::string s = trimmed(text);
    size_t pos = 0;
    Chord chord;
    if (!parsePitchClass(s, pos, chord.root))
        return std::nullopt;

    const size_t slash = s.find('/', pos);
    const std::string suffix = s.substr(pos, slash == std::string::npos ? std::string::npos : slash - pos);

    bool found = false;
    const auto &table = qualityTable();
    for (size_t q = 0; q < table.size() && !found; ++q) {
        for (const char *alias : table[q].aliases) {
            if (suffix == alias) {
                chord.quality = static_cast<Quality>(q);
                found = true;
                break;
            }
        }
    }
    if (!found)
        return std::nullopt;

    if (slash != std::string::npos) {
        size_t bpos = slash + 1;
        int bass = 0;
        if (!parsePitchClass(s, bpos, bass) || bpos != s.size())
            return std::nullopt;
        if (bass != chord.root)
            chord.bass = bass;
    }
    return chord;
}

} // namespace mc
