#include "BandEngine.h"

#include "MidiEngine.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdlib>

using namespace std::chrono;

namespace {

constexpr long long Forever = LLONG_MAX;

int holdSteps(const std::string &pattern, int step)
{
    int n = 1;
    while (step + n < static_cast<int>(pattern.size()) && pattern[step + n] == '-')
        ++n;
    return n;
}

int drumVelocity(char c)
{
    switch (c) {
    case 'X': return 118;
    case 'x': return 95;
    case 'o': return 70;
    case 'g': return 42;
    default: return 0;
    }
}

// Maps a tick inside a beat to a position (0..1) inside the beat, delaying
// the off-beats according to the swing amount.
double swingWarp(int tick, double swing, int unit)
{
    const double p = static_cast<double>(tick) / BandEngine::TicksPerBeat;
    const double segment = unit == 16 ? 0.5 : 1.0;
    const double base = std::floor(p / segment) * segment;
    const double q = (p - base) / segment;
    const double offbeat = 0.5 + swing / 6.0; // swing 1.0 = triplet feel
    const double w = q < 0.5 ? q * offbeat / 0.5 : offbeat + (q - 0.5) * (1.0 - offbeat) / 0.5;
    return base + w * segment;
}

int bassRoot(int pc)
{
    return 33 + (pc - 9 + 12) % 12; // A1 .. G#2
}

// Close-position voicing with the inversion that moves least from the
// previous voicing.
std::vector<int> voiceChord(const mc::Chord &chord, const std::vector<int> &previous, int lowest, int highest)
{
    std::vector<int> pcs;
    for (int iv : chord.intervals()) {
        const int pc = (chord.root + iv) % 12;
        if (std::find(pcs.begin(), pcs.end(), pc) == pcs.end())
            pcs.push_back(pc);
    }
    if (pcs.size() > 4) {
        auto fifth = std::find(pcs.begin(), pcs.end(), (chord.root + 7) % 12);
        if (fifth != pcs.end())
            pcs.erase(fifth);
    }
    while (pcs.size() > 4)
        pcs.pop_back();
    if (pcs.size() < 3)
        pcs.push_back(pcs.front());

    std::vector<int> best;
    double bestCost = 1e9;
    for (size_t inv = 0; inv < pcs.size(); ++inv) {
        std::vector<int> v;
        v.push_back(lowest + (pcs[inv] - lowest % 12 + 12) % 12);
        for (size_t k = 1; k < pcs.size(); ++k) {
            const int pc = pcs[(inv + k) % pcs.size()];
            int n = v.back() + 1;
            while (n % 12 != pc)
                ++n;
            v.push_back(n);
        }

        double cost = 0;
        if (previous.empty()) {
            double sum = 0;
            for (int n : v)
                sum += n;
            cost = std::abs(sum / v.size() - (lowest + highest) / 2.0);
        } else {
            for (int n : v) {
                int d = 127;
                for (int p : previous)
                    d = std::min(d, std::abs(n - p));
                cost += d;
            }
            for (int p : previous) {
                int d = 127;
                for (int n : v)
                    d = std::min(d, std::abs(n - p));
                cost += d;
            }
        }
        if (v.back() > highest)
            cost += 50;
        if (cost < bestCost) {
            bestCost = cost;
            best = v;
        }
    }
    return best;
}

} // namespace

int BandEngine::channel(Part part)
{
    switch (part) {
    case Drums: return 9;
    case Bass: return 1;
    case Piano: return 0;
    case Pad: return 2;
    default: return 15;
    }
}

BandEngine::BandEngine(MidiEngine *midi, QObject *parent)
    : QObject(parent)
    , m_midi(midi)
    , m_style(std::make_shared<mc::Style>(mc::builtinStyles().front()))
    , m_progression(std::make_shared<mc::Progression>())
{
    m_enabled[Drums] = true;
    m_enabled[Bass] = true;
    m_enabled[Piano] = true;
    m_enabled[Pad] = false;
}

BandEngine::~BandEngine()
{
    stop();
}

void BandEngine::start(bool countIn)
{
    if (m_running)
        return;
    if (m_thread.joinable())
        m_thread.join();
    m_running = true;
    m_thread = std::thread(&BandEngine::run, this, countIn);
}

