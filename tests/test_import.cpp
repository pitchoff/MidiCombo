// Tests the MIDI file reader and the import into a style and a song, with a
// MIDI file written here: 16 bars of C | G | Am | F with drums, bass and
// piano. Bars 1-8 are a hi-hat groove with a tom fill in every 4th bar,
// bars 9-16 a ride groove with the kick on every beat.

#include "MidiImport.h"
#include "Progression.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const std::string &what)
{
    if (!ok) {
        ++failures;
        std::cerr << "FAIL: " << what << "\n";
    }
}

struct Event {
    long tick;
    std::vector<unsigned char> data;
};

void varLen(std::vector<unsigned char> &out, unsigned long v)
{
    unsigned char buf[4];
    int n = 0;
    do {
        buf[n++] = v & 0x7f;
        v >>= 7;
    } while (v);
    while (n--)
        out.push_back(buf[n] | (n ? 0x80 : 0));
}

std::vector<unsigned char> track(std::vector<Event> events)
{
    std::stable_sort(events.begin(), events.end(), [](const Event &a, const Event &b) { return a.tick < b.tick; });
    std::vector<unsigned char> body;
    long now = 0;
    for (const Event &e : events) {
        varLen(body, e.tick - now);
        now = e.tick;
        body.insert(body.end(), e.data.begin(), e.data.end());
    }
    body.insert(body.end(), {0x00, 0xff, 0x2f, 0x00});
    std::vector<unsigned char> out = {'M', 'T', 'r', 'k'};
    const unsigned long len = body.size();
    for (int i = 3; i >= 0; --i)
        out.push_back((len >> (8 * i)) & 0xff);
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

void noteAt(std::vector<Event> &ev, int channel, long tick, long length, int note, int velocity)
{
    ev.push_back({tick, {static_cast<unsigned char>(0x90 | channel), static_cast<unsigned char>(note),
                         static_cast<unsigned char>(velocity)}});
    // Note off as note on with velocity 0, using running status where it can.
    ev.push_back({tick + length, {static_cast<unsigned char>(0x90 | channel), static_cast<unsigned char>(note), 0}});
}

std::vector<unsigned char> songFile()
{
    const long q = 480, bar = 4 * q, e = q / 2;
    std::vector<Event> meta = {
        {0, {0xff, 0x03, 9, 'T', 'e', 's', 't', ' ', 'S', 'o', 'n', 'g'}},
        {0, {0xff, 0x51, 0x03, 0x07, 0xa1, 0x20}}, // 500000 us = 120 BPM
        {0, {0xff, 0x58, 0x04, 4, 2, 24, 8}},      // 4/4
    };
    std::vector<Event> drums, bass, piano;
    bass.push_back({0, {0xc1, 33}});
    piano.push_back({0, {0xc0, 0}});
    const int roots[4] = {48, 43, 45, 41}; // C G A F
    const std::vector<std::vector<int>> chords = {{60, 64, 67}, {59, 62, 67}, {57, 60, 64}, {57, 60, 65}};
    for (int b = 0; b < 16; ++b) {
        const long t = b * bar;
        const bool ride = b >= 8;
        const bool fill = b % 4 == 3 && !ride;
        for (int i = 0; i < 8; ++i) {
            if (fill) {
                noteAt(drums, 9, t + i * e, 60, i < 4 ? 50 : 45, 100);
            } else if (ride) {
                if (i % 2 == 0)
                    noteAt(drums, 9, t + i * e, 60, 51, 90);
            } else {
                noteAt(drums, 9, t + i * e, 60, 42, 80);
            }
        }
        if (!fill) {
            for (int beat = 0; beat < 4; ++beat) {
                if (ride || beat % 2 == 0)
                    noteAt(drums, 9, t + beat * q, 60, 36, 100);
                if (beat % 2 == 1)
                    noteAt(drums, 9, t + beat * q, 60, 38, 100);
            }
        }
        const int root = roots[b % 4];
        for (int beat = 0; beat < 4; ++beat)
            noteAt(bass, 1, t + beat * q, q - 20, beat % 2 ? root + 7 : root, 96);
        for (int half = 0; half < 2; ++half)
            for (int n : chords[b % 4])
                noteAt(piano, 0, t + half * 2 * q, 2 * q - 20, n, 80);
    }

    std::vector<unsigned char> file = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, 4, 0x01, 0xe0};
    for (auto *events : {&meta, &drums, &bass, &piano}) {
        const auto t = track(*events);
        file.insert(file.end(), t.begin(), t.end());
    }
    return file;
}

} // namespace

