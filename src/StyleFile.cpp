#include "StyleFile.h"

#include <QFile>
#include <QStringList>
#include <QSaveFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace {

constexpr auto RootElement = "midicombo-styles";
constexpr int FormatVersion = 1;

struct DrumName {
    const char *name;
    int note;
};

// General MIDI percussion map (channel 10)
const DrumName kDrums[] = {
    {"kick", 36}, {"side-stick", 37}, {"snare", 38}, {"clap", 39}, {"electric-snare", 40},
    {"floor-tom", 41}, {"closed-hat", 42}, {"high-floor-tom", 43}, {"pedal-hat", 44},
    {"low-tom", 45}, {"open-hat", 46}, {"mid-tom", 47}, {"high-mid-tom", 48}, {"crash", 49},
    {"high-tom", 50}, {"ride", 51}, {"china", 52}, {"ride-bell", 53}, {"tambourine", 54},
    {"splash", 55}, {"cowbell", 56}, {"crash-2", 57}, {"ride-2", 59}, {"high-bongo", 60},
    {"low-bongo", 61}, {"mute-conga", 62}, {"high-conga", 63}, {"low-conga", 64},
    {"high-timbale", 65}, {"low-timbale", 66}, {"high-agogo", 67}, {"low-agogo", 68},
    {"cabasa", 69}, {"shaker", 70}, {"short-guiro", 73}, {"long-guiro", 74}, {"claves", 75},
    {"high-woodblock", 76}, {"low-woodblock", 77}, {"mute-triangle", 80}, {"triangle", 81},
};

int intAttr(QXmlStreamReader &xml, const char *name, int fallback, int min, int max)
{
    const auto value = xml.attributes().value(QLatin1String(name));
    if (value.isEmpty())
        return fallback;
    bool ok = false;
    const int v = value.toInt(&ok);
    if (!ok || v < min || v > max) {
        xml.raiseError(QStringLiteral("<%1 %2=\"%3\">: expected a number from %4 to %5")
                           .arg(xml.name()).arg(QLatin1String(name)).arg(value).arg(min).arg(max));
        return fallback;
    }
    return v;
}

// Reads note="36" or drum="kick". Returns 0 for drum="none".
int drumAttr(QXmlStreamReader &xml, int fallback)
{
    const auto attrs = xml.attributes();
    if (attrs.hasAttribute(QLatin1String("note")))
        return intAttr(xml, "note", fallback, 0, 127);
    if (!attrs.hasAttribute(QLatin1String("drum")))
        return fallback;
    const auto name = attrs.value(QLatin1String("drum"));
    if (name == QLatin1String("none"))
        return 0;
    for (const auto &d : kDrums)
        if (name == QLatin1String(d.name))
            return d.note;
    xml.raiseError(QStringLiteral("Unknown drum '%1'").arg(name));
    return fallback;
}

std::vector<mc::DrumRow> readRows(QXmlStreamReader &xml)
{
    std::vector<mc::DrumRow> rows;
    while (xml.readNextStartElement()) {
        if (xml.name() != QLatin1String("row")) {
            xml.skipCurrentElement();
            continue;
        }
        const int note = drumAttr(xml, -1);
        if (note <= 0 && !xml.hasError())
            xml.raiseError(QStringLiteral("<row> needs a note or drum attribute"));
        const QString steps = xml.readElementText();
        if (note > 0)
            rows.push_back({note, steps.toStdString()});
    }
    return rows;
}

