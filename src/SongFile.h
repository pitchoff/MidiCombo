#pragma once

#include <QList>
#include <QString>

// A song as stored in a song file. Optional values (-1 / present = false)
// leave the current setting or the style default untouched.
struct SongData {
    struct Part {
        bool present = false;
        bool enabled = true;
        int program = 0;
        int volume = 100;
    };
    static constexpr int PartCount = 5; // same order as BandEngine::Part
    static const char *const partKeys[PartCount];

    QString name;
    QString style;
    int tempo = -1;
    int swing = -1;     // percent
    int transpose = 0;
    QString variation;       // "in-turn", "random", "Main" or a variation name; empty = keep
    int variationEvery = -1; // bars
    QString progression;
    Part parts[PartCount];
};

// File format:
//
// <midicombo-songs version="1">
//   <song name="My Blues">
//     <style>Shuffle Blues</style>
//     <tempo>100</tempo>
//     <swing>0</swing>
//     <transpose>2</transpose>
//     <variation select="in-turn" every="8"/>
//     <progression>| F7 | Bb7 | F7 | F7 |</progression>
//     <band>
//       <part name="bass" enabled="true" program="33" volume="100"/>
//     </band>
//   </song>
// </midicombo-songs>
//
// Files of older versions (<midicombo-presets> with <preset> elements) are
// read as well.
bool readSongFile(const QString &path, QList<SongData> &songs, QString &error);
bool writeSongFile(const QString &path, const QList<SongData> &songs, QString &error);