int main()
{
    std::string error;
    mc::MidiFileData midi;
    check(mc::parseMidiFile(songFile(), midi, error), "parse: " + error);
    check(midi.ticksPerQuarter == 480, "division");
    check(midi.title == "Test Song", "title: " + midi.title);
    check(std::abs(midi.quarterBpm - 120.0) < 0.01, "tempo");
    check(midi.meterNumerator == 4 && midi.meterDenominator == 4, "meter");
    check(midi.programs[1] == 33 && midi.programs[0] == 0 && midi.programs[5] == -1, "programs");
    check(!midi.notes.empty() && midi.notes.front().endTick > midi.notes.front().tick, "notes have lengths");

    mc::ImportOptions options;
    mc::ImportResult r;
    check(mc::importMidi(midi, options, r, error), "import: " + error);
    const mc::Style &s = r.style;
    check(s.name == "Test Song", "style name from the title");
    check(r.tempo == 120 && s.tempo == 120, "imported tempo");
    check(s.beatsPerBar == 4 && s.stepsPerBeat == 2 && s.bars == 2, "grid: eighths, 2-bar patterns");
    check(r.roles[9] == mc::ImportRole::Drums && r.roles[1] == mc::ImportRole::Bass && r.roles[0] == mc::ImportRole::Comp,
          "channel roles");
    check(s.bass == "R-5-R-5-R-5-R-5-", "bass pattern: " + s.bass);
    check(s.comp == "C---C---C---C---", "comp pattern: " + s.comp);
    check(s.bassProgram == 33 && s.pianoProgram == 0, "programs");
    check(s.guitar.empty(), "no guitar in the file");

    auto row = [](const std::vector<mc::DrumRow> &rows, int note) {
        for (const auto &r : rows)
            if (r.note == note)
                return r.steps;
        return std::string();
    };
    check(row(s.drums, 42) == "xxxxxxxxxxxxxxxx", "hi-hat row: " + row(s.drums, 42));
    check(row(s.drums, 36) == "X...X...X...X...", "kick row: " + row(s.drums, 36));
    check(row(s.drums, 38) == "..X...X...X...X.", "snare row: " + row(s.drums, 38));
    check(row(s.drums, 50).empty(), "the fill is not in the main pattern");
    check(row(s.fill, 50) == "XXXX...." && row(s.fill, 45) == "....XXXX", "fill: " + row(s.fill, 50) + " " + row(s.fill, 45));
    check(s.drums.size() == 3 && s.drums.back().note == 36, "rows: cymbals first, kick last");

    check(s.variations.size() == 1, "one variation");
    if (s.variations.size() == 1) {
        const auto &v = s.variations[0];
        check(v.name == "B", "variation name");
        check(row(v.drums, 51) == "x.x.x.x.x.x.x.x." && row(v.drums, 36) == "X.X.X.X.X.X.X.X.",
              "variation drums: " + row(v.drums, 51) + " " + row(v.drums, 36));
        check(v.bass.empty() && v.comp.empty(), "unchanged parts are inherited");
    }

    mc::Progression p;
    check(mc::Progression::parse(r.progression, p, error), "progression parses: " + error);
    check(p.size() == 16, "16 bars");
    const char *expected[4] = {"C", "G", "Am", "F"};
    for (int b = 0; b < p.size(); ++b)
        check(p.chordAt(b, 0, 48).name() == expected[b % 4], "chord of bar " + std::to_string(b + 1) + ": " +
                                                              p.chordAt(b, 0, 48).name());
    check(p.variationAt(0) == "Main" && p.variationAt(7) == "Main" && p.variationAt(8) == "B" && p.variationAt(15) == "B",
          "variation markers");

    // Options: one-bar patterns, no variations, the guitar channel forced to "none".
    options.bars = 1;
    options.maxVariations = 0;
    options.name = "Other";
    check(mc::importMidi(midi, options, r, error), "import with options: " + error);
    check(r.style.bars == 1 && r.style.variations.empty() && r.style.name == "Other", "options are used");
    check(r.progression.find('[') == std::string::npos, "no markers without variations");

    // Errors
    check(!mc::parseMidiFile({'R', 'I', 'F', 'F'}, midi, error), "not a MIDI file");
    std::vector<unsigned char> truncated = songFile();
    truncated.resize(truncated.size() - 40);
    check(!mc::parseMidiFile(truncated, midi, error), "truncated file");
    std::vector<unsigned char> smpte = songFile();
    smpte[12] = 0xe7; // -25 fps
    check(!mc::parseMidiFile(smpte, midi, error), "SMPTE timing");
    mc::MidiFileData empty;
    check(!mc::importMidi(empty, {}, r, error), "no notes");

    if (failures == 0)
        std::cout << "All import tests passed\n";
    return failures == 0 ? 0 : 1;
}
