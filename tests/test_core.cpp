#include "Chord.h"
#include "ChordDetector.h"
#include "Progression.h"
#include "Style.h"

#include <iostream>

namespace {

int failures = 0;

void check(bool ok, const std::string &what)
{
    if (!ok) {
        ++failures;
        std::cerr << "FAIL: " << what << "\n";
    }
}

void expectParse(const std::string &text, const std::string &name)
{
    const auto c = mc::parseChord(text);
    check(c && c->name() == name, "parse '" + text + "' -> " + name + " (got " + (c ? c->name() : "nothing") + ")");
}

void expectDetect(const std::vector<int> &notes, const std::string &name)
{
    const auto c = mc::ChordDetector().detect(notes);
    std::string got = c ? c->name() : "nothing";
    std::string desc;
    for (int n : notes)
        desc += mc::noteName(n) + " ";
    check(got == name, "detect " + desc + "-> " + name + " (got " + got + ")");
}

} // namespace

int main()
{
    expectParse("C", "C");
    expectParse("Am7", "Am7");
    expectParse("F#maj7", "F#maj7");
    expectParse("Bb7", "Bb7");
    expectParse("Ebm7b5", "Ebm7b5");
    expectParse("Gsus", "Gsus4");
    expectParse("C/E", "C/E");
    expectParse("D-7", "Dm7");
    expectParse("Ab^7", "Abmaj7");
    check(!mc::parseChord("H7"), "reject H7");
    check(!mc::parseChord("Cxyz"), "reject Cxyz");

    expectDetect({60, 64, 67}, "C");
    expectDetect({57, 60, 64, 67}, "Am7");
    expectDetect({60, 64, 67, 69}, "C6");
    expectDetect({60, 64, 67, 70}, "C7");
    expectDetect({62, 65, 69, 72}, "Dm7");
    expectDetect({52, 55, 60}, "C/E");
    expectDetect({60, 63, 66, 69}, "Cdim7");
    expectDetect({55, 59, 62, 65}, "G7");
    expectDetect({60, 65, 67}, "Csus4");
    expectDetect({48, 64, 70, 74}, "C9");
    expectDetect({48, 52, 58}, "C7");
    expectDetect({45, 60, 64}, "Am");
    expectDetect({59, 62, 65, 69}, "Bm7b5");
    expectDetect({40, 47}, "E5");

    check(mc::noteName(60) == "C4", "noteName 60");
    check(mc::parseNoteName("F#3") == 54, "parseNoteName F#3");

    mc::Progression p;
    std::string err;
    check(mc::Progression::parse("| C G | Am / | % |", p, err) && p.size() == 3, "parse progression");
    check(p.chordAt(0, 0, 48).name() == "C" && p.chordAt(0, 24, 48).name() == "G", "chordAt bar 0");
    check(p.chordAt(1, 30, 48).name() == "Am", "chordAt with repeat");
    check(p.chordAt(2, 0, 48).name() == "Am", "repeat bar");
    check(p.chordAt(0, 48, 48).name() == "Am", "chordAt wraps into next bar");
    check(p.chordAt(3, 0, 48).name() == "C", "progression loops");
    check(!mc::Progression::parse("| C | Q7 |", p, err), "reject bad chord");
    check(mc::Progression::parse("C G Am F", p, err) && p.size() == 4, "line without bars");

    for (const auto &preset : mc::progressionPresets()) {
        check(mc::Progression::parse(preset.text, p, err), std::string("preset ") + preset.name + ": " + err);
        bool styleFound = false;
        for (const auto &s : mc::builtinStyles())
            styleFound |= s.name == preset.style;
        check(styleFound, std::string("preset style exists: ") + preset.style);
    }
    for (const auto &s : mc::builtinStyles())
        check(mc::validateStyle(s, err), err);

    // Resizing a style keeps steps that fall on the new grid.
    mc::Style st;
    st.name = "R";
    st.beatsPerBar = 2;
    st.stepsPerBeat = 2;
    st.bars = 1;
    st.drums = {{36, "X.x."}};
    st.fill = {{38, "..xX"}};
    st.bass = "R-5.";
    st.comp = "C.c.";
    mc::Style up = mc::resizedStyle(st, 3, 4, 2);
    check(mc::validateStyle(up, err), "resized style valid: " + err);
    check(up.drums[0].steps == "X...x.......X...x.......", "resize drums up: " + up.drums[0].steps);
    check(up.fill[0].steps == "....x.X.....", "resize fill: " + up.fill[0].steps);
    check(up.bass == "R---5-......R---5-......", "resize bass up: " + up.bass);
    mc::Style down = mc::resizedStyle(up, 2, 1, 1);
    check(down.drums[0].steps == "Xx" && down.bass == "R5", "resize down: " + down.drums[0].steps + " " + down.bass);
    mc::Style triplet = mc::resizedStyle(st, 2, 3, 1);
    check(triplet.bass == "R--5-." && triplet.drums[0].steps == "X..x..", "resize to triplets: " + triplet.bass + " "
                                                                              + triplet.drums[0].steps);

    if (failures == 0)
        std::cout << "All tests passed\n";
    return failures == 0 ? 0 : 1;
}
