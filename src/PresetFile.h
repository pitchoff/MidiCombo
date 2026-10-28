#pragma once

#include <QList>
#include <QString>

// A song preset as stored in a preset file. Optional values (-1 / present
// = false) leave the current setting or the style default untouched.
struct PresetData {
    struct Part {
        bool present = false;
        bool enabled = true;
        int program = 0;
        int volume = 100;
    };
    static constexpr int PartCount = 4; // same order as BandEngine::Part
    static const char *const partKeys[PartCount];

    QString name;
    QString style;
    int tempo = -1;
    int swing = -1;     // percent
    int transpose = 0;
    QString progression;
    Part parts[PartCount];
};

// File format:
//
// <midicombo-presets version="1">
//   <preset name="My Blues">
//     <style>Shuffle Blues</style>
//     <tempo>100</tempo>
//     <swing>0</swing>
//     <transpose>2</transpose>
//     <progression>| F7 | Bb7 | F7 | F7 |</progression>
//     <band>
//       <part name="bass" enabled="true" program="33" volume="100"/>
//     </band>
//   </preset>
// </midicombo-presets>
bool readPresetFile(const QString &path, QList<PresetData> &presets, QString &error);
bool writePresetFile(const QString &path, const QList<PresetData> &presets, QString &error);
