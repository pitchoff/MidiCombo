#include "MidiImport.h"

#include "Chord.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iterator>
#include <map>
#include <set>
#include <sstream>

namespace mc {

const char *importRoleName(ImportRole role)
{
    switch (role) {
    case ImportRole::Drums: return "drums";
    case ImportRole::Bass: return "bass";
    case ImportRole::Comp: return "comp";
    case ImportRole::Guitar: return "guitar";
    case ImportRole::Pad: return "pad";
    case ImportRole::Melody: return "melody (ignored)";
    default: return "chords only";
    }
}

ImportResult::ImportResult()
{
    std::fill(std::begin(programs), std::end(programs), -1);
}

namespace {

constexpr int Crash = 49;
constexpr int Crash2 = 57;

struct ChannelInfo {
    std::vector<const MidiNote *> notes;
    double avgPitch = 0;
    double polyphony = 1; // average notes per onset
    double avgBeats = 0;  // average note length in beats
    int medianVelocity = 80;
    int program = -1;
};

int median(std::vector<int> v, int fallback)
{
    if (v.empty())
        return fallback;
    std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
    return v[v.size() / 2];
}

double dice(const std::set<long> &a, const std::set<long> &b)
{
    if (a.empty() && b.empty())
        return 1.0;
    std::vector<long> common;
    std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(common));
    return 2.0 * common.size() / (a.size() + b.size());
}

bool inRange(int program, int from, int to) { return program >= from && program <= to; }

// One block of `bars` bars, cut out of the file as patterns.
struct Block {
    int startBar = 0;
    std::map<int, std::string> drums; // note -> steps
    std::string bass, comp, guitar;
    std::set<long> cells;             // (part, note, step) of every onset, for comparing
    std::set<long> lastBarDrums;
    int cluster = -1;
    bool fill = false;
};

long cellKey(int part, int note, int step) { return (static_cast<long>(part) * 128 + note) * 4096 + step; }
int cellStep(long key) { return static_cast<int>(key % 4096); }

std::set<long> cellsBefore(const std::set<long> &cells, int step)
{
    std::set<long> out;
    for (long c : cells)
        if (cellStep(c) < step)
            out.insert(c);
    return out;
}

class Importer {
public:
    Importer(const MidiFileData &midi, const ImportOptions &options, ImportResult &result)
        : m_midi(midi), m_opt(options), m_res(result) {}

    bool run(std::string &error);

private:
    void collectChannels();
    void assignRoles();
    void detectMeter();
    void detectGrid();
    void detectChords();
    std::string barChords(int bar) const;
    void cutBlocks();
    void cluster();
    void buildStyle();
    void buildProgression();

    Chord chordAt(double tick) const;
    int stepOf(double tick, double blockStart) const { return static_cast<int>(std::lround((tick - blockStart) / m_step)); }
    std::vector<std::vector<const MidiNote *>> groups(int channel, double blockStart) const;
    std::map<int, std::string> drumPattern(double blockStart) const;
    std::string bassPattern(double blockStart) const;
    std::string compPattern(double blockStart) const;
    std::string guitarPattern(double blockStart) const;
    int channelOf(ImportRole role) const;
    void note(const std::string &line) { m_res.report.push_back(line); }

    const MidiFileData &m_midi;
    const ImportOptions &m_opt;
    ImportResult &m_res;

