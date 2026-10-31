#include "PresetFile.h"

#include <QFile>
#include <QTemporaryDir>

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

QString writeText(const QTemporaryDir &dir, const QString &name, const QByteArray &content)
{
    const QString path = dir.filePath(name);
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(content);
    return path;
}

} // namespace

int main()
{
    QTemporaryDir dir;
    QString error;

    // Round trip, including characters that need XML escaping.
    PresetData a;
    a.name = QStringLiteral("Blues & <Friends>");
    a.style = QStringLiteral("Shuffle Blues");
    a.tempo = 96;
    a.swing = 40;
    a.transpose = -3;
    a.variation = QStringLiteral("random");
    a.variationEvery = 4;
    a.progression = QStringLiteral("# comment\n| F7 | Bb7 |\n| C7 / Bb7 / |");
    a.parts[1] = {true, false, 32, 90};
    a.parts[3] = {true, true, 89, 70};
    PresetData b;
    b.name = QStringLiteral("Second");
    b.progression = QStringLiteral("| C | G |");

    const QString path = dir.filePath(QStringLiteral("roundtrip.xml"));
    check(writePresetFile(path, {a, b}, error), "write: " + error.toStdString());

    QList<PresetData> loaded;
    check(readPresetFile(path, loaded, error), "read: " + error.toStdString());
    check(loaded.size() == 2, "two presets loaded");
    if (loaded.size() == 2) {
        const PresetData &l = loaded[0];
        check(l.name == a.name, "name");
        check(l.style == a.style, "style");
        check(l.tempo == 96 && l.swing == 40 && l.transpose == -3, "numbers");
        check(l.progression == a.progression, "progression text");
        check(l.variation == a.variation && l.variationEvery == 4, "variation");
        check(loaded[1].variation.isEmpty() && loaded[1].variationEvery == -1, "no variation");
        check(!l.parts[0].present && !l.parts[2].present, "absent parts stay absent");
        check(l.parts[1].present && !l.parts[1].enabled && l.parts[1].program == 32 && l.parts[1].volume == 90, "bass part");
        check(l.parts[3].present && l.parts[3].enabled && l.parts[3].program == 89, "pad part");
        check(loaded[1].tempo == -1 && loaded[1].swing == -1 && loaded[1].style.isEmpty(), "optional values");
    }

    // Hand-written file with unknown elements.
    const QString lenient = writeText(dir, QStringLiteral("lenient.xml"),
        "<midicombo-presets version=\"1\"><preset name=\"X\"><future/>"
        "<progression>| Am |</progression></preset></midicombo-presets>");
    check(readPresetFile(lenient, loaded, error) && loaded.size() == 1 && loaded[0].progression == "| Am |",
          "unknown elements are ignored");

    // Errors
    check(!readPresetFile(dir.filePath(QStringLiteral("missing.xml")), loaded, error), "missing file");
    check(!readPresetFile(writeText(dir, QStringLiteral("other.xml"), "<html/>"), loaded, error), "wrong root element");
    check(!readPresetFile(writeText(dir, QStringLiteral("broken.xml"), "<midicombo-presets><preset>"), loaded, error),
          "truncated XML");
    check(!readPresetFile(writeText(dir, QStringLiteral("empty.xml"), "<midicombo-presets/>"), loaded, error),
          "no presets");
    check(!readPresetFile(writeText(dir, QStringLiteral("badnum.xml"),
                                    "<midicombo-presets><preset><tempo>fast</tempo></preset></midicombo-presets>"),
                          loaded, error),
          "non-numeric tempo");
    check(!readPresetFile(writeText(dir, QStringLiteral("newer.xml"), "<midicombo-presets version=\"9\"/>"), loaded, error),
          "newer format version");

    if (failures == 0)
        std::cout << "All preset tests passed\n";
    return failures == 0 ? 0 : 1;
}
