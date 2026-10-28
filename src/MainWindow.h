#pragma once

#include "BandEngine.h"
#include "ChordDetector.h"
#include "PresetFile.h"

#include <QList>
#include <QMainWindow>

#include <set>

class QAction;
class QCheckBox;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QSlider;
class QSpinBox;
class QTabWidget;
class QTimer;
class BeatIndicator;
class MidiEngine;
class NoteSpinBox;
class ProgressionView;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(MidiEngine *midi, QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    struct PartControls {
        QCheckBox *enabled = nullptr;
        QComboBox *program = nullptr;
        QSlider *volume = nullptr;
    };

    void buildUi();
    QWidget *buildProgressionTab();
    QWidget *buildLiveTab();
    QWidget *buildBandBox();
    QWidget *buildConnectionBox();

    void togglePlay();
    void startBand(bool countIn);
    void stopBand();
    void updatePlayActions();

    void onStyleChanged(int index);
    void onPresetChosen(int index);
    void applyPreset(const PresetData &preset);
    PresetData currentPreset(const QString &name) const;
    int addPreset(const PresetData &preset);
    void openPresetFile();
    void savePresetFile();
    bool applyProgression();
    ChordMode currentMode() const;

    void refreshPorts();
    void onOutputChosen(int index);
    void onInputChosen(int index);

    void onNoteOn(int channel, int note, int velocity);
    void onNoteOff(int channel, int note);
    void onControl(int channel, int controller, int value);
    bool inChordZone(int note) const;
    void updateDetection();
    void updateHeldLabel();

    void onPosition(int bar, int beat, int beatsPerBar);
    void onBandChord(const QString &name);

    void sendPart(BandEngine::Part part);
    void sendMixer();

    void loadSettings();
    void saveSettings();

    MidiEngine *m_midi;
    BandEngine *m_engine;
    mc::ChordDetector m_detector;
    mc::Progression m_progression;
    QList<PresetData> m_presets; // built-in presets first, then presets loaded from files

    // Transport
    QAction *m_playAction = nullptr;
    QAction *m_stopAction = nullptr;
    QComboBox *m_styleCombo = nullptr;
    QSpinBox *m_tempo = nullptr;
    QSlider *m_swing = nullptr;
    QLabel *m_swingLabel = nullptr;
    QCheckBox *m_countIn = nullptr;

    // Display
    QLabel *m_chordLabel = nullptr;
    QLabel *m_positionLabel = nullptr;
    BeatIndicator *m_beatIndicator = nullptr;
    QTabWidget *m_tabs = nullptr;

    // Progression mode
    QComboBox *m_presetCombo = nullptr;
    QSpinBox *m_transpose = nullptr;
    QPlainTextEdit *m_progressionEdit = nullptr;
    QLabel *m_progressionError = nullptr;
    ProgressionView *m_progressionView = nullptr;
    QTimer *m_applyTimer = nullptr;

    // Live mode
    QCheckBox *m_useSplit = nullptr;
    NoteSpinBox *m_split = nullptr;
    QSpinBox *m_minNotes = nullptr;
    QCheckBox *m_bassInversion = nullptr;
    QCheckBox *m_autoStart = nullptr;
    QComboBox *m_thruMode = nullptr;
    QComboBox *m_thruProgram = nullptr;
    QLabel *m_heldLabel = nullptr;
    QLabel *m_detectedLabel = nullptr;
    std::set<int> m_held;
    std::set<int> m_thruNotes;

    // Band and connections
    PartControls m_parts[BandEngine::PartCount];
    QComboBox *m_outputCombo = nullptr;
    QComboBox *m_inputCombo = nullptr;
    QString m_outputId;
    QString m_inputId;
};