void BandEngine::stop()
{
    m_running = false;
    if (m_thread.joinable())
        m_thread.join();
}

void BandEngine::setStyle(const mc::Style &style)
{
    auto s = std::make_shared<const mc::Style>(style);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_style = s;
}

void BandEngine::setProgression(const mc::Progression &progression)
{
    auto p = std::make_shared<const mc::Progression>(progression);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_progression = p;
}

void BandEngine::setLiveChord(const mc::Chord &chord)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_liveChord = chord;
}

void BandEngine::setVariationPlan(mc::VariationMode mode, int fixed, int everyBars)
{
    m_variationMode = mode;
    m_variationFixed = fixed;
    m_variationEvery = everyBars;
}

void BandEngine::run(bool countIn)
{
    m_tActive.clear();
    m_tAbsTick = 0;
    m_tChord = {};
    m_tCompVoicing.clear();
    m_tPadVoicing.clear();
    m_tLastBass = -1;
    m_tVariation = 0;
    m_tVariationName.clear();
    m_tCountedIn = countIn;

    auto beatStart = steady_clock::now() + milliseconds(50);
    int bar = countIn ? -1 : 0;

    while (m_running) {
        const auto previousStyle = m_tStyle;
        {
            // Style and progression changes take effect at bar boundaries.
            std::lock_guard<std::mutex> lock(m_mutex);
            m_tStyle = m_style;
            m_tProgression = m_progression;
        }
        if (m_tStyle != previousStyle && m_tChord.isValid())
            voiceComp(); // the new style may voice chords differently
        if (bar >= 0)
            updateVariation(bar);
        const int beats = m_tStyle->beatsPerBar;
        for (int beat = 0; beat < beats && m_running; ++beat) {
            const double beatSeconds = 60.0 / std::clamp(m_tempo.load(), 20, 400);
            const double swing = m_swing.load();
            for (int t = 0; t < TicksPerBeat && m_running; ++t) {
                const double offset = beatSeconds * swingWarp(t, swing, m_tStyle->swingUnit);
                std::this_thread::sleep_until(beatStart + duration_cast<steady_clock::duration>(duration<double>(offset)));
                if (!m_running)
                    break;
                if (t == 0)
                    emit positionChanged(bar, beat, beats);
                if (bar < 0)
                    countInTick(beat, t);
                else
                    processTick(bar, beat, t);
                ++m_tAbsTick;
            }
            beatStart += duration_cast<steady_clock::duration>(duration<double>(beatSeconds));
        }
        ++bar;
    }

    for (const auto &n : m_tActive)
        m_midi->noteOff(channel(n.part), n.note);
    m_tActive.clear();
    m_midi->allNotesOff();
    emit chordChanged(QString());
    emit variationChanged(QString());
}

// Called at every bar start. A style change may leave fewer variations.
void BandEngine::updateVariation(int bar)
{
    const mc::Style &s = *m_tStyle;
    const int count = s.variationCount();
    const mc::VariationMode mode = m_variationMode;
    if (mode == mc::VariationMode::Fixed) {
        m_tVariation = std::clamp(m_variationFixed.load(), 0, count - 1);
    } else {
        if (m_tVariation >= count)
            m_tVariation = 0;
        if (bar > 0 && bar % mc::variationInterval(m_variationEvery, s.bars) == 0)
            m_tVariation = mc::nextVariation(mode, m_tVariation, count, m_rng());
    }
    const std::string name = m_tVariation == 0 ? "Main" : s.variations[m_tVariation - 1].name;
    if (name != m_tVariationName) {
        m_tVariationName = name;
        emit variationChanged(QString::fromStdString(name));
    }
}

void BandEngine::countInTick(int beat, int tick)
{
    releaseDue();
    if (tick == 0)
        playNote(Drums, 37, beat == 0 ? 120 : 90, m_tAbsTick + TicksPerBeat / 2);
}

mc::Chord BandEngine::chordAt(int bar, int barTick)
{
    if (m_mode == ChordMode::Live) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_liveChord;
    }
    if (!m_tProgression || m_tProgression->empty())
        return {};
    return m_tProgression->chordAt(bar, barTick, m_tStyle->beatsPerBar * TicksPerBeat);
}

