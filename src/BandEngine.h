#pragma once

#include "Chord.h"
#include "Progression.h"
#include "Style.h"

#include <QObject>
#include <QString>

#include <atomic>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

class MidiEngine;

enum class ChordMode { Progression, Live };

// The band sequencer. It runs on its own thread with a resolution of
// 12 ticks per beat and reads the current chord either from the chord
// progression or from the live chord set by the chord detector.
class BandEngine : public QObject {
    Q_OBJECT
public:
    enum Part { Drums, Bass, Piano, Pad, PartCount };
    static constexpr int TicksPerBeat = 12;
    static constexpr int ThruChannel = 3;
    static int channel(Part part);

    explicit BandEngine(MidiEngine *midi, QObject *parent = nullptr);
    ~BandEngine() override;

    void start(bool countIn);
    void stop();
    bool isRunning() const { return m_running; }

    void setStyle(const mc::Style &style);
    void setTempo(int bpm) { m_tempo = bpm; }
    void setSwing(double swing) { m_swing = swing; }
    void setProgression(const mc::Progression &progression);
    void setMode(ChordMode mode) { m_mode = mode; }
    void setLiveChord(const mc::Chord &chord);
    void setPartEnabled(Part part, bool enabled) { m_enabled[part] = enabled; }
    // Takes effect at the next bar. `fixed` is used in VariationMode::Fixed.
    void setVariationPlan(mc::VariationMode mode, int fixed, int everyBars);

signals:
    // Emitted from the band thread; connect with queued connections.
    void positionChanged(int bar, int beat, int beatsPerBar); // bar -1 = count-in
    void chordChanged(const QString &name);
    void variationChanged(const QString &name); // empty when the band stops

private:
    struct ActiveNote {
        Part part;
        int note;
        long long offTick;
    };

    void run(bool countIn);
    void countInTick(int beat, int tick);
    void processTick(int bar, int beat, int tick);
    mc::Chord chordAt(int bar, int barTick);
    int phraseLength() const;
    void updateVariation(int bar);

    void playDrums(int bar, int stepInBar, int patternStep, int stepTicks);
    bool playBassStep(int patternStep, int bar, int barTick, int stepTicks, bool downbeat);
    bool playCompStep(int patternStep, int stepTicks);
    int bassNote(char token, int bar, int nextTick);
    void voiceComp();
    void retrigger(Part part);
    void startPad();

    void playNote(Part part, int note, int velocity, long long offTick);
    void stopPart(Part part);
    bool isSounding(Part part) const;
    void releaseDue();
    int humanize(int velocity);

    MidiEngine *m_midi;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<int> m_tempo{120};
    std::atomic<double> m_swing{0.0};
    std::atomic<ChordMode> m_mode{ChordMode::Progression};
    std::atomic<bool> m_enabled[PartCount];
    std::atomic<mc::VariationMode> m_variationMode{mc::VariationMode::InTurn};
    std::atomic<int> m_variationFixed{0};
    std::atomic<int> m_variationEvery{8};

    std::mutex m_mutex; // guards the three members below
    std::shared_ptr<const mc::Style> m_style;
    std::shared_ptr<const mc::Progression> m_progression;
    mc::Chord m_liveChord;

    // Band thread state
    std::shared_ptr<const mc::Style> m_tStyle;
    std::shared_ptr<const mc::Progression> m_tProgression;
    std::vector<ActiveNote> m_tActive;
    long long m_tAbsTick = 0;
    mc::Chord m_tChord;
    std::vector<int> m_tCompVoicing;
    std::vector<int> m_tPadVoicing;
    int m_tLastBass = -1;
    int m_tVariation = 0;
    std::string m_tVariationName;
    bool m_tCountedIn = false;
    std::mt19937 m_rng{std::random_device{}()};
};