    ChannelInfo m_ch[16];
    int m_beats = 4;
    double m_beatTicks = 480;
    int m_spb = 2;
    double m_step = 240;
    double m_swing = 0;
    int m_swingUnit = 8;
    int m_bars = 2;          // pattern length
    int m_stepsPerBar = 8;
    int m_firstBar = 0, m_lastBar = 0;
    std::vector<Chord> m_beatChords;
    std::vector<Block> m_blocks;
    int m_clusterCount = 0;
    std::vector<int> m_medoids;
    std::map<int, std::string> m_fillRows; // drum note -> one bar; empty = no fill
    double m_drumScale = 1.0;
    GuitarVoicing m_voicing = GuitarVoicing::Full;
    bool m_crash = false;
};

void Importer::collectChannels()
{
    for (const MidiNote &n : m_midi.notes)
        m_ch[n.channel].notes.push_back(&n);
    for (int c = 0; c < 16; ++c) {
        ChannelInfo &ci = m_ch[c];
        ci.program = m_midi.programs[c];
        m_res.programs[c] = ci.program;
        if (ci.notes.empty())
            continue;
        double pitch = 0, length = 0;
        std::vector<int> vel;
        int onsets = 0;
        long lastOnset = -1000000;
        const long together = std::max(1, m_midi.ticksPerQuarter / 16);
        for (const MidiNote *n : ci.notes) {
            pitch += n->note;
            length += n->endTick - n->tick;
            vel.push_back(n->velocity);
            if (n->tick - lastOnset > together) {
                ++onsets;
                lastOnset = n->tick;
            }
        }
        ci.avgPitch = pitch / ci.notes.size();
        ci.avgBeats = length / ci.notes.size() / m_midi.ticksPerQuarter;
        ci.polyphony = static_cast<double>(ci.notes.size()) / std::max(1, onsets);
        ci.medianVelocity = median(vel, 80);
    }
}

int Importer::channelOf(ImportRole role) const
{
    for (int c = 0; c < 16; ++c)
        if (m_res.roles[c] == role)
            return c;
    return -1;
}

void Importer::assignRoles()
{
    ImportRole *roles = m_res.roles;
    std::fill(roles, roles + 16, ImportRole::None);
    std::vector<bool> taken(16, false);
    for (int c = 0; c < 16; ++c)
        taken[c] = m_ch[c].notes.empty();

    auto fixed = [&](int option, ImportRole role) {
        if (option >= 0 && option < 16 && !m_ch[option].notes.empty()) {
            roles[option] = role;
            taken[option] = true;
        }
    };
    fixed(m_opt.drumChannel, ImportRole::Drums);
    fixed(m_opt.bassChannel, ImportRole::Bass);
    fixed(m_opt.compChannel, ImportRole::Comp);
    fixed(m_opt.guitarChannel, ImportRole::Guitar);
    fixed(m_opt.padChannel, ImportRole::Pad);

    // Picks the free channel with the most notes that passes `accept`.
    auto pick = [&](int option, ImportRole role, auto accept) {
        if (option != -1 || channelOf(role) >= 0)
            return;
        int best = -1;
        for (int c = 0; c < 16; ++c)
            if (!taken[c] && accept(m_ch[c], c) && (best < 0 || m_ch[c].notes.size() > m_ch[best].notes.size()))
                best = c;
        if (best >= 0) {
            roles[best] = role;
            taken[best] = true;
        }
    };
    auto program = [](const ChannelInfo &ci) { return std::max(ci.program, 0); };
    const bool noPrograms = std::all_of(std::begin(m_ch), std::end(m_ch), [](const ChannelInfo &ci) { return ci.program <= 0; });

    pick(m_opt.drumChannel, ImportRole::Drums, [](const ChannelInfo &, int c) { return c == 9; });
    pick(m_opt.bassChannel, ImportRole::Bass, [&](const ChannelInfo &ci, int c) {
        return c != 9 && inRange(program(ci), 32, 39) && ci.avgPitch < 60;
    });
    pick(m_opt.bassChannel, ImportRole::Bass, [&](const ChannelInfo &ci, int c) {
        return c != 9 && ci.avgPitch < 50 && ci.polyphony < 1.4 && (noPrograms || !inRange(program(ci), 32, 39));
    });
    pick(m_opt.guitarChannel, ImportRole::Guitar, [&](const ChannelInfo &ci, int c) {
        return c != 9 && inRange(program(ci), 24, 31) && ci.polyphony >= 1.5;
    });
    pick(m_opt.padChannel, ImportRole::Pad, [&](const ChannelInfo &ci, int c) {
        const int p = program(ci);
        return c != 9 && (inRange(p, 48, 55) || inRange(p, 88, 95)) && ci.polyphony >= 1.5;
    });
    pick(m_opt.compChannel, ImportRole::Comp, [&](const ChannelInfo &ci, int c) {
        return c != 9 && inRange(program(ci), 0, 23) && ci.polyphony >= 1.5;
    });
    pick(m_opt.compChannel, ImportRole::Comp, [&](const ChannelInfo &ci, int c) {
        return c != 9 && ci.polyphony >= 1.5 && ci.avgBeats < 2.0;
    });
    pick(m_opt.padChannel, ImportRole::Pad, [&](const ChannelInfo &ci, int c) {
        return c != 9 && ci.polyphony >= 1.5 && ci.avgBeats >= 2.0;
    });
    for (int c = 0; c < 16; ++c) {
        if (taken[c])
            continue;
        if (c == 9)
            continue; // a second drum channel is ignored
        roles[c] = m_ch[c].polyphony >= 1.5 ? ImportRole::None : ImportRole::Melody;
    }
}

void Importer::detectMeter()
{
    const int num = m_midi.meterNumerator;
    const int den = m_midi.meterDenominator;
    const double tpq = m_midi.ticksPerQuarter;
    if (den == 8 && num % 3 == 0 && num >= 6) { // compound meter: the beat is a dotted quarter
        m_beats = num / 3;
        m_beatTicks = tpq * 1.5;
    } else {
        m_beats = num;
        m_beatTicks = tpq * 4.0 / den;
    }
    m_beats = std::clamp(m_beats, 1, 16);
    m_res.tempo = static_cast<int>(std::lround(std::clamp(m_midi.quarterBpm * tpq / m_beatTicks, 30.0, 320.0)));
    std::ostringstream s;
    s << "Meter " << num << "/" << den << " (" << m_beats << " beats per bar), tempo " << m_res.tempo
      << " beats per minute";
    if (m_midi.meterChanges)
        s << "; the meter changes in the file, only the first one is used";
    if (m_midi.tempoChanges)
        s << "; the tempo changes in the file, the longest one is used";
    note(s.str());
}

// Chooses the step grid from the onsets of the rhythm parts.
void Importer::detectGrid()
{
    const bool compound = std::abs(m_beatTicks - m_midi.ticksPerQuarter * 1.5) < 1e-9
                          && m_midi.meterDenominator == 8;
    std::vector<double> fractions;
    for (int c = 0; c < 16; ++c) {
        const ImportRole r = m_res.roles[c];
        if (r != ImportRole::Drums && r != ImportRole::Bass && r != ImportRole::Comp && r != ImportRole::Guitar)
            continue;
        for (const MidiNote *n : m_ch[c].notes) {
            const double pos = n->tick / m_beatTicks;
            fractions.push_back(pos - std::floor(pos));
        }
    }
    auto hits = [&](int spb, double tolerance) {
        if (fractions.empty())
            return 1.0;
        int ok = 0;
        for (double f : fractions) {
            const double g = f * spb;
            ok += std::abs(g - std::round(g)) * (1.0 / spb) < tolerance;
        }
        return static_cast<double>(ok) / fractions.size();
    };

    int spb;
    if (m_opt.stepsPerBeat > 0) {
        spb = m_opt.stepsPerBeat;
    } else if (compound) {
        spb = hits(3, 0.05) >= 0.9 ? 3 : 6;
    } else {
        const double h2 = hits(2, 0.07), h3 = hits(3, 0.05), h4 = hits(4, 0.05);
        if (h2 >= 0.92)
            spb = 2;
        else if (h3 >= 0.9 && h3 >= h4)
            spb = 3;
        else if (h4 >= 0.9)
            spb = 4;
        else if (hits(2, 0.12) >= 0.9)
            spb = 2; // swung eighths
        else
            spb = h3 > h4 ? 3 : 4;
    }
    m_spb = spb;
    m_step = m_beatTicks / spb;
    m_stepsPerBar = m_beats * spb;

    // Swing: how late the off-beat steps come, for even grids.
    if (spb % 2 == 0 && !compound) {
        const double pair = 2.0 / spb; // two steps, in beats
        double sum = 0;
        int count = 0;
        for (double f : fractions) {
            const double inPair = std::fmod(f, pair) / pair;
            if (inPair > 0.4 && inPair < 0.75) {
                sum += inPair;
                ++count;
            }
        }
        if (count >= 8) {
            const double swing = std::clamp((sum / count - 0.5) / (1.0 / 6.0), 0.0, 1.0);
            if (swing >= 0.15) {
                m_swing = std::round(swing * 20) / 20;
                m_swingUnit = spb == 2 ? 8 : 16;
            }
        }
    }
    m_res.swingPercent = static_cast<int>(std::lround(m_swing * 100));

    std::ostringstream s;
    const char *grid = spb == 2 ? "eighths" : spb == 3 ? "eighth triplets" : spb == 4 ? "sixteenths" : spb == 6 ? "sixteenths" : "steps";
    s << "Grid: " << spb << " steps per beat (" << grid << ")";
    if (m_swing > 0)
        s << ", swing " << m_res.swingPercent << "% on " << m_swingUnit << "ths";
    note(s.str());
}

// How well a chord explains a weighted pitch-class histogram (weights sum
// to 1): chord tones count, other notes count against it, missing tones
// (except the fifth) and extra tones cost a little, the root in the bass helps.
double chordScore(const Chord &chord, const double weights[12], int bassPc)
{
    if (!chord.isValid())
        return -10.0;
    unsigned tones = 0;
    for (int iv : chord.intervals())
        tones |= 1u << ((chord.root + iv) % 12);
    double in = 0, missing = 0;
    for (int iv : chord.intervals()) {
        const int pc = (chord.root + iv) % 12;
        if (weights[pc] < 0.02)
            missing += (iv == 7 && chord.intervals().size() >= 3) ? 0.05 : 0.15;
    }
    for (int pc = 0; pc < 12; ++pc)
        if (tones & (1u << pc))
            in += weights[pc];
    const int size = static_cast<int>(chord.intervals().size());
    // Power and sus chords only where the third is really missing.
    const bool noThird = chord.quality == Quality::Power || chord.quality == Quality::Sus2 || chord.quality == Quality::Sus4;
    return in - 1.2 * (1.0 - in) - missing - 0.03 * std::max(0, size - 3) - (noThird ? 0.1 : 0.0)
           + (bassPc == chord.root ? 0.12 : 0.0);
}

// Chords for the progression: per bar, as a whole, in halves or per beat,
// whichever explains the notes clearly best. Notes count by how long they
// sound; the bass counts too.
void Importer::detectChords()
{
    const int totalBars = static_cast<int>(std::ceil(m_midi.endTick / (m_beatTicks * m_beats))) + 1;
    std::vector<int> harmony;
    for (int c = 0; c < 16; ++c) {
        const ImportRole r = m_res.roles[c];
        if (r == ImportRole::Comp || r == ImportRole::Guitar || r == ImportRole::Pad
            || (r == ImportRole::None && !m_ch[c].notes.empty() && c != 9))
            harmony.push_back(c);
    }
    const int bassCh = channelOf(ImportRole::Bass);
    if (harmony.empty())
        note("No chord instruments found; the chords are guessed from the bass");

    static const Quality qualities[] = {Quality::Major, Quality::Minor, Quality::Dom7, Quality::Maj7,
                                        Quality::Min7, Quality::Sus4, Quality::Sus2, Quality::Dim,
                                        Quality::HalfDim7, Quality::Dim7, Quality::Aug, Quality::Power};

    struct Segment {
        Chord chord;
        double score = 0;
        bool empty = true;
    };
    Chord previous;
    // The best chord for [t0, t1), or the previous one if it is nearly as good.
    auto analyse = [&](double t0, double t1, const Chord &before) {
        Segment seg;
        double w[12] = {};
        double total = 0;
        auto add = [&](int channel, double factor) {
            for (const MidiNote *n : m_ch[channel].notes) {
                if (n->tick >= t1)
                    break;
                const double overlap = std::min<double>(n->endTick, t1) - std::max<double>(n->tick, t0);
                if (overlap <= 0)
                    continue;
                w[n->note % 12] += overlap * factor;
                total += overlap * factor;
            }
        };
        for (int c : harmony)
            add(c, 1.0);
        int bassPc = -1;
        if (bassCh >= 0) {
            add(bassCh, harmony.empty() ? 1.0 : 0.7);
            int low = 128;
            for (const MidiNote *n : m_ch[bassCh].notes) {
                if (n->tick >= t0 + m_beatTicks / 2)
                    break;
                if (n->endTick > t0 && n->note < low)
                    low = n->note;
            }
            if (low < 128)
                bassPc = low % 12;
        }
        if (total <= 0)
            return seg;
        for (double &x : w)
            x /= total;
        seg.empty = false;
        for (Quality q : qualities)
            for (int root = 0; root < 12; ++root) {
                Chord c;
                c.root = root;
                c.quality = q;
                const double score = chordScore(c, w, bassPc);
                if (score > seg.score || !seg.chord.isValid()) {
                    seg.score = score;
                    seg.chord = c;
                }
            }
        if (before.isValid()) {
            const double keep = chordScore(before, w, bassPc);
            if (keep >= seg.score - 0.08) {
                seg.chord = before;
                seg.score = keep;
            }
        }
        return seg;
    };

    m_beatChords.assign(static_cast<size_t>(totalBars) * m_beats, Chord());
    std::vector<int> divisions = {1};
    if (m_beats % 2 == 0 && m_beats > 2)
        divisions.push_back(2);
    if (m_beats > 1)
        divisions.push_back(m_beats);
    for (int bar = 0; bar < totalBars; ++bar) {
        const double barStart = bar * m_beatTicks * m_beats;
        std::vector<Chord> best;
        double bestScore = -1e9;
        for (int k : divisions) {
            const double len = m_beatTicks * m_beats / k;
            std::vector<Chord> chords;
            double score = 0;
            Chord before = previous;
            int filled = 0;
            for (int i = 0; i < k; ++i) {
                const Segment seg = analyse(barStart + i * len, barStart + (i + 1) * len, before);
                if (!seg.empty) {
                    score += seg.score;
                    ++filled;
                    before = seg.chord;
                }
                chords.push_back(seg.empty ? before : seg.chord);
            }
            score = filled ? score / filled : -1e9;
            // Every chord change inside the bar costs, so more chords per bar
            // only win when they fit clearly better.
            for (size_t i = 1; i < chords.size(); ++i)
                score -= chords[i] != chords[i - 1] ? 0.15 : 0.0;
            if (best.empty() || score > bestScore) {
                best = chords;
                bestScore = score;
            }
        }
        const int perChord = m_beats / static_cast<int>(best.size());
        for (int b = 0; b < m_beats; ++b)
            m_beatChords[static_cast<size_t>(bar) * m_beats + b] = best[b / perChord];
        previous = best.back();
    }
    // Bars before the first chord take that chord.
    const auto first = std::find_if(m_beatChords.begin(), m_beatChords.end(), [](const Chord &c) { return c.isValid(); });
    if (first != m_beatChords.end())
        std::fill(m_beatChords.begin(), first, *first);
}

Chord Importer::chordAt(double tick) const
{
    if (m_beatChords.empty())
        return {};
    const int b = std::clamp(static_cast<int>(std::floor((tick + m_step / 2) / m_beatTicks)), 0,
                             static_cast<int>(m_beatChords.size()) - 1);
    return m_beatChords[b];
}

// The chords of a bar in the progression syntax: as few equal slots as
// possible, a repeated chord written as '/'.
std::string Importer::barChords(int bar) const
{
    std::vector<Chord> beats;
    for (int i = 0; i < m_beats; ++i) {
        const size_t b = static_cast<size_t>(bar) * m_beats + i;
        beats.push_back(b < m_beatChords.size() ? m_beatChords[b] : Chord());
    }
    int slots = m_beats;
    for (int k = 1; k <= m_beats; ++k) {
        if (m_beats % k)
            continue;
        const int len = m_beats / k;
        bool same = true;
        for (int i = 0; i < m_beats && same; ++i)
            same = beats[i] == beats[(i / len) * len];
        if (same) {
            slots = k;
            break;
        }
    }
    std::string text;
    const int len = m_beats / slots;
    for (int s = 0; s < slots; ++s) {
        const Chord &c = beats[s * len];
        if (!text.empty())
            text += ' ';
        if (s > 0 && c == beats[(s - 1) * len])
            text += '/';
        else
            text += c.isValid() ? c.name() : std::string("C");
    }
    return text;
}

// Notes of a channel that start in the block, grouped by step.
std::vector<std::vector<const MidiNote *>> Importer::groups(int channel, double blockStart) const
{
    const int steps = m_stepsPerBar * m_bars;
    std::vector<std::vector<const MidiNote *>> out(steps);
    if (channel < 0)
        return out;
    for (const MidiNote *n : m_ch[channel].notes) {
        const int step = stepOf(n->tick, blockStart);
        if (step >= 0 && step < steps)
            out[step].push_back(n);
    }
    return out;
}

std::map<int, std::string> Importer::drumPattern(double blockStart) const
{
    std::map<int, std::string> rows;
    const auto g = groups(channelOf(ImportRole::Drums), blockStart);
    const int steps = static_cast<int>(g.size());
    for (int s = 0; s < steps; ++s) {
        for (const MidiNote *n : g[s]) {
            auto &row = rows[n->note];
            if (row.empty())
                row.assign(steps, '.');
            const double v = n->velocity * m_drumScale;
            const char c = v >= 108 ? 'X' : v >= 80 ? 'x' : v >= 55 ? 'o' : 'g';
            const std::string order = ".goxX";
            if (order.find(c) > order.find(row[s]))
                row[s] = c;
        }
    }
    return rows;
}

std::string Importer::bassPattern(double blockStart) const
{
    const int steps = m_stepsPerBar * m_bars;
    std::string out(steps, '.');
    const int ch = channelOf(ImportRole::Bass);
    if (ch < 0)
        return out;
    const auto &all = m_ch[ch].notes;

    // The lowest note per step, and where the next one starts.
    struct Item { int step; const MidiNote *n; double nextTick; };
    std::vector<Item> items;
    for (size_t i = 0; i < all.size(); ++i) {
        const int step = stepOf(all[i]->tick, blockStart);
        if (step < 0 || step >= steps)
            continue;
        if (!items.empty() && items.back().step == step) {
            if (all[i]->note < items.back().n->note)
                items.back().n = all[i];
            continue;
        }
        double next = 1e18;
        for (size_t j = i + 1; j < all.size(); ++j)
            if (stepOf(all[j]->tick, blockStart) != step) {
                next = all[j]->tick;
                break;
            }
        items.push_back({step, all[i], next});
    }

    int lowestRoot = 128;
    for (const Item &it : items) {
        const Chord c = chordAt(it.n->tick);
        if (c.isValid() && (it.n->note - c.root) % 12 == 0)
            lowestRoot = std::min(lowestRoot, it.n->note);
    }

    for (const Item &it : items) {
        const Chord c = chordAt(it.n->tick);
        char token = 'R';
        if (c.isValid()) {
            const int iv = ((it.n->note - c.root) % 12 + 12) % 12;
            static const char table[12] = {'R', 'R', '3', '3', '3', '3', '5', '5', '5', '6', '7', '7'};
            token = table[iv];
            if (iv == 0 && it.n->note >= lowestRoot + 12)
                token = '8';
            const Chord next = it.nextTick < 1e17 ? chordAt(it.nextTick) : Chord();
            if (iv != 0 && next.isValid() && next != c) {
                const int d = ((it.n->note - next.root) % 12 + 12) % 12;
                if (d == 1 || d == 11)
                    token = 'A';
            }
        }
        out[it.step] = token;
        const double end = std::min<double>(it.n->endTick, it.nextTick);
        const int endStep = std::min(steps, std::max(it.step + 1, stepOf(end, blockStart)));
        for (int s = it.step + 1; s < endStep; ++s)
            out[s] = '-';
    }
    return out;
}

std::string Importer::compPattern(double blockStart) const
{
    const int steps = m_stepsPerBar * m_bars;
    std::string out(steps, '.');
    const int ch = channelOf(ImportRole::Comp);
    if (ch < 0)
        return out;
    const auto g = groups(ch, blockStart);
    const int median = m_ch[ch].medianVelocity;
    for (int s = 0; s < steps; ++s) {
        if (g[s].empty())
            continue;
        int next = s + 1;
        while (next < steps && g[next].empty())
            ++next;
        if (g[s].size() >= 2) {
            double vel = 0;
            long end = 0;
            for (const MidiNote *n : g[s]) {
                vel += n->velocity;
                end = std::max(end, n->endTick);
            }
            out[s] = vel / g[s].size() >= median ? 'C' : 'c';
            const int endStep = std::min(next, std::max(s + 1, stepOf(end, blockStart)));
            for (int h = s + 1; h < endStep; ++h)
                out[h] = '-';
        } else {
            const MidiNote *n = g[s].front();
            const Chord c = chordAt(n->tick);
            int digit = 1;
            if (c.isValid()) {
                const auto &ivs = c.intervals();
                const int iv = ((n->note - c.root) % 12 + 12) % 12;
                int best = 0;
                for (size_t i = 0; i < ivs.size(); ++i) {
                    const int d = std::min(std::abs(ivs[i] % 12 - iv), 12 - std::abs(ivs[i] % 12 - iv));
                    const int bd = std::min(std::abs(ivs[best] % 12 - iv), 12 - std::abs(ivs[best] % 12 - iv));
                    if (d < bd)
                        best = static_cast<int>(i);
                }
                digit = best + 1 + (n->note >= 67 ? static_cast<int>(std::min<size_t>(ivs.size(), 4)) : 0);
            }
            out[s] = static_cast<char>('0' + std::clamp(digit, 1, 9));
        }
    }
    return out;
}

std::string Importer::guitarPattern(double blockStart) const
{
    const int steps = m_stepsPerBar * m_bars;
    const int ch = channelOf(ImportRole::Guitar);
    if (ch < 0)
        return {};
    std::string out(steps, '.');
    const auto g = groups(ch, blockStart);
    const int median = m_ch[ch].medianVelocity;
    for (int s = 0; s < steps; ++s) {
        if (g[s].empty())
            continue;
        int next = s + 1;
        while (next < steps && g[next].empty())
            ++next;
        if (g[s].size() == 1) {
            const int digit = 1 + std::clamp((g[s].front()->note - 40) / 5, 0, 5);
            out[s] = static_cast<char>('0' + digit);
            continue;
        }
        double vel = 0, pitch = 0;
        long end = 0;
        std::vector<long> lengths;
        const MidiNote *first = g[s].front(), *last = g[s].front();
        for (const MidiNote *n : g[s]) {
            vel += n->velocity;
            pitch += n->note;
            end = std::max(end, n->endTick);
            lengths.push_back(n->endTick - n->tick);
            if (n->tick < first->tick)
                first = n;
            if (n->tick > last->tick)
                last = n;
        }
        vel /= g[s].size();
        pitch /= g[s].size();
        std::sort(lengths.begin(), lengths.end());
        const bool soft = vel < median - 8;
        char c;
        if (lengths[lengths.size() / 2] <= 0.6 * m_step && pitch < 57) {
            c = soft ? 'x' : 'X';
        } else {
            bool down;
            if (first->tick != last->tick)
                down = first->note < last->note;
            else
                down = ((s % m_spb) * 2) % m_spb == 0;
            c = down ? (soft ? 'd' : 'D') : (soft ? 'u' : 'U');
            const int endStep = std::min(next, std::max(s + 1, stepOf(end, blockStart)));
            for (int h = s + 1; h < endStep; ++h)
                out[h] = '-';
        }
        out[s] = c;
    }
    return out;
}

void Importer::cutBlocks()
{
    const double barTicks = m_beatTicks * m_beats;
    const int steps = m_stepsPerBar * m_bars;
    const int lastStepOfFirstBars = (m_bars - 1) * m_stepsPerBar;
    for (int bar = m_firstBar; bar + m_bars - 1 <= m_lastBar; bar += m_bars) {
        Block b;
        b.startBar = bar - m_firstBar;
        const double start = bar * barTicks;
        b.drums = drumPattern(start);
        b.bass = bassPattern(start);
        b.comp = compPattern(start);
        b.guitar = guitarPattern(start);
        for (const auto &row : b.drums)
            for (int s = 0; s < steps; ++s)
                if (row.second[s] != '.') {
                    b.cells.insert(cellKey(0, row.first, s));
                    if (s >= lastStepOfFirstBars)
                        b.lastBarDrums.insert(cellKey(0, row.first, s - lastStepOfFirstBars));
                }
        const std::string *parts[] = {&b.bass, &b.comp, &b.guitar};
        for (int p = 0; p < 3; ++p)
            for (int s = 0; s < static_cast<int>(parts[p]->size()); ++s)
                if ((*parts[p])[s] != '.' && (*parts[p])[s] != '-')
                    b.cells.insert(cellKey(p + 1, 0, s));
        m_blocks.push_back(std::move(b));
    }
}

// Groups similar blocks: the most common one is the main pattern, the next
// groups (with at least two blocks each) become variations. A block's last
// bar is left out of the comparison when the pattern has several bars, so
// that fills don't split the groups.
void Importer::cluster()
{
    if (m_blocks.empty())
        return;
    const int compareSteps = m_bars > 1 ? (m_bars - 1) * m_stepsPerBar : m_stepsPerBar;
    const size_t n = m_blocks.size();
    std::vector<std::set<long>> keys(n);
    for (size_t i = 0; i < n; ++i)
        keys[i] = cellsBefore(m_blocks[i].cells, compareSteps);
    std::vector<std::vector<double>> sim(n, std::vector<double>(n, 1.0));
    for (size_t i = 0; i < n; ++i)
        for (size_t j = i + 1; j < n; ++j)
            sim[i][j] = sim[j][i] = dice(keys[i], keys[j]);

    constexpr double Threshold = 0.8;
    std::vector<int> seeds;
    while (static_cast<int>(seeds.size()) < 1 + m_opt.maxVariations) {
        int seed = -1, seedCount = 0;
        for (size_t i = 0; i < n; ++i) {
            if (m_blocks[i].cluster >= 0)
                continue;
            int count = 0;
            for (size_t j = 0; j < n; ++j)
                count += m_blocks[j].cluster < 0 && sim[i][j] >= Threshold;
            if (count > seedCount) {
                seed = static_cast<int>(i);
                seedCount = count;
            }
        }
        if (seed < 0 || (!seeds.empty() && seedCount < 2))
            break;
        const int id = static_cast<int>(seeds.size());
        for (size_t j = 0; j < n; ++j)
            if (m_blocks[j].cluster < 0 && sim[seed][j] >= Threshold)
                m_blocks[j].cluster = id;
        seeds.push_back(seed);
    }
    for (size_t i = 0; i < n; ++i) { // the rest joins the closest group
        if (m_blocks[i].cluster >= 0)
            continue;
        int best = 0;
        for (size_t k = 1; k < seeds.size(); ++k)
            if (sim[i][seeds[k]] > sim[i][seeds[best]])
                best = static_cast<int>(k);
        m_blocks[i].cluster = best;
    }
    m_clusterCount = static_cast<int>(seeds.size());

    // Fills: a last bar that differs from the group's usual last bar.
    m_medoids.assign(m_clusterCount, -1);
    for (int k = 0; k < m_clusterCount; ++k) {
        std::vector<size_t> members;
        for (size_t i = 0; i < n; ++i)
            if (m_blocks[i].cluster == k)
                members.push_back(i);
        if (m_bars > 1 && members.size() > 1) {
            size_t ref = members.front();
            double refScore = -1;
            for (size_t a : members) {
                double score = 0;
                for (size_t b : members)
                    score += dice(m_blocks[a].lastBarDrums, m_blocks[b].lastBarDrums);
                if (score > refScore) {
                    refScore = score;
                    ref = a;
                }
            }
            for (size_t a : members)
                m_blocks[a].fill = !m_blocks[a].lastBarDrums.empty()
                                   && dice(m_blocks[a].lastBarDrums, m_blocks[ref].lastBarDrums) < 0.7;
        }
        double best = -1;
        for (size_t a : members) {
            if (m_blocks[a].fill && members.size() > 1)
                continue;
            double score = 0;
            for (size_t b : members)
                if (!m_blocks[b].fill)
                    score += dice(m_blocks[a].cells, m_blocks[b].cells);
            if (score > best) {
                best = score;
                m_medoids[k] = static_cast<int>(a);
            }
        }
        if (m_medoids[k] < 0)
            m_medoids[k] = static_cast<int>(members.front());
    }

    // The most common fill becomes the style's fill.
    std::vector<size_t> fills;
    for (size_t i = 0; i < n; ++i)
        if (m_blocks[i].fill)
            fills.push_back(i);
    if (!fills.empty()) {
        size_t bestFill = fills.front();
        double bestScore = -1;
        for (size_t a : fills) {
            double score = 0;
            for (size_t b : fills)
                score += dice(m_blocks[a].lastBarDrums, m_blocks[b].lastBarDrums) >= 0.8;
            if (score > bestScore) {
                bestScore = score;
                bestFill = a;
            }
        }
        const int offset = (m_bars - 1) * m_stepsPerBar;
        for (const auto &row : m_blocks[bestFill].drums) {
            const std::string bar = row.second.substr(offset, m_stepsPerBar);
            if (bar.find_first_not_of('.') != std::string::npos)
                m_fillRows[row.first] = bar;
        }
    }
}

std::vector<DrumRow> toRows(const std::map<int, std::string> &rows)
{
    std::vector<DrumRow> out;
    for (auto it = rows.rbegin(); it != rows.rend(); ++it) // cymbals first, kick last
        if (it->second.find_first_not_of('.') != std::string::npos)
            out.push_back({it->first, it->second});
    return out;
}

void Importer::buildStyle()
{
    Style &s = m_res.style;
    s = Style();
    s.name = m_opt.name;
    s.beatsPerBar = m_beats;
    s.stepsPerBeat = m_spb;
    s.bars = m_bars;
    s.tempo = m_res.tempo;
    s.swing = m_swing;
    s.swingUnit = m_swingUnit;
    s.crashNote = m_crash ? Crash : 0;
    s.padDefault = channelOf(ImportRole::Pad) >= 0;

    auto programOf = [&](ImportRole role, int fallback) {
        const int c = channelOf(role);
        return c >= 0 && m_ch[c].program >= 0 ? m_ch[c].program : fallback;
    };
    auto velocityOf = [&](ImportRole role, int fallback) {
        const int c = channelOf(role);
        return c >= 0 ? std::clamp(m_ch[c].medianVelocity, 30, 120) : fallback;
    };
    s.pianoProgram = programOf(ImportRole::Comp, 0);
    s.bassProgram = programOf(ImportRole::Bass, 33);
    s.guitarProgram = programOf(ImportRole::Guitar, 25);
    s.padProgram = programOf(ImportRole::Pad, 48);
    s.bassVelocity = velocityOf(ImportRole::Bass, 96);
    s.compVelocity = velocityOf(ImportRole::Comp, 78);
    s.guitarVelocity = velocityOf(ImportRole::Guitar, 80);
    s.guitarVoicing = m_voicing;

    const int steps = s.stepCount();
    if (m_blocks.empty()) {
        s.bass.assign(steps, '.');
        s.comp.assign(steps, '.');
        return;
    }

    // The crash on a block's first step is played by the style's phrase crash.
    auto withoutCrash = [](std::map<int, std::string> rows) {
        for (int c : {Crash, Crash2}) {
            auto it = rows.find(c);
            if (it != rows.end()) {
                it->second[0] = '.';
                if (it->second.find_first_not_of('.') == std::string::npos)
                    rows.erase(it);
            }
        }
        return rows;
    };

    const Block &main = m_blocks[m_medoids[0]];
    s.drums = toRows(withoutCrash(main.drums));
    s.fill = toRows(m_fillRows);
    s.bass = main.bass;
    s.comp = main.comp;
    s.guitar = main.guitar;
    for (int k = 1; k < m_clusterCount; ++k) {
        const Block &b = m_blocks[m_medoids[k]];
        StyleVariation v;
        v.name = std::string(1, static_cast<char>('A' + k));
        const auto drums = toRows(withoutCrash(b.drums));
        bool sameDrums = drums.size() == s.drums.size();
        for (size_t i = 0; sameDrums && i < drums.size(); ++i)
            sameDrums = drums[i].note == s.drums[i].note && drums[i].steps == s.drums[i].steps;
        if (!sameDrums)
            v.drums = drums.empty() ? std::vector<DrumRow>{{36, std::string(steps, '.')}} : drums;
        if (b.bass != s.bass)
            v.bass = b.bass;
        if (b.comp != s.comp)
            v.comp = b.comp;
        if (b.guitar != s.guitar)
            v.guitar = b.guitar;
        s.variations.push_back(std::move(v));
    }
}

void Importer::buildProgression()
{
    const bool markers = m_clusterCount > 1;
    std::ostringstream text;
    std::vector<std::string> line;
    auto flush = [&] {
        if (line.empty())
            return;
        text << "|";
        for (const auto &b : line)
            text << " " << b << " |";
        text << "\n";
        line.clear();
    };
    int currentCluster = -1;
    for (int bar = 0; bar < m_res.songBars; ++bar) {
        if (markers && bar % m_bars == 0) {
            const size_t block = static_cast<size_t>(bar / m_bars);
            if (block < m_blocks.size() && m_blocks[block].cluster != currentCluster) {
                flush();
                currentCluster = m_blocks[block].cluster;
                text << "[" << (currentCluster == 0 ? std::string("Main") : std::string(1, static_cast<char>('A' + currentCluster)))
                     << "]\n";
            }
        }
        line.push_back(barChords(m_firstBar + bar));
        if (line.size() == 4)
            flush();
    }
    flush();
    m_res.progression = text.str();
}

bool Importer::run(std::string &error)
{
    if (m_midi.notes.empty()) {
        error = "The MIDI file contains no notes";
        return false;
    }
    m_bars = std::clamp(m_opt.bars, 1, 8);
    collectChannels();
    assignRoles();
    detectMeter();
    detectGrid();
    detectChords();

    for (int c = 0; c < 16; ++c) {
        if (m_ch[c].notes.empty())
            continue;
        std::ostringstream s;
        s << "Channel " << (c + 1) << ": " << m_ch[c].notes.size() << " notes";
        if (m_ch[c].program >= 0 && c != 9)
            s << ", program " << m_ch[c].program;
        s << " -> " << importRoleName(m_res.roles[c]);
        note(s.str());
    }

    // The song runs from the first to the last bar with bass or chords.
    const double barTicks = m_beatTicks * m_beats;
    long first = -1, last = 0;
    for (int c = 0; c < 16; ++c) {
        const ImportRole r = m_res.roles[c];
        if (r == ImportRole::Drums || r == ImportRole::Melody)
            continue;
        for (const MidiNote *n : m_ch[c].notes) {
            if (first < 0 || n->tick < first)
                first = n->tick;
            last = std::max(last, n->endTick - 1);
        }
    }
    if (first < 0) { // drums only
        first = m_midi.notes.front().tick;
        last = m_midi.endTick - 1;
    }
    m_firstBar = static_cast<int>(std::floor((first + m_step / 2) / barTicks));
    m_lastBar = std::max(m_firstBar, static_cast<int>(std::floor(last / barTicks)));
    m_res.firstBar = m_firstBar;
    m_res.songBars = m_lastBar - m_firstBar + 1;
    if (m_res.songBars < m_bars) {
        m_bars = m_res.songBars;
        note("The song is shorter than the pattern length; patterns are " + std::to_string(m_bars) + " bars long");
    }

    // Drum velocities: the loudest hits (90th percentile) become accents.
    if (const int dc = channelOf(ImportRole::Drums); dc >= 0) {
        std::vector<int> v;
        for (const MidiNote *n : m_ch[dc].notes) {
            v.push_back(n->velocity);
            if ((n->note == Crash || n->note == Crash2)
                && std::abs(std::fmod(n->tick + m_step / 2, barTicks)) < m_step)
                m_crash = true;
        }
        std::sort(v.begin(), v.end());
        const int p90 = v[std::min(v.size() - 1, v.size() * 9 / 10)];
        m_drumScale = p90 > 0 ? 115.0 / p90 : 1.0;
    }

    // Guitar voicing from the size and register of its chords.
    if (const int gc = channelOf(ImportRole::Guitar); gc >= 0) {
        const double size = m_ch[gc].polyphony;
        if (size <= 3.3 && m_ch[gc].avgPitch < 60)
            m_voicing = GuitarVoicing::Power;
        else if (size <= 4 && m_ch[gc].avgPitch >= 62)
            m_voicing = GuitarVoicing::High;
    }

    cutBlocks();
    cluster();
    buildStyle();
    buildProgression();

    std::ostringstream s;
    s << "Song: " << m_res.songBars << " bars from bar " << (m_firstBar + 1) << "; patterns of " << m_bars
      << " bar(s), main pattern";
    for (int k = 1; k < m_clusterCount; ++k)
        s << ", variation " << static_cast<char>('A' + k);
    s << (m_fillRows.empty() ? ", no fill" : ", fill");
    note(s.str());

    std::string validation;
    if (!validateStyle(m_res.style, validation)) {
        error = "Internal error, the style is invalid: " + validation;
        return false;
    }
    return true;
}

} // namespace

bool importMidi(const MidiFileData &midi, const ImportOptions &options, ImportResult &result, std::string &error)
{
    result = ImportResult();
    ImportOptions opt = options;
    if (opt.name.empty()) {
        std::string title;
        for (char c : midi.title)
            if (static_cast<unsigned char>(c) >= 0x20)
                title += c;
        const auto first = title.find_first_not_of(' ');
        title = first == std::string::npos ? std::string() : title.substr(first, title.find_last_not_of(' ') - first + 1);
        const auto letters = std::count_if(title.begin(), title.end(),
                                           [](char c) { return std::isalpha(static_cast<unsigned char>(c)); });
        opt.name = letters >= 2 ? title : "Imported";
    }
    Importer importer(midi, opt, result);
    return importer.run(error);
}

} // namespace mc