mc::Style readStyle(QXmlStreamReader &xml)
{
    mc::Style s;
    s.name = xml.attributes().value(QLatin1String("name")).toString().trimmed().toStdString();
    if (s.name.empty())
        xml.raiseError(QStringLiteral("<style> needs a name attribute"));

    while (xml.readNextStartElement()) {
        const auto name = xml.name();
        if (name == QLatin1String("meter")) {
            s.beatsPerBar = intAttr(xml, "beats", s.beatsPerBar, 1, 16);
            s.stepsPerBeat = intAttr(xml, "steps-per-beat", s.stepsPerBeat, 1, 12);
            s.bars = intAttr(xml, "bars", s.bars, 1, 16);
            xml.skipCurrentElement();
        } else if (name == QLatin1String("tempo")) {
            bool ok = false;
            s.tempo = xml.readElementText().trimmed().toInt(&ok);
            if (!ok || s.tempo < 30 || s.tempo > 320)
                xml.raiseError(QStringLiteral("<tempo> must be a number from 30 to 320"));
        } else if (name == QLatin1String("swing")) {
            s.swing = intAttr(xml, "amount", 0, 0, 100) / 100.0;
            s.swingUnit = intAttr(xml, "unit", s.swingUnit, 8, 16);
            if (s.swingUnit != 8 && s.swingUnit != 16)
                xml.raiseError(QStringLiteral("<swing unit> must be 8 or 16"));
            xml.skipCurrentElement();
        } else if (name == QLatin1String("crash")) {
            s.crashNote = drumAttr(xml, s.crashNote);
            xml.skipCurrentElement();
        } else if (name == QLatin1String("drums")) {
            s.drums = readRows(xml);
        } else if (name == QLatin1String("fill")) {
            s.fill = readRows(xml);
        } else if (name == QLatin1String("bass")) {
            s.bassProgram = intAttr(xml, "program", s.bassProgram, 0, 127);
            s.bassVelocity = intAttr(xml, "velocity", s.bassVelocity, 1, 127);
            s.bass = xml.readElementText().toStdString();
        } else if (name == QLatin1String("comp")) {
            s.pianoProgram = intAttr(xml, "program", s.pianoProgram, 0, 127);
            s.compVelocity = intAttr(xml, "velocity", s.compVelocity, 1, 127);
            const auto voicing = xml.attributes().value(QLatin1String("voicing"));
            if (voicing == QLatin1String("power"))
                s.powerChords = true;
            else if (!voicing.isEmpty() && voicing != QLatin1String("close"))
                xml.raiseError(QStringLiteral("<comp voicing> must be \"close\" or \"power\""));
            s.comp = xml.readElementText().toStdString();
        } else if (name == QLatin1String("pad")) {
            s.padProgram = intAttr(xml, "program", s.padProgram, 0, 127);
            s.padDefault = xml.attributes().value(QLatin1String("enabled")) == QLatin1String("true");
            xml.skipCurrentElement();
        } else {
            xml.skipCurrentElement(); // unknown elements are ignored for forward compatibility
        }
    }

    s = mc::normalizedStyle(s);
    // A missing bass or comp part means the instrument rests.
    if (s.bass.empty())
        s.bass.assign(s.stepCount(), '.');
    if (s.comp.empty())
        s.comp.assign(s.stepCount(), '.');
    return s;
}

// Groups the steps by beat (by two beats for coarse grids) and separates
// the bars with '|', the way hand-written files look.
QString formatPattern(const std::string &steps, const mc::Style &s)
{
    int group = s.stepsPerBeat;
    if (s.stepsPerBeat <= 2 && s.beatsPerBar % 2 == 0)
        group *= 2;
    QString text;
    for (size_t i = 0; i < steps.size(); ++i) {
        if (i > 0) {
            if (i % s.stepsPerBar() == 0)
                text += QLatin1String(" | ");
            else if (i % group == 0)
                text += QLatin1Char(' ');
        }
        text += QLatin1Char(steps[i]);
    }
    return text;
}

void writeDrumAttr(QXmlStreamWriter &xml, int note)
{
    const QString name = drumName(note);
    if (note == 0)
        xml.writeAttribute("drum", "none");
    else if (!name.isEmpty())
        xml.writeAttribute("drum", name);
    else
        xml.writeAttribute("note", QString::number(note));
}

void writeRows(QXmlStreamWriter &xml, const char *element, const std::vector<mc::DrumRow> &rows, const mc::Style &s)
{
    xml.writeStartElement(element);
    for (const auto &r : rows) {
        xml.writeStartElement("row");
        writeDrumAttr(xml, r.note);
        xml.writeCharacters(formatPattern(r.steps, s));
        xml.writeEndElement();
    }
    xml.writeEndElement();
}