int BandEngine::phraseLength() const
{
    if (m_mode == ChordMode::Progression && m_tProgression) {
        const int n = m_tProgression->size();
        if (n >= 4)
            return n % 8 == 0 ? 8 : n;
    }
    return 8;
}

void BandEngine::processTick(int bar, int beat, int tick)
{
    const mc::Style &s = *m_tStyle;
    releaseDue();

    const int barTick = beat * TicksPerBeat + tick;
    const mc::Chord chord = chordAt(bar, barTick);
    const bool changed = chord != m_tChord;
    if (changed) {
        m_tChord = chord;
        if (chord.isValid()) {
            voiceComp();
            m_tPadVoicing = voiceChord(chord, m_tPadVoicing, 55, 81);
        }
        emit chordChanged(QString::fromStdString(chord.name()));
    }

    bool hit[PartCount] = {};
    const int stepTicks = TicksPerBeat / s.stepsPerBeat;
    if (barTick % stepTicks == 0) {
        const int stepInBar = barTick / stepTicks;
        const int patternStep = (bar % s.bars) * s.stepsPerBar() + stepInBar;
        if (m_enabled[Drums])
            playDrums(bar, stepInBar, patternStep, stepTicks);
        if (chord.isValid()) {
            if (m_enabled[Bass])
                hit[Bass] = playBassStep(patternStep, bar, barTick, stepTicks, stepInBar == 0);
            if (m_enabled[Piano])
                hit[Piano] = playCompStep(patternStep, stepTicks);
        }
    }

    if (!chord.isValid()) {
        stopPart(Bass);
        stopPart(Piano);
        stopPart(Pad);
        return;
    }

    // A chord change between pattern hits re-voices the notes still ringing.
    if (changed) {
        if (!hit[Bass])
            retrigger(Bass);
        if (!hit[Piano])
            retrigger(Piano);
    }
    if (m_enabled[Pad]) {
        if (changed || !isSounding(Pad))
            startPad();
    } else if (isSounding(Pad)) {
        stopPart(Pad);
    }
}

void BandEngine::playDrums(int bar, int stepInBar, int patternStep, int stepTicks)
{
    const mc::Style &s = *m_tStyle;
    const int phrase = phraseLength();
    const auto &fill = s.fillOf(m_tVariation);
    const bool fillBar = !fill.empty() && (bar + 1) % phrase == 0;
    const auto &rows = fillBar ? fill : s.drumsOf(m_tVariation);
    const int step = fillBar ? stepInBar : patternStep;

    for (const auto &row : rows) {
        if (step >= static_cast<int>(row.steps.size()))
            continue;
        if (const int v = drumVelocity(row.steps[step]))
            playNote(Drums, row.note, humanize(v), m_tAbsTick + stepTicks);
    }
    if (stepInBar == 0 && s.crashNote && bar % phrase == 0 && (bar > 0 || m_tCountedIn))
        playNote(Drums, s.crashNote, 105, m_tAbsTick + stepTicks);
}

bool BandEngine::playBassStep(int patternStep, int bar, int barTick, int stepTicks, bool downbeat)
{
    const mc::Style &s = *m_tStyle;
    const std::string &bass = s.bassOf(m_tVariation);
    const char c = bass[patternStep];
    if (c == '.' || c == '-')
        return false;
    const int len = holdSteps(bass, patternStep);
    const int note = bassNote(c, bar, barTick + len * stepTicks);
    if (note < 0)
        return false;
    stopPart(Bass);
    playNote(Bass, note, humanize(s.bassVelocity + (downbeat ? 8 : 0)), m_tAbsTick + std::max(1, len * stepTicks - 1));
    m_tLastBass = note;
    return true;
}

int BandEngine::bassNote(char token, int bar, int nextTick)
{
    const mc::Chord &ch = m_tChord;
    const int base = bassRoot(ch.root);
    switch (token) {
    case 'R': return bassRoot(ch.bass >= 0 ? ch.bass : ch.root);
    case '3': return base + ch.third();
    case '5': return base + ch.fifth();
    case '6': return base + 9;
    case '7': return base + ch.seventh();
    case '8': return base + 12;
    case 'A': {
        mc::Chord next = chordAt(bar, nextTick);
        if (!next.isValid())
            next = ch;
        const int target = bassRoot(next.bass >= 0 ? next.bass : next.root);
        const int ref = m_tLastBass > 0 ? m_tLastBass : base;
        return std::abs(target - 1 - ref) <= std::abs(target + 1 - ref) ? target - 1 : target + 1;
    }
    default: return -1;
    }
}

