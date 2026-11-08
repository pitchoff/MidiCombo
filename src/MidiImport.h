#pragma once

#include "MidiFile.h"
#include "Style.h"

#include <string>
#include <vector>

namespace mc {

// What a MIDI channel is used for when importing.
enum class ImportRole { None, Drums, Bass, Comp, Guitar, Pad, Melody };
const char *importRoleName(ImportRole role);

struct ImportOptions {
    std::string name;          // style and song name; empty = title or "Imported"
    int bars = 2;              // pattern length in bars
    int stepsPerBeat = 0;      // 0 = detect from the note timing
    int maxVariations = 2;
    // Channel (0..15) for each role, -1 = detect, -2 = none.
    int drumChannel = -1;
    int bassChannel = -1;
    int compChannel = -1;
    int guitarChannel = -1;
    int padChannel = -1;
};

struct ImportResult {
    Style style;
    std::string progression;   // song text, with variation markers
    int tempo = 120;           // style beats per minute
    int swingPercent = 0;
    ImportRole roles[16] = {};
    int programs[16];          // GM program per channel, -1 = none
    int firstBar = 0;          // first bar of the file used for the song
    int songBars = 0;
    std::vector<std::string> report; // what was found, for the user

    ImportResult();
};

// Turns the notes of a MIDI file into a style (main pattern, fill and
// variations) and a song (tempo and chord progression).
bool importMidi(const MidiFileData &midi, const ImportOptions &options, ImportResult &result, std::string &error);

} // namespace mc