void writeStyle(QXmlStreamWriter &xml, const mc::Style &s)
{
    xml.writeStartElement("style");
    xml.writeAttribute("name", QString::fromStdString(s.name));

    xml.writeEmptyElement("meter");
    xml.writeAttribute("beats", QString::number(s.beatsPerBar));
    xml.writeAttribute("steps-per-beat", QString::number(s.stepsPerBeat));
    xml.writeAttribute("bars", QString::number(s.bars));
    xml.writeTextElement("tempo", QString::number(s.tempo));
    xml.writeEmptyElement("swing");
    xml.writeAttribute("amount", QString::number(qRound(s.swing * 100)));
    xml.writeAttribute("unit", QString::number(s.swingUnit));
    xml.writeEmptyElement("crash");
    writeDrumAttr(xml, s.crashNote);

    writeRows(xml, "drums", s.drums, s);
    if (!s.fill.empty())
        writeRows(xml, "fill", s.fill, s);

    xml.writeStartElement("bass");
    xml.writeAttribute("program", QString::number(s.bassProgram));
    xml.writeAttribute("velocity", QString::number(s.bassVelocity));
    xml.writeCharacters(formatPattern(s.bass, s));
    xml.writeEndElement();

    xml.writeStartElement("comp");
    xml.writeAttribute("program", QString::number(s.pianoProgram));
    xml.writeAttribute("velocity", QString::number(s.compVelocity));
    if (s.powerChords)
        xml.writeAttribute("voicing", "power");
    xml.writeCharacters(formatPattern(s.comp, s));
    xml.writeEndElement();

    xml.writeEmptyElement("pad");
    xml.writeAttribute("program", QString::number(s.padProgram));
    xml.writeAttribute("enabled", s.padDefault ? "true" : "false");

    xml.writeEndElement();
}

} // namespace

bool writeStyleFile(const QString &path, const std::vector<mc::Style> &styles, QString &error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        error = file.errorString();
        return false;
    }
    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(4);
    xml.writeStartDocument();
    xml.writeStartElement(RootElement);
    xml.writeAttribute("version", QString::number(FormatVersion));
    for (const auto &s : styles)
        writeStyle(xml, mc::normalizedStyle(s));
    xml.writeEndElement();
    xml.writeEndDocument();
    if (xml.hasError() || !file.commit()) {
        error = file.errorString();
        return false;
    }
    return true;
}

bool readStyleFile(const QString &path, std::vector<mc::Style> &styles, QString &error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = file.errorString();
        return false;
    }

    QXmlStreamReader xml(&file);
    std::vector<mc::Style> result;
    if (xml.readNextStartElement()) {
        if (xml.name() != QLatin1String(RootElement)) {
            xml.raiseError(QStringLiteral("Not a MidiCombo style file"));
        } else if (xml.attributes().value(QLatin1String("version")).toInt() > FormatVersion) {
            xml.raiseError(QStringLiteral("The file was written for a newer version of MidiCombo"));
        } else {
            while (xml.readNextStartElement() && !xml.hasError()) {
                if (xml.name() != QLatin1String("style")) {
                    xml.skipCurrentElement();
                    continue;
                }
                const qint64 line = xml.lineNumber();
                mc::Style style = readStyle(xml);
                std::string invalid;
                if (!xml.hasError() && !mc::validateStyle(style, invalid))
                    xml.raiseError(QStringLiteral("Style starting at line %1: %2")
                                       .arg(line).arg(QString::fromStdString(invalid)));
                result.push_back(std::move(style));
            }
        }
    }

    if (xml.hasError()) {
        error = QStringLiteral("Line %1: %2").arg(xml.lineNumber()).arg(xml.errorString());
        return false;
    }
    if (result.empty()) {
        error = QStringLiteral("The file contains no styles");
        return false;
    }
    styles = std::move(result);
    return true;
}

QStringList drumNames()
{
    QStringList names;
    for (const auto &d : kDrums)
        names << QLatin1String(d.name);
    return names;
}

QString drumName(int note)
{
    for (const auto &d : kDrums)
        if (d.note == note)
            return QLatin1String(d.name);
    return {};
}

int drumNote(const QString &name)
{
    for (const auto &d : kDrums)
        if (name == QLatin1String(d.name))
            return d.note;
    return -1;
}
