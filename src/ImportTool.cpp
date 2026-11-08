// midicombo-import: turns a Standard MIDI File into a MidiCombo style file
// and a song file.

#include "MidiImport.h"
#include "SongFile.h"
#include "StyleFile.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

#include <cctype>
#include <cstdio>

namespace {

void print(const QString &text, FILE *to = stdout)
{
    std::fputs(qPrintable(text + QLatin1Char('\n')), to);
}

// "--bass 3" -> channel index 2, "--bass 0" -> none (-2), not given -> detect (-1).
bool channelOption(const QCommandLineParser &parser, const QString &name, int &channel)
{
    channel = -1;
    if (!parser.isSet(name))
        return true;
    bool ok = false;
    const int value = parser.value(name).toInt(&ok);
    if (!ok || value < 0 || value > 16) {
        print(QStringLiteral("--%1 needs a MIDI channel from 1 to 16, or 0 for none").arg(name), stderr);
        return false;
    }
    channel = value == 0 ? -2 : value - 1;
    return true;
}

// A file name made from the input file's name.
QString baseName(const QString &path)
{
    QString base = QFileInfo(path).completeBaseName().toLower();
    base.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    base.remove(QRegularExpression(QStringLiteral("^-+|-+$")));
    return base.isEmpty() ? QStringLiteral("imported") : base;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("midicombo-import"));
    QCoreApplication::setApplicationVersion(QStringLiteral(MIDICOMBO_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "Creates a MidiCombo style and song from a Standard MIDI File.\n"
        "Writes <name>-style.xml and <name>-song.xml. Load the style file in MidiCombo\n"
        "(File > Load Styles), then the song file (File > Open Songs)."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("midi-file"), QStringLiteral("The MIDI file (.mid) to import."));
    const QCommandLineOption outputOpt({QStringLiteral("o"), QStringLiteral("output-dir")},
                                       QStringLiteral("Directory for the two files (default: the current one)."),
                                       QStringLiteral("dir"), QStringLiteral("."));
    const QCommandLineOption nameOpt({QStringLiteral("n"), QStringLiteral("name")},
                                     QStringLiteral("Name of the style and the song (default: the title in the file, "
                                                    "or the file name)."),
                                     QStringLiteral("name"));
    const QCommandLineOption barsOpt({QStringLiteral("b"), QStringLiteral("bars")},
                                     QStringLiteral("Pattern length in bars, 1-8 (default: 2)."), QStringLiteral("n"),
                                     QStringLiteral("2"));
    const QCommandLineOption stepsOpt({QStringLiteral("s"), QStringLiteral("steps")},
                                      QStringLiteral("Steps per beat: 2, 3, 4, 6 or 12 (default: detected)."),
                                      QStringLiteral("n"));
    const QCommandLineOption variationsOpt(
        QStringLiteral("variations"),
        QStringLiteral("Most variations to create besides the main pattern (default: 2)."), QStringLiteral("n"),
        QStringLiteral("2"));
    const QCommandLineOption forceOpt({QStringLiteral("f"), QStringLiteral("force")},
                                      QStringLiteral("Overwrite existing files."));
    const QCommandLineOption quietOpt({QStringLiteral("q"), QStringLiteral("quiet")},
                                      QStringLiteral("Print only errors."));
    parser.addOptions({outputOpt, nameOpt, barsOpt, stepsOpt, variationsOpt, forceOpt, quietOpt});
    const QString roles[] = {QStringLiteral("drums"), QStringLiteral("bass"), QStringLiteral("comp"),
                             QStringLiteral("guitar"), QStringLiteral("pad")};
    for (const QString &role : roles)
        parser.addOption(QCommandLineOption(
            role, QStringLiteral("MIDI channel (1-16) of the %1, 0 = none (default: detected).").arg(role),
            QStringLiteral("channel")));
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.size() != 1) {
        print(QStringLiteral("Give exactly one MIDI file. See --help."), stderr);
        return 2;
    }
    const QString input = args.first();
    const bool quiet = parser.isSet(quietOpt);

