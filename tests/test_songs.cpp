#include "SongFile.h"

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
    SongData a;
    a.name = QStringLiteral("Blues & <Friends>");
    a.style = QStringLiteral("Shuffle Blues");
    a.tempo = 96;
    a.swing = 40;
    a.transpose = -3;
    a.variation = QStringLiteral("random");
    a.variationEvery = 4;
    a.progression = QStringLiteral("# comment\n[One Drop]\n| F7 | Bb7 |\n| C7 / Bb7 / |");
    a.parts[1] = {true, false, 32, 90};
    a.parts[3] = {true, true, 89, 70};
    SongData b;
    b.name = QStringLiteral("Second");
    b.progression = QStringLiteral("| C | G |");

    const QString path = dir.filePath(QStringLiteral("roundtrip.xml"));
    check(writeSongFile(path, {a, b}, error), "write: " + error.toStdString());

    QList<SongData> loaded;
    check(readSongFile(path, loaded, error), "read: " + error.toStdString());
    check(loaded.size() == 2, "two songs loaded");
    if (loaded.size() == 2) {
        const SongData &l = loaded[0];
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
        "<midicombo-songs version=\"1\"><song name=\"X\"><future/>"
        "<progression>| Am |</progression></song></midicombo-songs>");
    check(readSongFile(lenient, loaded, error) && loaded.size() == 1 && loaded[0].progression == "| Am |",
          "unknown elements are ignored");

    // Preset files of older versions
    const QString old = writeText(dir, QStringLiteral("old.xml"),
        "<midicombo-presets version=\"1\"><preset name=\"Old\"><tempo>90</tempo>"
        "<progression>| D |</progression></preset></midicombo-presets>");
    check(readSongFile(old, loaded, error) && loaded.size() == 1 && loaded[0].name == "Old" && loaded[0].tempo == 90,
          "old preset files are read");

    // Written files use the new element names.
    const QString written = dir.filePath(QStringLiteral("written.xml"));
    check(writeSongFile(written, {b}, error), "write single song");
    {
        QFile f(written);
        const QByteArray content = f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
        check(content.contains("<midicombo-songs") && content.contains("<song name=\"Second\""), "new element names");
    }

    // Errors
    check(!readSongFile(dir.filePath(QStringLiteral("missing.xml")), loaded, error), "missing file");
    check(!readSongFile(writeText(dir, QStringLiteral("other.xml"), "<html/>"), loaded, error), "wrong root element");
    check(!readSongFile(writeText(dir, QStringLiteral("broken.xml"), "<midicombo-songs><song>"), loaded, error),
          "truncated XML");
    check(!readSongFile(writeText(dir, QStringLiteral("empty.xml"), "<midicombo-songs/>"), loaded, error),
          "no songs");
    check(!readSongFile(writeText(dir, QStringLiteral("badnum.xml"),
                                    "<midicombo-songs><song><tempo>fast</tempo></song></midicombo-songs>"),
                          loaded, error),
          "non-numeric tempo");
    check(!readSongFile(writeText(dir, QStringLiteral("newer.xml"), "<midicombo-songs version=\"9\"/>"), loaded, error),
          "newer format version");

    if (failures == 0)
        std::cout << "All song tests passed\n";
    return failures == 0 ? 0 : 1;
}
