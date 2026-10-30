#include "Style.h"

#include <algorithm>
#include <cctype>

namespace mc {

namespace {

// General MIDI percussion notes
enum Drum {
    Kick = 36, SideStick = 37, Snare = 38, FloorTom = 41, ClosedHat = 42,
    LowTom = 45, PedalHat = 44, OpenHat = 46, MidTom = 47, HighTom = 50,
    Crash = 49, Ride = 51, Shaker = 70,
};

// Patterns may contain whitespace and '|' for readability.
std::string strip(std::string s)
{
    auto isFiller = [](char c) { return std::isspace(static_cast<unsigned char>(c)) || c == '|'; };
    s.erase(std::remove_if(s.begin(), s.end(), isFiller), s.end());
    return s;
}

std::vector<Style> makeStyles()
{
    std::vector<Style> styles;

    {
        Style s;
        s.name = "Pop Rock";
        s.tempo = 112;
        s.drums = {
            {ClosedHat, "XxXx XxXx"},
            {Snare,     "..X. ..X."},
            {Kick,      "X... Xx.."},
        };
        s.fill = {
            {ClosedHat, "XxXx ...."},
            {Snare,     "..x. Xx.."},
            {MidTom,    ".... ..X."},
            {FloorTom,  ".... ...X"},
            {Kick,      "X... X..."},
        };
        s.bass = "R-RR 5-RA";
        s.comp = "C--C --C-";
        s.pianoProgram = 0;
        s.bassProgram = 33;
        s.padProgram = 48;
        styles.push_back(s);
    }
    {
        Style s;
        s.name = "Ballad";
        s.tempo = 72;
        s.stepsPerBeat = 4;
        s.drums = {
            {ClosedHat, "x.o. x.o. x.o. x.o."},
            {SideStick, ".... x... .... x..."},
            {Kick,      "X... .... X.x. ...."},
        };
        s.fill = {
            {ClosedHat, "x.o. x.o. x.o. ...."},
            {SideStick, ".... x... .... ...."},
            {HighTom,   ".... .... .... x.x."},
            {LowTom,    ".... .... .... ..x."},
            {FloorTom,  ".... .... .... ...X"},
            {Kick,      "X... .... X... ...."},
        };
        s.bass = "R--- ---- 5--- --A-";
        s.comp = "1-2- 3-4- 5-4- 3-2-";
        s.pianoProgram = 0;
        s.bassProgram = 33;
        s.padProgram = 89;
        s.padDefault = true;
        s.compVelocity = 70;
        styles.push_back(s);
    }
    {
        Style s;
        s.name = "Swing Jazz";
        s.tempo = 140;
        s.bars = 2;
        s.swing = 1.0;
        s.drums = {
            {Ride,     "x.Xx x.Xx | x.Xx x.Xx"},
            {PedalHat, "..x. ..x. | ..x. ..x."},
            {Kick,     "g.g. g.g. | g.g. g.g."},
            {Snare,    ".... .... | ...g ...."},
        };
        s.fill = {
            {Ride,     "x.Xx x..."},
            {PedalHat, "..x. ..x."},
            {Snare,    "..g. x.xX"},
        };
        s.bass = "R-3- 5-A- | R-8- 5-A-";
        s.comp = "C-.c -... | ..C- ...c";
        s.pianoProgram = 0;
        s.bassProgram = 32;
        s.padProgram = 49;
        s.bassVelocity = 100;
        s.compVelocity = 72;
        styles.push_back(s);
    }
    {
        Style s;
        s.name = "Bossa Nova";
        s.tempo = 130;
        s.bars = 2;
        s.crashNote = 0;
        s.drums = {
            {SideStick, "x..x ..x. | ..x. .x.."},
            {ClosedHat, "xoxo xoxo | xoxo xoxo"},
            {Kick,      "o..o o..o | o..o o..o"},
        };
        s.bass = "R--5 5--R | R--5 5--R";
        s.comp = "C-.C -.C. | .C-. C-..";
        s.pianoProgram = 24; // nylon guitar
        s.bassProgram = 32;
        s.padProgram = 48;
        s.compVelocity = 74;
        styles.push_back(s);
    }
    {
        Style s;
        s.name = "Funk";
        s.tempo = 100;
        s.stepsPerBeat = 4;
        s.swing = 0.15;
        s.swingUnit = 16;
        s.drums = {
            {ClosedHat, "Xxox Xxox Xxox Xx.x"},
            {OpenHat,   ".... .... .... ..x."},
            {Snare,     ".... X..g .g.. X..g"},
            {Kick,      "X.x. ...x ..X. ...."},
        };
        s.fill = {
            {ClosedHat, "Xxox Xxox .... ...."},
            {Snare,     ".... X..g xgxg XxXx"},
            {Kick,      "X.x. ...x .... ...."},
        };
        s.bass = "R--8 ..R. R.5. 7-A.";
        s.comp = "..C. ..c. .C.. C.c.";
        s.pianoProgram = 7; // clavinet
        s.bassProgram = 33;
        s.padProgram = 50;
        s.bassVelocity = 104;
        styles.push_back(s);
    }
    {
        Style s;
        s.name = "Waltz";
        s.tempo = 132;
        s.beatsPerBar = 3;
        s.bars = 2;
        s.swing = 0.6;
        s.drums = {
            {Kick,      "X... .. | X... .."},
            {ClosedHat, "..x. x. | ..x. x."},
            {Ride,      "x.x. x. | x.x. xx"},
        };
        s.fill = {
            {Ride,  "x.x. .."},
            {Snare, "..x. xX"},
            {Kick,  "X... .."},
        };
        s.bass = "R--- -- | 5--- --";
        s.comp = "..C- C- | ..C- C-";
        s.pianoProgram = 0;
        s.bassProgram = 32;
        s.padProgram = 48;
        styles.push_back(s);
    }
    {
        Style s;
        s.name = "Reggae";
        s.tempo = 76;
        s.swing = 0.3;
        s.crashNote = 0;
        s.drums = {
            {ClosedHat, "xoxo xoxo"},
            {SideStick, ".... x..."},
            {Kick,      ".... X..."},
        };
        s.fill = {
            {ClosedHat, "xoxo ...."},
            {Snare,     ".... xxXX"},
            {Kick,      ".... X..."},
        };
        s.bass = "R--R 5-..";
        s.comp = ".c.c .c.c";
        s.pianoProgram = 27; // clean electric guitar
        s.bassProgram = 33;
        s.padProgram = 16;   // drawbar organ
        s.compVelocity = 82;
        styles.push_back(s);
    }
    {
        Style s;
        s.name = "Shuffle Blues";
        s.tempo = 100;
        s.stepsPerBeat = 3;
        s.drums = {
            {ClosedHat, "x.x X.x x.x X.x"},
            {Snare,     "... X.. ... X.."},
            {Kick,      "X.. ... X.. ..."},
        };
        s.fill = {
            {ClosedHat, "x.x X.x ... ..."},
            {Snare,     "... X.. x.x XxX"},
            {Kick,      "X.. ... X.. ..."},
        };
        s.bass = "R-3 5-6 7-6 5-3";
        s.comp = "..c ..C ..c ..C";
        s.pianoProgram = 0;
        s.bassProgram = 33;
        s.padProgram = 16;
        styles.push_back(s);
    }

    for (auto &s : styles)
        s = normalizedStyle(s);
    return styles;
}

} // namespace

Style normalizedStyle(Style s)
{
    for (auto &r : s.drums)
        r.steps = strip(r.steps);
    for (auto &r : s.fill)
        r.steps = strip(r.steps);
    s.bass = strip(s.bass);
    s.comp = strip(s.comp);
    return s;
}

const std::vector<Style> &builtinStyles()
{
    static const std::vector<Style> styles = makeStyles();
    return styles;
}

bool validateStyle(const Style &s, std::string &error)
{
    auto fail = [&](const std::string &what) {
        error = s.name + ": " + what;
        return false;
    };
    if (s.stepsPerBeat <= 0 || 12 % s.stepsPerBeat != 0)
        return fail("stepsPerBeat must divide 12");
    auto length = [](const std::string &what, size_t got, int expected) {
        return what + " has " + std::to_string(got) + " steps, expected " + std::to_string(expected);
    };
    const int steps = s.stepCount();
    for (const auto &r : s.drums)
        if (static_cast<int>(r.steps.size()) != steps)
            return fail(length("drum row " + std::to_string(r.note), r.steps.size(), steps));
    for (const auto &r : s.fill)
        if (static_cast<int>(r.steps.size()) != s.stepsPerBar())
            return fail(length("fill row " + std::to_string(r.note), r.steps.size(), s.stepsPerBar()));
    if (static_cast<int>(s.bass.size()) != steps)
        return fail(length("bass pattern", s.bass.size(), steps));
    if (static_cast<int>(s.comp.size()) != steps)
        return fail(length("comp pattern", s.comp.size(), steps));
    return true;
}

namespace {

// `track` patterns (bass, comp) turn a step inside a note that has no
// counterpart on the old grid into a hold instead of a rest.
std::string resizePattern(const std::string &old, const Style &from, int beats, int spb, int oldBars, int newBars,
                          bool track)
{
    std::string result;
    result.reserve(static_cast<size_t>(newBars * beats * spb));
    for (int bar = 0; bar < newBars; ++bar) {
        const int srcBar = bar % oldBars;
        for (int beat = 0; beat < beats; ++beat) {
            for (int sub = 0; sub < spb; ++sub) {
                char c = '.';
                if (beat < from.beatsPerBar) {
                    const int pos = sub * from.stepsPerBeat; // in units of 1/(spb*oldSpb) beat
                    const size_t index = static_cast<size_t>(srcBar * from.stepsPerBar() + beat * from.stepsPerBeat
                                                             + pos / spb);
                    if (index < old.size()) {
                        if (pos % spb == 0)
                            c = old[index];
                        else if (track && old[index] != '.')
                            c = '-';
                    }
                }
                result += c;
            }
        }
    }
    return result;
}

} // namespace

Style resizedStyle(const Style &style, int beatsPerBar, int stepsPerBeat, int bars)
{
    Style s = style;
    s.beatsPerBar = beatsPerBar;
    s.stepsPerBeat = stepsPerBeat;
    s.bars = bars;
    for (auto &r : s.drums)
        r.steps = resizePattern(r.steps, style, beatsPerBar, stepsPerBeat, style.bars, bars, false);
    for (auto &r : s.fill)
        r.steps = resizePattern(r.steps, style, beatsPerBar, stepsPerBeat, 1, 1, false);
    s.bass = resizePattern(style.bass, style, beatsPerBar, stepsPerBeat, style.bars, bars, true);
    s.comp = resizePattern(style.comp, style, beatsPerBar, stepsPerBeat, style.bars, bars, true);
    return s;
}

} // namespace mc