    mc::ImportOptions options;
    options.name = parser.value(nameOpt).trimmed().toStdString();
    bool ok = false;
    options.bars = parser.value(barsOpt).toInt(&ok);
    if (!ok || options.bars < 1 || options.bars > 8) {
        print(QStringLiteral("--bars needs a number from 1 to 8"), stderr);
        return 2;
    }
    if (parser.isSet(stepsOpt)) {
        options.stepsPerBeat = parser.value(stepsOpt).toInt(&ok);
        if (!ok || options.stepsPerBeat <= 0 || 12 % options.stepsPerBeat != 0) {
            print(QStringLiteral("--steps needs 1, 2, 3, 4, 6 or 12"), stderr);
            return 2;
        }
    }
    options.maxVariations = parser.value(variationsOpt).toInt(&ok);
    if (!ok || options.maxVariations < 0 || options.maxVariations > 24) {
        print(QStringLiteral("--variations needs a number from 0 to 24"), stderr);
        return 2;
    }
    int *channels[] = {&options.drumChannel, &options.bassChannel, &options.compChannel, &options.guitarChannel,
                       &options.padChannel};
    for (int i = 0; i < 5; ++i)
        if (!channelOption(parser, roles[i], *channels[i]))
            return 2;

    mc::MidiFileData midi;
    std::string error;
    if (!mc::readMidiFile(QFile::encodeName(input).toStdString(), midi, error)) {
        print(QStringLiteral("%1: %2").arg(input, QString::fromStdString(error)), stderr);
        return 1;
    }
    if (options.name.empty()) { // the title in the file, if it is a usable one, or the file name
        int letters = 0;
        for (char c : midi.title)
            letters += std::isalpha(static_cast<unsigned char>(c)) != 0;
        if (letters < 2)
            options.name = QFileInfo(input).completeBaseName().toStdString();
    }

    mc::ImportResult result;
    if (!mc::importMidi(midi, options, result, error)) {
        print(QStringLiteral("%1: %2").arg(input, QString::fromStdString(error)), stderr);
        return 1;
    }

    const QDir dir(parser.value(outputOpt));
    if (!dir.exists()) {
        print(QStringLiteral("The directory %1 does not exist").arg(dir.path()), stderr);
        return 1;
    }
    const QString base = baseName(input);
    const QString stylePath = dir.filePath(base + QStringLiteral("-style.xml"));
    const QString songPath = dir.filePath(base + QStringLiteral("-song.xml"));
    if (!parser.isSet(forceOpt)) {
        for (const QString &path : {stylePath, songPath}) {
            if (QFileInfo::exists(path)) {
                print(QStringLiteral("%1 exists; use --force to overwrite it").arg(path), stderr);
                return 1;
            }
        }
    }

    const mc::Style &style = result.style;
    QString writeError;
    if (!writeStyleFile(stylePath, {style}, writeError)) {
        print(QStringLiteral("Could not write %1: %2").arg(stylePath, writeError), stderr);
        return 1;
    }

    SongData song;
    song.name = QString::fromStdString(style.name);
    song.style = song.name;
    song.tempo = result.tempo;
    song.swing = result.swingPercent;
    song.progression = QString::fromStdString(result.progression).trimmed();
    const mc::ImportRole partRoles[SongData::PartCount] = {mc::ImportRole::Drums, mc::ImportRole::Bass,
                                                          mc::ImportRole::Comp, mc::ImportRole::Pad,
                                                          mc::ImportRole::Guitar};
    const int defaults[SongData::PartCount] = {0, style.bassProgram, style.pianoProgram, style.padProgram,
                                               style.guitarProgram};
    for (int i = 0; i < SongData::PartCount; ++i) {
        SongData::Part &part = song.parts[i];
        part.present = true;
        part.enabled = false;
        part.program = defaults[i];
        for (int c = 0; c < 16; ++c) {
            if (result.roles[c] != partRoles[i])
                continue;
            part.enabled = true;
            if (result.programs[c] >= 0)
                part.program = result.programs[c]; // for drums, the kit
        }
    }
    if (!writeSongFile(songPath, {song}, writeError)) {
        print(QStringLiteral("Could not write %1: %2").arg(songPath, writeError), stderr);
        return 1;
    }

    if (!quiet) {
        for (const std::string &line : result.report)
            print(QString::fromStdString(line));
        print(QStringLiteral("Wrote %1 (style \"%2\") and %3").arg(stylePath, song.name, songPath));
    }
    return 0;
}