void BandEngine::voiceComp()
{
    if (m_tStyle->powerChords) {
        const int root = 40 + (m_tChord.root - 4 + 12) % 12; // E2 .. D#3
        m_tCompVoicing = {root, root + m_tChord.fifth(), root + 12};
    } else {
        m_tCompVoicing = voiceChord(m_tChord, m_tCompVoicing, 52, 76);
    }
}

bool BandEngine::playCompStep(int patternStep, int stepTicks)
{
    const mc::Style &s = *m_tStyle;
    const std::string &comp = s.compOf(m_tVariation);
    const char c = comp[patternStep];
    const int len = holdSteps(comp, patternStep);
    if (c == 'C' || c == 'c') {
        stopPart(Piano);
        const int vel = c == 'C' ? s.compVelocity + 8 : s.compVelocity - 15;
        const long long off = m_tAbsTick + std::max(1, len * stepTicks - 1);
        for (int n : m_tCompVoicing)
            playNote(Piano, n, humanize(vel), off);
        return true;
    }
    if (c >= '1' && c <= '9' && !m_tCompVoicing.empty()) {
        const int idx = c - '1';
        const int size = static_cast<int>(m_tCompVoicing.size());
        const int note = m_tCompVoicing[idx % size] + 12 * (idx / size);
        // Arpeggio notes ring on a little so they overlap.
        playNote(Piano, note, humanize(s.compVelocity), m_tAbsTick + (len + 2) * stepTicks);
        return true;
    }
    return false;
}

void BandEngine::retrigger(Part part)
{
    long long off = -1;
    for (const auto &n : m_tActive)
        if (n.part == part)
            off = std::max(off, n.offTick);
    if (off < 0)
        return;
    stopPart(part);
    if (off - m_tAbsTick < 2)
        return;

    const mc::Style &s = *m_tStyle;
    if (part == Bass) {
        const int note = bassRoot(m_tChord.bass >= 0 ? m_tChord.bass : m_tChord.root);
        playNote(Bass, note, humanize(s.bassVelocity - 10), off);
        m_tLastBass = note;
    } else if (part == Piano && s.compOf(m_tVariation).find_first_of("123456789") == std::string::npos) {
        for (int n : m_tCompVoicing)
            playNote(Piano, n, humanize(s.compVelocity - 15), off);
    }
}

void BandEngine::startPad()
{
    stopPart(Pad);
    for (int n : m_tPadVoicing)
        playNote(Pad, n, 68, Forever);
}

void BandEngine::playNote(Part part, int note, int velocity, long long offTick)
{
    if (note < 0 || note > 127)
        return;
    const int ch = channel(part);
    for (auto it = m_tActive.begin(); it != m_tActive.end(); ++it) {
        if (it->part == part && it->note == note) {
            m_midi->noteOff(ch, note);
            m_tActive.erase(it);
            break;
        }
    }
    m_midi->noteOn(ch, note, std::clamp(velocity, 1, 127));
    m_tActive.push_back({part, note, offTick});
}

void BandEngine::stopPart(Part part)
{
    auto it = m_tActive.begin();
    while (it != m_tActive.end()) {
        if (it->part == part) {
            m_midi->noteOff(channel(part), it->note);
            it = m_tActive.erase(it);
        } else {
            ++it;
        }
    }
}

bool BandEngine::isSounding(Part part) const
{
    return std::any_of(m_tActive.begin(), m_tActive.end(), [part](const ActiveNote &n) { return n.part == part; });
}

void BandEngine::releaseDue()
{
    auto it = m_tActive.begin();
    while (it != m_tActive.end()) {
        if (it->offTick <= m_tAbsTick) {
            m_midi->noteOff(channel(it->part), it->note);
            it = m_tActive.erase(it);
        } else {
            ++it;
        }
    }
}

int BandEngine::humanize(int velocity)
{
    return velocity + std::uniform_int_distribution<int>(-5, 5)(m_rng);
}
