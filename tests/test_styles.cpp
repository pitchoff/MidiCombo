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

    std::vector<mc::Style> lengthErr;
    readStyleFile(writeText(dir, QStringLiteral("len.xml"), styleFile("<bass>R-5-</bass>")), lengthErr, error);
    check(error.contains(QStringLiteral("4 steps, expected 8")), "length error names the step counts: " + error.toStdString());

    if (failures == 0)
        std::cout << "All style tests passed\n";
    return failures == 0 ? 0 : 1;
}
