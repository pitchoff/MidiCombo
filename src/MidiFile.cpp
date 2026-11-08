#include "MidiFile.h"

#include <algorithm>
#include <deque>
#include <fstream>
#include <iterator>
#include <map>
#include <tuple>
#include <utility>

namespace mc {

MidiFileData::MidiFileData()
{
    std::fill(std::begin(programs), std::end(programs), -1);
}

namespace {

class Reader {
public:
    Reader(const std::vector<unsigned char> &bytes, size_t begin, size_t end)
        : m_bytes(bytes), m_pos(begin), m_end(end) {}

    bool atEnd() const { return m_pos >= m_end; }
    bool ok() const { return m_ok; }
    size_t pos() const { return m_pos; }

    int byte()
    {
        if (m_pos >= m_end) {
            m_ok = false;
            return 0;
        }
        return m_bytes[m_pos++];
    }
    int peek() const { return m_pos < m_end ? m_bytes[m_pos] : 0; }
    unsigned long be(int count)
    {
        unsigned long v = 0;
        for (int i = 0; i < count; ++i)
            v = (v << 8) | static_cast<unsigned>(byte());
        return v;
    }
    unsigned long varLen()
    {
        unsigned long v = 0;
        for (int i = 0; i < 4; ++i) {
            const int b = byte();
            v = (v << 7) | (b & 0x7f);
            if (!(b & 0x80))
                return v;
        }
        m_ok = false; // longer than four bytes
        return v;
    }
    void skip(unsigned long count)
    {
        if (count > m_end - m_pos) {
            m_ok = false;
            m_pos = m_end;
        } else {
            m_pos += count;
        }
    }
    std::string text(unsigned long count)
    {
        std::string s;
        for (unsigned long i = 0; i < count && ok(); ++i)
            s += static_cast<char>(byte());
        return s;
    }

private:
    const std::vector<unsigned char> &m_bytes;
    size_t m_pos;
    size_t m_end;
    bool m_ok = true;
};

struct TimedValue {
    long tick;
    double value;
};

} // namespace

bool parseMidiFile(const std::vector<unsigned char> &bytes, MidiFileData &out, std::string &error)
{
    out = MidiFileData();
    Reader file(bytes, 0, bytes.size());
    if (file.text(4) != "MThd") {
        error = "Not a Standard MIDI File";
        return false;
    }
    const unsigned long headerLength = file.be(4);
    const int format = static_cast<int>(file.be(2));
    const int trackCount = static_cast<int>(file.be(2));
    const int division = static_cast<int>(file.be(2));
    if (!file.ok() || headerLength < 6) {
        error = "The file header is truncated";
        return false;
    }
    file.skip(headerLength - 6);
    if (format > 1) {
        error = "MIDI file format " + std::to_string(format) + " is not supported (only 0 and 1)";
        return false;
    }
    if (division & 0x8000 || division == 0) {
        error = "MIDI files with SMPTE timing are not supported";
        return false;
    }
    out.ticksPerQuarter = division;

    std::vector<TimedValue> tempos;                // microseconds per quarter
    std::vector<std::pair<long, std::pair<int, int>>> meters;
    std::vector<std::tuple<long, int, int>> programs; // tick, channel, program

    for (int t = 0; t < trackCount && !file.atEnd(); ++t) {
        const std::string id = file.text(4);
        const unsigned long length = file.be(4);
        if (!file.ok()) {
            error = "Track " + std::to_string(t + 1) + " is truncated";
            return false;
        }
        if (id != "MTrk") { // unknown chunks are skipped, as the standard asks
            file.skip(length);
            --t;
            continue;
        }
        const size_t begin = file.pos();
        const size_t end = std::min(bytes.size(), begin + length);
        file.skip(length);

        Reader r(bytes, begin, end);
        long tick = 0;
        int status = 0;
        std::map<std::pair<int, int>, std::deque<size_t>> open; // (channel, note) -> note indices
        while (!r.atEnd() && r.ok()) {
            tick += static_cast<long>(r.varLen());
            int b = r.peek();
            if (b & 0x80) {
                r.byte();
                if (b < 0xf0)
                    status = b; // running status applies to channel messages only
            } else if (status == 0) {
                error = "Track " + std::to_string(t + 1) + ": data without a status byte";
                return false;
            } else {
                b = status;
            }

            if (b == 0xff) {
                const int type = r.byte();
                const unsigned long len = r.varLen();
                if (type == 0x51 && len == 3) {
                    tempos.push_back({tick, static_cast<double>(r.be(3))});
                } else if (type == 0x58 && len >= 2) {
                    const int num = r.byte();
                    const int den = 1 << std::min(r.byte(), 6);
                    r.skip(len - 2);
                    meters.push_back({tick, {num, den}});
                } else if (type == 0x03 && t == 0 && out.title.empty()) { // the first track's name is the title
                    out.title = r.text(len);
                } else if (type == 0x2f) {
                    r.skip(len);
                    break;
                } else {
                    r.skip(len);
                }
                continue;
            }
            if (b == 0xf0 || b == 0xf7) {
                r.skip(r.varLen());
                continue;
            }
            if (b > 0xf0) {
                error = "Track " + std::to_string(t + 1) + ": unexpected status byte";
                return false;
            }

            const int channel = b & 0x0f;
            switch (b & 0xf0) {
            case 0x80:
            case 0x90: {
                const int note = r.byte() & 0x7f;
                const int velocity = r.byte() & 0x7f;
                auto &queue = open[{channel, note}];
                if ((b & 0xf0) == 0x90 && velocity > 0) {
                    queue.push_back(out.notes.size());
                    out.notes.push_back({tick, tick, channel, note, velocity});
                } else if (!queue.empty()) {
                    out.notes[queue.front()].endTick = tick;
                    queue.pop_front();
                }
                break;
            }
            case 0xc0:
                programs.emplace_back(tick, channel, r.byte() & 0x7f);
                break;
            case 0xd0:
                r.byte();
                break;
            default: // 0xa0, 0xb0, 0xe0
                r.byte();
                r.byte();
                break;
            }
        }
        if (!r.ok()) {
            error = "Track " + std::to_string(t + 1) + " is truncated";
            return false;
        }
        for (auto &entry : open) // notes that are never released end with the track
            for (size_t idx : entry.second)
                out.notes[idx].endTick = tick;
        out.endTick = std::max(out.endTick, tick);
    }

    std::sort(out.notes.begin(), out.notes.end(), [](const MidiNote &a, const MidiNote &b) {
        return std::tie(a.tick, a.channel, a.note) < std::tie(b.tick, b.channel, b.note);
    });
    for (auto &n : out.notes) {
        if (n.endTick <= n.tick)
            n.endTick = n.tick + 1;
        out.endTick = std::max(out.endTick, n.endTick);
    }

    std::stable_sort(programs.begin(), programs.end(),
                     [](const auto &a, const auto &b) { return std::get<0>(a) < std::get<0>(b); });
    for (const auto &p : programs)
        if (out.programs[std::get<1>(p)] < 0)
            out.programs[std::get<1>(p)] = std::get<2>(p);

    // The tempo that lasts longest.
    std::stable_sort(tempos.begin(), tempos.end(), [](const TimedValue &a, const TimedValue &b) { return a.tick < b.tick; });
    if (tempos.empty() || tempos.front().tick > 0)
        tempos.insert(tempos.begin(), {0, 500000.0});
    std::map<double, long> tempoLength;
    for (size_t i = 0; i < tempos.size(); ++i) {
        const long until = i + 1 < tempos.size() ? tempos[i + 1].tick : std::max(out.endTick, tempos[i].tick + 1);
        tempoLength[tempos[i].value] += until - tempos[i].tick;
    }
    const auto longest = std::max_element(tempoLength.begin(), tempoLength.end(),
                                          [](const auto &a, const auto &b) { return a.second < b.second; });
    out.quarterBpm = 60000000.0 / longest->first;
    out.tempoChanges = tempoLength.size() > 1;

    std::stable_sort(meters.begin(), meters.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
    if (!meters.empty()) {
        out.meterNumerator = std::max(1, meters.front().second.first);
        out.meterDenominator = meters.front().second.second;
        for (const auto &m : meters)
            if (m.second != meters.front().second)
                out.meterChanges = true;
    }
    return true;
}

bool readMidiFile(const std::string &path, MidiFileData &out, std::string &error)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "Cannot open " + path;
        return false;
    }
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return parseMidiFile(bytes, out, error);
}

} // namespace mc
