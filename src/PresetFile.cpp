#include "PresetFile.h"

#include <QFile>
#include <QSaveFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

const char *const PresetData::partKeys[PresetData::PartCount] = {"drums", "bass", "piano", "pad"};

namespace {

constexpr auto RootElement = "midicombo-presets";
constexpr int FormatVersion = 1;

int partIndex(QStringView key)
{
    for (int i = 0; i < PresetData::PartCount; ++i)
        if (key == QLatin1String(PresetData::partKeys[i]))
            return i;
    return -1;
}

int readInt(QXmlStreamReader &xml, int fallback)
{
    bool ok = false;
    const int v = xml.readElementText().trimmed().toInt(&ok);
    if (!ok)
        xml.raiseError(QStringLiteral("'%1' needs a number").arg(xml.name()));
    return ok ? v : fallback;
}

void readBand(QXmlStreamReader &xml, PresetData &preset)
{
    while (xml.readNextStartElement()) {
        if (xml.name() != QLatin1String("part")) {
            xml.skipCurrentElement();
            continue;
        }
        const auto attrs = xml.attributes();
        const int idx = partIndex(attrs.value(QLatin1String("name")));
        if (idx >= 0) {
            PresetData::Part &part = preset.parts[idx];
            part.present = true;
            if (attrs.hasAttribute(QLatin1String("enabled")))
                part.enabled = attrs.value(QLatin1String("enabled")) == QLatin1String("true");
            if (attrs.hasAttribute(QLatin1String("program")))
                part.program = attrs.value(QLatin1String("program")).toInt();
            if (attrs.hasAttribute(QLatin1String("volume")))
                part.volume = attrs.value(QLatin1String("volume")).toInt();
        }
        xml.skipCurrentElement();
    }
}

PresetData readPreset(QXmlStreamReader &xml)
{
    PresetData preset;
    preset.name = xml.attributes().value(QLatin1String("name")).toString();
    while (xml.readNextStartElement()) {
        const auto name = xml.name();
        if (name == QLatin1String("style"))
            preset.style = xml.readElementText().trimmed();
        else if (name == QLatin1String("tempo"))
            preset.tempo = readInt(xml, -1);
        else if (name == QLatin1String("swing"))
            preset.swing = readInt(xml, -1);
        else if (name == QLatin1String("transpose"))
            preset.transpose = readInt(xml, 0);
        else if (name == QLatin1String("progression"))
            preset.progression = xml.readElementText().trimmed();
        else if (name == QLatin1String("band"))
            readBand(xml, preset);
        else
            xml.skipCurrentElement(); // unknown elements are ignored for forward compatibility
    }
    return preset;
}

} // namespace

bool readPresetFile(const QString &path, QList<PresetData> &presets, QString &error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = file.errorString();
        return false;
    }

    QXmlStreamReader xml(&file);
    QList<PresetData> result;
    if (xml.readNextStartElement()) {
        if (xml.name() != QLatin1String(RootElement)) {
            xml.raiseError(QStringLiteral("Not a MidiCombo preset file"));
        } else if (xml.attributes().value(QLatin1String("version")).toInt() > FormatVersion) {
            xml.raiseError(QStringLiteral("The file was written by a newer version of MidiCombo"));
        } else {
            while (xml.readNextStartElement()) {
                if (xml.name() == QLatin1String("preset"))
                    result.append(readPreset(xml));
                else
                    xml.skipCurrentElement();
            }
        }
    }

    if (xml.hasError()) {
        error = QStringLiteral("Line %1: %2").arg(xml.lineNumber()).arg(xml.errorString());
        return false;
    }
    if (result.isEmpty()) {
        error = QStringLiteral("The file contains no presets");
        return false;
    }
    presets = result;
    return true;
}

bool writePresetFile(const QString &path, const QList<PresetData> &presets, QString &error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        error = file.errorString();
        return false;
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QLatin1String(RootElement));
    xml.writeAttribute(QLatin1String("version"), QString::number(FormatVersion));

    for (const PresetData &p : presets) {
        xml.writeStartElement(QLatin1String("preset"));
        xml.writeAttribute(QLatin1String("name"), p.name);
        if (!p.style.isEmpty())
            xml.writeTextElement(QLatin1String("style"), p.style);
        if (p.tempo > 0)
            xml.writeTextElement(QLatin1String("tempo"), QString::number(p.tempo));
        if (p.swing >= 0)
            xml.writeTextElement(QLatin1String("swing"), QString::number(p.swing));
        xml.writeTextElement(QLatin1String("transpose"), QString::number(p.transpose));
        xml.writeTextElement(QLatin1String("progression"), p.progression);

        xml.writeStartElement(QLatin1String("band"));
        for (int i = 0; i < PresetData::PartCount; ++i) {
            const PresetData::Part &part = p.parts[i];
            if (!part.present)
                continue;
            xml.writeEmptyElement(QLatin1String("part"));
            xml.writeAttribute(QLatin1String("name"), QLatin1String(PresetData::partKeys[i]));
            xml.writeAttribute(QLatin1String("enabled"), part.enabled ? QStringLiteral("true") : QStringLiteral("false"));
            xml.writeAttribute(QLatin1String("program"), QString::number(part.program));
            xml.writeAttribute(QLatin1String("volume"), QString::number(part.volume));
        }
        xml.writeEndElement(); // band
        xml.writeEndElement(); // preset
    }

    xml.writeEndElement();
    xml.writeEndDocument();

    if (!file.commit()) {
        error = file.errorString();
        return false;
    }
    return true;
}
