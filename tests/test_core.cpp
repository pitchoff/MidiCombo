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

    check(p.variationAt(0).empty() && p.variationNames().empty(), "no variation markers");

    // Variation markers
    check(mc::Progression::parse("| C | [B] G |\n[One Drop]\n| Am | % |\n[auto] F [main] G", p, err),
          "parse variation markers: " + err);
    check(p.size() == 6 && p.variations.size() == 6, "markers are no bars");
    check(p.variationAt(0).empty() && p.variationAt(1) == "B", "marker inside a bar applies to it");
    check(p.variationAt(2) == "One Drop" && p.variationAt(3) == "One Drop", "marker line, '%' keeps it");
    check(p.variationAt(4).empty() && p.variationAt(5) == "Main", "auto and main");
    check(p.variationAt(7) == "B", "variationAt loops");
    check(p.chordAt(1, 0, 48).name() == "G" && p.chordAt(4, 0, 48).name() == "F", "chords around markers");
    check(p.variationNames() == std::vector<std::string>{"B", "One Drop", "Main"}, "variationNames");
    check(p.transposed(2).variationAt(1) == "B", "transpose keeps markers");
    check(!mc::Progression::parse("| [B C |", p, err), "unterminated marker");
    check(!mc::Progression::parse("| C ] |", p, err), "stray ']'");
    check(!mc::Progression::parse("| [ ] C |", p, err), "empty marker");
    check(!mc::Progression::parse("[B]", p, err), "markers alone are empty");

    for (const auto &song : mc::builtinSongs()) {
        check(mc::Progression::parse(song.text, p, err), std::string("song ") + song.name + ": " + err);
        const mc::Style *style = nullptr;
        for (const auto &s : mc::builtinStyles())
            if (s.name == song.style)
                style = &s;
        check(style, std::string("song style exists: ") + song.style);
        for (const auto &v : p.variationNames()) {
            bool found = v == "Main";
            for (size_t i = 0; style && i < style->variations.size(); ++i)
                found |= style->variations[i].name == v;
            check(found, std::string("song ") + song.name + " uses a variation of its style: " + v);
        }
    }
    for (const auto &s : mc::builtinStyles()) {
        check(mc::validateStyle(s, err), err);
        check(s.guitar.size() == static_cast<size_t>(s.stepCount()), "built-in style has a guitar: " + s.name);
    }

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

    // Variations: empty parts come from the main pattern.
    st.variations = {{"B", {}, {}, "R5-.", "", "D-u."}};
    check(mc::validateStyle(st, err), "variation valid: " + err);
    check(st.bassOf(1) == "R5-." && st.compOf(1) == st.comp && st.drumsOf(1)[0].steps == st.drums[0].steps
              && st.fillOf(1)[0].steps == st.fill[0].steps && st.bassOf(0) == st.bass && st.bassOf(5) == st.bass,
          "variation parts fall back to main");
    check(st.guitarOf(1) == "D-u." && st.guitarOf(0).empty(), "variation guitar");
    mc::Style resized = mc::resizedStyle(st, 2, 4, 1);
    check(resized.variations[0].bass == "R-5---..", "variation bass is resized: " + resized.variations[0].bass);
    check(resized.variations[0].comp.empty() && resized.variations[0].drums.empty(), "main parts stay inherited");
    check(mc::validateStyle(resized, err), "resized variation valid: " + err);
    mc::Style badVariation = st;
    badVariation.variations[0].bass = "R5";
    check(!mc::validateStyle(badVariation, err), "variation length is checked");
    badVariation = st;
    badVariation.variations = {{"B", {}, {}, "", ""}, {"B", {}, {}, "", ""}};
    check(!mc::validateStyle(badVariation, err), "variation names are unique");
    for (const char *name : {"Main", "main", "Auto", "random", "A [1]", "A|B"}) {
        badVariation.variations = {{name, {}, {}, "", ""}};
        check(!mc::validateStyle(badVariation, err), std::string("reserved variation name: ") + name);
    }
    badVariation.variations = {{"One Drop", {}, {}, "", ""}};
    check(mc::validateStyle(badVariation, err), "variation name with a space: " + err);
    using mc::VariationMode;
    check(mc::nextVariation(VariationMode::InTurn, 0, 3, 0) == 1 && mc::nextVariation(VariationMode::InTurn, 2, 3, 0) == 0,
          "variations in turn");
    check(mc::nextVariation(VariationMode::Fixed, 1, 3, 7) == 1 && mc::nextVariation(VariationMode::Random, 0, 1, 7) == 0,
          "no change without a choice");
    for (unsigned r = 0; r < 20; ++r) {
        const int next = mc::nextVariation(VariationMode::Random, 1, 4, r);
        check(next != 1 && next >= 0 && next < 4, "random variation differs");
    }
    check(mc::variationInterval(8, 1) == 8 && mc::variationInterval(3, 2) == 4 && mc::variationInterval(1, 4) == 4,
          "variation changes on whole patterns");

    if (failures == 0)
        std::cout << "All tests passed\n";
    return failures == 0 ? 0 : 1;
}
