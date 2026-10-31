#include "StyleFile.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <iostream>
#include <set>

namespace {

int failures = 0;

void check(bool ok, const std::string &what)
{
    if (!ok) {
        ++failures;
        std::cerr << "FAIL: " << what << "\n";
    }
}

QString writeText(const QTemporaryDir &dir, const QString &name, const QByteArray &content)
{
    const QString path = dir.filePath(name);
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(content);
    return path;
}

// Wraps a single style body into a complete file.
QByteArray styleFile(const QByteArray &body)
{
    return "<midicombo-styles version=\"1\"><style name=\"T\">" + body + "</style></midicombo-styles>";
}

bool fails(const QTemporaryDir &dir, const QByteArray &content, const std::string &what)
{
    std::vector<mc::Style> styles;
    QString error;
    const bool ok = readStyleFile(writeText(dir, QStringLiteral("t.xml"), content), styles, error);
    check(!ok, "should fail: " + what);
    return !ok;
}

} // namespace

int main()
{
    QString error;
    std::vector<mc::Style> styles;

    // Every example file loads and every style in it is valid.
    const QDir examples(QStringLiteral(EXAMPLES_DIR "/styles"));
    const QStringList files = examples.entryList({QStringLiteral("*.xml")}, QDir::Files);
    check(files.size() >= 4, "example style files found in " EXAMPLES_DIR);
    std::set<std::string> names;
    for (const QString &f : files) {
        const bool ok = readStyleFile(examples.filePath(f), styles, error);
        check(ok, f.toStdString() + ": " + error.toStdString());
        for (const auto &s : styles) {
            std::string invalid;
            check(mc::validateStyle(s, invalid), invalid);
            check(names.insert(s.name).second, "duplicate example style " + s.name);
        }
    }
    check(names.count("6/8 Folk") && names.count("Samba") && names.count("Disco"), "expected example styles");
    check(names.count("Hard Rock") && names.count("Heavy Metal") && names.count("Symphonic Rock"), "rock example styles");

    // Values are read correctly.
    QTemporaryDir dir;
    const QString full = writeText(dir, QStringLiteral("full.xml"), styleFile(
        "<meter beats=\"3\" steps-per-beat=\"3\" bars=\"2\"/>"
        "<tempo>90</tempo><swing amount=\"50\" unit=\"16\"/><crash drum=\"splash\"/>"
        "<drums><row drum=\"kick\">X.. ... ...|X.. ... ...</row>\n"
        "       <row note=\"42\">\n  x.x x.x x.x\n  x.x x.x x.x\n</row></drums>"
        "<fill><row drum=\"snare\">... ... xxX</row></fill>"
        "<bass program=\"35\" velocity=\"90\">R-- 5-- 3-- | R-- 5-- A--</bass>"
        "<pad program=\"50\" enabled=\"true\"/><unknown-future-element/>"));
    check(readStyleFile(full, styles, error) && styles.size() == 1, "read full style: " + error.toStdString());
    if (styles.size() == 1) {
        const mc::Style &s = styles[0];
        check(s.name == "T" && s.beatsPerBar == 3 && s.stepsPerBeat == 3 && s.bars == 2, "meter");
        check(s.tempo == 90 && s.swing == 0.5 && s.swingUnit == 16 && s.crashNote == 55, "tempo, swing, crash");
        check(s.drums.size() == 2 && s.drums[1].note == 42 && s.drums[1].steps == "x.xx.xx.xx.xx.xx.x", "drum rows");
        check(s.fill.size() == 1 && s.fill[0].note == 38, "fill");
        check(s.bass == "R--5--3--R--5--A--" && s.bassProgram == 35 && s.bassVelocity == 90, "bass");
        check(s.comp == std::string(18, '.'), "missing comp part rests");
        check(s.padDefault && s.padProgram == 50, "pad");
        check(!s.powerChords, "close voicing by default");
    }

    // Errors
    check(!readStyleFile(dir.filePath(QStringLiteral("missing.xml")), styles, error), "missing file");
    fails(dir, "<midicombo-presets/>", "wrong root element");
    fails(dir, "<midicombo-styles version=\"1\"/>", "no styles");
    fails(dir, "<midicombo-styles version=\"7\"><style name=\"T\"/></midicombo-styles>", "newer version");
    fails(dir, "<midicombo-styles><style name=\"T\">", "truncated XML");
    fails(dir, "<midicombo-styles><style><bass>RRRRRRRR</bass></style></midicombo-styles>", "style without name");
    fails(dir, styleFile("<drums><row drum=\"bongo-drum\">X.......</row></drums>"), "unknown drum");
    fails(dir, styleFile("<drums><row>X.......</row></drums>"), "row without drum");
    fails(dir, styleFile("<bass>R-5-</bass>"), "wrong pattern length");
    fails(dir, styleFile("<meter steps-per-beat=\"5\"/>"), "steps per beat not dividing 12");
    fails(dir, styleFile("<meter beats=\"0\"/>"), "zero beats");
    fails(dir, styleFile("<tempo>fast</tempo>"), "non-numeric tempo");
    fails(dir, styleFile("<swing amount=\"30\" unit=\"12\"/>"), "bad swing unit");
    fails(dir, styleFile("<comp voicing=\"open\">C-C-C-C-</comp>"), "unknown comp voicing");

    const QString power = writeText(dir, QStringLiteral("power.xml"),
                                    styleFile("<comp voicing=\"power\">C-C-C-C-</comp>"));
    check(readStyleFile(power, styles, error) && styles.size() == 1 && styles[0].powerChords, "power voicing");

    std::vector<mc::Style> lengthErr;
    readStyleFile(writeText(dir, QStringLiteral("len.xml"), styleFile("<bass>R-5-</bass>")), lengthErr, error);
    check(error.contains(QStringLiteral("4 steps, expected 8")), "length error names the step counts: " + error.toStdString());

    // Writing and reading back gives the same styles.
    std::vector<mc::Style> all = mc::builtinStyles();
    for (const QString &name : QDir(QStringLiteral(EXAMPLES_DIR "/styles")).entryList({"*.xml"})) {
        std::vector<mc::Style> loaded;
        check(readStyleFile(QStringLiteral(EXAMPLES_DIR "/styles/") + name, loaded, error),
              name.toStdString() + ": " + error.toStdString());
        for (const auto &style : loaded)
            check(!style.variations.empty(), "example has a variation: " + style.name);
            all.insert(all.end(), loaded.begin(), loaded.end());
    }
    all.back().crashNote = 0;
    all.back().variations.push_back({"Bridge", all.back().drums, all.back().fill, "", ""});
    all.back().drums.push_back({90, std::string(all.back().stepCount(), 'x')}); // a note without a name
    const QString roundTrip = dir.filePath(QStringLiteral("roundtrip.xml"));
    check(writeStyleFile(roundTrip, all, error), "write styles: " + error.toStdString());
    std::vector<mc::Style> back;
    check(readStyleFile(roundTrip, back, error), "read written styles: " + error.toStdString());
    check(back.size() == all.size(), "round trip keeps the number of styles");
    for (size_t i = 0; i < std::min(back.size(), all.size()); ++i) {
        const mc::Style &a = all[i], &b = back[i];
        bool same = a.name == b.name && a.beatsPerBar == b.beatsPerBar && a.stepsPerBeat == b.stepsPerBeat
                    && a.bars == b.bars && a.tempo == b.tempo && qRound(a.swing * 100) == qRound(b.swing * 100)
                    && a.swingUnit == b.swingUnit && a.crashNote == b.crashNote && a.bass == b.bass && a.comp == b.comp
                    && a.powerChords == b.powerChords && a.pianoProgram == b.pianoProgram
                    && a.bassProgram == b.bassProgram && a.padProgram == b.padProgram && a.padDefault == b.padDefault
                    && a.bassVelocity == b.bassVelocity && a.compVelocity == b.compVelocity
                    && a.drums.size() == b.drums.size() && a.fill.size() == b.fill.size();
        for (size_t r = 0; same && r < a.drums.size(); ++r)
            same = a.drums[r].note == b.drums[r].note && a.drums[r].steps == b.drums[r].steps;
        for (size_t r = 0; same && r < a.fill.size(); ++r)
            same = a.fill[r].note == b.fill[r].note && a.fill[r].steps == b.fill[r].steps;
        same = same && a.variations.size() == b.variations.size();
        for (size_t v = 0; same && v < a.variations.size(); ++v) {
            const mc::StyleVariation &x = a.variations[v], &y = b.variations[v];
            same = x.name == y.name && x.bass == y.bass && x.comp == y.comp && x.drums.size() == y.drums.size()
                   && x.fill.size() == y.fill.size();
            for (size_t r = 0; same && r < x.drums.size(); ++r)
                same = x.drums[r].note == y.drums[r].note && x.drums[r].steps == y.drums[r].steps;
            for (size_t r = 0; same && r < x.fill.size(); ++r)
                same = x.fill[r].note == y.fill[r].note && x.fill[r].steps == y.fill[r].steps;
        }
        check(same, "round trip of style " + a.name);
    }
    check(drumName(38) == QLatin1String("snare") && drumNote(QStringLiteral("kick")) == 36 && drumName(90).isEmpty(),
          "drum name lookup");

    if (failures == 0)
        std::cout << "All style tests passed\n";
    return failures == 0 ? 0 : 1;
}
