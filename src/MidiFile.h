#pragma once

#include <string>
#include <vector>

namespace mc {

// A note of a Standard MIDI File, with absolute times in ticks.
struct MidiNote {
    long tick = 0;
    long endTick = 0;
    int channel = 0; // 0..15
    int note = 0;
    int velocity = 0;
};

// The contents of a Standard MIDI File (format 0 or 1) that matter for
// importing: all notes of all tracks, the first program of every channel,
// tempo and meter.
struct MidiFileData {
    int ticksPerQuarter = 480;
    std::vector<MidiNote> notes; // sorted by tick, then channel and note
    int programs[16];            // first program change per channel, -1 = none
    double quarterBpm = 120.0;   // the tempo that lasts longest
    bool tempoChanges = false;
    int meterNumerator = 4;      // the first time signature
    int meterDenominator = 4;
    bool meterChanges = false;
    long endTick = 0;
    std::string title;           // name of the first track, usually the song title

    MidiFileData();
};

bool parseMidiFile(const std::vector<unsigned char> &bytes, MidiFileData &out, std::string &error);
bool readMidiFile(const std::string &path, MidiFileData &out, std::string &error);

} // namespace mc
