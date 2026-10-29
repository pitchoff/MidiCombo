#include "MainWindow.h"

#include "MidiEngine.h"
#include "StyleFile.h"
#include "Widgets.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

namespace {

const char *const kPartNames[BandEngine::PartCount] = {"Drums", "Bass", "Piano / Comp", "Pad"};

struct DrumKit {
    const char *name;
    int program;
};
const DrumKit kDrumKits[] = {
    {"Standard Kit", 0}, {"Room Kit", 8}, {"Power Kit", 16}, {"Electronic Kit", 24},
    {"TR-808 Kit", 25}, {"Jazz Kit", 32}, {"Brush Kit", 40}, {"Orchestra Kit", 48},
};

bool parsePortId(const QString &id, int &client, int &port)
{
    const QStringList parts = id.split(QLatin1Char(':'));
    if (parts.size() != 2)
        return false;
    bool ok1 = false, ok2 = false;
    client = parts[0].toInt(&ok1);
    port = parts[1].toInt(&ok2);
    return ok1 && ok2;
}

} // namespace

MainWindow::MainWindow(MidiEngine *midi, QWidget *parent)
    : QMainWindow(parent)
    , m_midi(midi)
    , m_engine(new BandEngine(midi, this))
    , m_styles(mc::builtinStyles())
{
    buildUi();

    connect(m_engine, &BandEngine::positionChanged, this, &MainWindow::onPosition, Qt::QueuedConnection);
    connect(m_engine, &BandEngine::chordChanged, this, &MainWindow::onBandChord, Qt::QueuedConnection);
    connect(m_midi, &MidiEngine::noteOnReceived, this, &MainWindow::onNoteOn);
    connect(m_midi, &MidiEngine::noteOffReceived, this, &MainWindow::onNoteOff);
    connect(m_midi, &MidiEngine::controlReceived, this, &MainWindow::onControl);

    refreshPorts();
    loadSettings();
    applyProgression();
    m_engine->setMode(currentMode());
    sendMixer();
    updatePlayActions();
}

MainWindow::~MainWindow()
{
    m_engine->stop();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    stopBand();
    saveSettings();
    event->accept();
}

// ---------------------------------------------------------------- UI setup

void MainWindow::buildUi()
{
    setWindowTitle(tr("MidiCombo – Background Band"));

    // Toolbar / transport
    QToolBar *tb = addToolBar(tr("Transport"));
    tb->setMovable(false);
    m_playAction = tb->addAction(style()->standardIcon(QStyle::SP_MediaPlay), tr("Play"));
    m_playAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Space));
    m_playAction->setToolTip(tr("Start the band (Ctrl+Space toggles)"));
    connect(m_playAction, &QAction::triggered, this, &MainWindow::togglePlay);
    m_stopAction = tb->addAction(style()->standardIcon(QStyle::SP_MediaStop), tr("Stop"));
    connect(m_stopAction, &QAction::triggered, this, &MainWindow::stopBand);
    tb->addSeparator();

    tb->addWidget(new QLabel(tr(" Style ")));
    m_styleCombo = new QComboBox;
    for (const auto &s : m_styles)
        m_styleCombo->addItem(QString::fromStdString(s.name));
    tb->addWidget(m_styleCombo);
    connect(m_styleCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onStyleChanged);

    tb->addWidget(new QLabel(tr("  Tempo ")));
    m_tempo = new QSpinBox;
    m_tempo->setRange(30, 320);
    m_tempo->setSuffix(tr(" BPM"));
    m_tempo->setValue(120);
    tb->addWidget(m_tempo);
    connect(m_tempo, &QSpinBox::valueChanged, this, [this](int v) { m_engine->setTempo(v); });

    tb->addWidget(new QLabel(tr("  Swing ")));
    m_swing = new QSlider(Qt::Horizontal);
    m_swing->setRange(0, 100);
    m_swing->setFixedWidth(110);
    tb->addWidget(m_swing);
    m_swingLabel = new QLabel(QStringLiteral("0%"));
    m_swingLabel->setMinimumWidth(40);
    tb->addWidget(m_swingLabel);
    connect(m_swing, &QSlider::valueChanged, this, [this](int v) {
        m_swingLabel->setText(QStringLiteral("%1%").arg(v));
        m_engine->setSwing(v / 100.0);
    });

    m_countIn = new QCheckBox(tr("Count-in"));
    m_countIn->setChecked(true);
    tb->addWidget(m_countIn);

    // Menus
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&Open Presets…"), QKeySequence::Open, this, &MainWindow::openPresetFile);
    fileMenu->addAction(tr("&Save Preset As…"), QKeySequence::Save, this, &MainWindow::savePresetFile);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("Load St&yles…"), QKeySequence(Qt::CTRL | Qt::Key_L), this, &MainWindow::openStyleFile);
    fileMenu->addAction(tr("&Forget Loaded Styles"), this, &MainWindow::forgetLoadedStyles);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), QKeySequence::Quit, this, &QWidget::close);
    QMenu *bandMenu = menuBar()->addMenu(tr("&Band"));
    bandMenu->addAction(m_playAction);
    bandMenu->addAction(m_stopAction);
    bandMenu->addAction(tr("MIDI &Panic (all notes off)"), this, [this] { m_midi->allNotesOff(); });
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&Sound Setup…"), this, [this] {
        QMessageBox::information(this, tr("Sound Setup"),
            tr("<p>MidiCombo sends MIDI to an ALSA sequencer port. To hear it you need a "
               "General MIDI synthesizer, for example FluidSynth:</p>"
               "<pre>sudo apt install fluidsynth fluid-soundfont-gm\n"
               "fluidsynth -a pipewire -m alsa_seq -is \\\n"
               "  /usr/share/sounds/sf2/FluidR3_GM.sf2</pre>"
               "<p>Then press <b>Refresh</b> and choose the synth as <b>Output</b>. "
               "A hardware synth or keyboard with GM sounds works as well.</p>"
               "<p>Channels: 1 piano/comp, 2 bass, 3 pad, 4 MIDI thru, 10 drums.</p>"));
    });
    helpMenu->addAction(tr("&About"), this, [this] {
        QMessageBox::about(this, tr("About MidiCombo"),
            tr("<b>MidiCombo</b><br>A background band for practicing musicians.<br>"
               "Qt %1 · ALSA sequencer").arg(QString::fromLatin1(qVersion())));
    });

    // Central area
    auto *central = new QWidget;
    auto *layout = new QVBoxLayout(central);

    auto *display = new QHBoxLayout;
    m_chordLabel = new QLabel(QStringLiteral("—"));
    QFont big = m_chordLabel->font();
    big.setPointSizeF(big.pointSizeF() * 3.2);
    big.setBold(true);
    m_chordLabel->setFont(big);
    m_chordLabel->setMinimumWidth(260);
    display->addWidget(m_chordLabel);
    display->addStretch();
    auto *posBox = new QVBoxLayout;
    m_beatIndicator = new BeatIndicator;
    m_positionLabel = new QLabel(tr("Stopped"));
    posBox->addWidget(m_beatIndicator);
    posBox->addWidget(m_positionLabel);
    display->addLayout(posBox);
    layout->addLayout(display);

    m_tabs = new QTabWidget;
    m_tabs->addTab(buildProgressionTab(), tr("Chord Progression"));
    m_tabs->addTab(buildLiveTab(), tr("Live MIDI Chords"));
    connect(m_tabs, &QTabWidget::currentChanged, this, [this] {
        m_engine->setMode(currentMode());
        m_progressionView->setCurrentBar(-1);
    });
    layout->addWidget(m_tabs, 1);

    auto *bottom = new QHBoxLayout;
    bottom->addWidget(buildBandBox(), 3);
    bottom->addWidget(buildConnectionBox(), 2);
    layout->addLayout(bottom);

    setCentralWidget(central);
    statusBar();
    resize(980, 720);
}

QWidget *MainWindow::buildProgressionTab()
{
    auto *w = new QWidget;
    auto *layout = new QVBoxLayout(w);

    auto *row = new QHBoxLayout;
    row->addWidget(new QLabel(tr("Preset:")));
    m_presetCombo = new QComboBox;
    m_presetCombo->addItem(tr("— choose a preset —"));
    for (const auto &p : mc::progressionPresets()) {
        PresetData preset;
        preset.name = QString::fromUtf8(p.name);
        preset.style = QString::fromUtf8(p.style);
        preset.tempo = p.tempo;
        preset.progression = QString::fromUtf8(p.text).trimmed();
        addPreset(preset);
    }
    connect(m_presetCombo, &QComboBox::activated, this, &MainWindow::onPresetChosen);
    row->addWidget(m_presetCombo, 1);
    auto *openButton = new QPushButton(tr("Open…"));
    openButton->setToolTip(tr("Load presets from an XML file"));
    connect(openButton, &QPushButton::clicked, this, &MainWindow::openPresetFile);
    row->addWidget(openButton);
    auto *saveButton = new QPushButton(tr("Save…"));
    saveButton->setToolTip(tr("Save style, tempo, progression and band settings to an XML file"));
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::savePresetFile);
    row->addWidget(saveButton);
    row->addSpacing(20);
    row->addWidget(new QLabel(tr("Transpose:")));
    m_transpose = new QSpinBox;
    m_transpose->setRange(-11, 11);
    m_transpose->setSuffix(tr(" semitones"));
    connect(m_transpose, &QSpinBox::valueChanged, this, [this] { applyProgression(); });
    row->addWidget(m_transpose);
    layout->addLayout(row);

    m_progressionEdit = new QPlainTextEdit;
    m_progressionEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_progressionEdit->setPlaceholderText(tr("| C | Am | F G | C |"));
    m_progressionEdit->setToolTip(
        tr("Bars are separated by '|', chords inside a bar share it evenly.\n"
           "'/' repeats the previous chord, '%' repeats the previous bar,\n"
           "lines starting with '#' are comments.\n"
           "Examples: C  Am7  F#m7b5  Bb7  Ebmaj7  Gsus4  D9  C/E"));
    m_progressionEdit->setMaximumHeight(130);
    layout->addWidget(m_progressionEdit);

    m_applyTimer = new QTimer(this);
    m_applyTimer->setSingleShot(true);
    m_applyTimer->setInterval(400);
    connect(m_applyTimer, &QTimer::timeout, this, [this] { applyProgression(); });
    connect(m_progressionEdit, &QPlainTextEdit::textChanged, m_applyTimer, qOverload<>(&QTimer::start));

    m_progressionError = new QLabel;
    m_progressionError->setStyleSheet(QStringLiteral("color: #d03030;"));
    layout->addWidget(m_progressionError);

    m_progressionView = new ProgressionView;
    layout->addWidget(m_progressionView);
    layout->addStretch();
    return w;
}

QWidget *MainWindow::buildLiveTab()
{
    auto *w = new QWidget;
    auto *layout = new QHBoxLayout(w);

    auto *form = new QFormLayout;
    auto *splitRow = new QHBoxLayout;
    m_useSplit = new QCheckBox(tr("Only notes below"));
    m_useSplit->setChecked(true);
    m_split = new NoteSpinBox;
    m_split->setValue(60);
    splitRow->addWidget(m_useSplit);
    splitRow->addWidget(m_split);
    splitRow->addStretch();
    connect(m_useSplit, &QCheckBox::toggled, m_split, &QWidget::setEnabled);
    form->addRow(tr("Chord zone:"), splitRow);

    m_minNotes = new QSpinBox;
    m_minNotes->setRange(2, 5);
    m_minNotes->setValue(3);
    form->addRow(tr("Minimum notes for a chord:"), m_minNotes);

    m_bassInversion = new QCheckBox(tr("Bass plays the lowest note of inversions (slash chords)"));
    form->addRow(QString(), m_bassInversion);

    m_autoStart = new QCheckBox(tr("Start the band with the first chord"));
    m_autoStart->setChecked(true);
    form->addRow(QString(), m_autoStart);

    m_thruMode = new QComboBox;
    m_thruMode->addItems({tr("Off"), tr("All notes"), tr("Only notes outside the chord zone")});
    form->addRow(tr("MIDI thru (channel 4):"), m_thruMode);

    m_thruProgram = new QComboBox;
    m_thruProgram->addItems(gmProgramNames());
    connect(m_thruProgram, &QComboBox::currentIndexChanged, this,
            [this](int p) { m_midi->programChange(BandEngine::ThruChannel, p); });
    form->addRow(tr("Thru instrument:"), m_thruProgram);
    layout->addLayout(form, 3);

    auto *status = new QVBoxLayout;
    status->addWidget(new QLabel(tr("Held notes:")));
    m_heldLabel = new QLabel(QStringLiteral("—"));
    m_heldLabel->setWordWrap(true);
    status->addWidget(m_heldLabel);
    status->addSpacing(12);
    status->addWidget(new QLabel(tr("Detected chord:")));
    m_detectedLabel = new QLabel(QStringLiteral("—"));
    QFont f = m_detectedLabel->font();
    f.setPointSizeF(f.pointSizeF() * 2.0);
    f.setBold(true);
    m_detectedLabel->setFont(f);
    status->addWidget(m_detectedLabel);
    auto *clear = new QPushButton(tr("Clear chord"));
    clear->setToolTip(tr("Silence bass and chords until the next chord is played"));
    connect(clear, &QPushButton::clicked, this, [this] {
        m_engine->setLiveChord({});
        m_detectedLabel->setText(QStringLiteral("—"));
    });
    status->addWidget(clear);
    status->addStretch();
    layout->addLayout(status, 2);
    return w;
}

QWidget *MainWindow::buildBandBox()
{
    auto *box = new QGroupBox(tr("Band"));
    auto *grid = new QGridLayout(box);
    const QStringList programs = gmProgramNames();

    for (int i = 0; i < BandEngine::PartCount; ++i) {
        const auto part = static_cast<BandEngine::Part>(i);
        PartControls &pc = m_parts[i];
        pc.enabled = new QCheckBox(tr(kPartNames[i]));
        pc.enabled->setChecked(part != BandEngine::Pad);
        pc.program = new QComboBox;
        if (part == BandEngine::Drums) {
            for (const auto &kit : kDrumKits)
                pc.program->addItem(QString::fromLatin1(kit.name), kit.program);
        } else {
            for (int p = 0; p < programs.size(); ++p)
                pc.program->addItem(programs[p], p);
        }
        pc.volume = new QSlider(Qt::Horizontal);
        pc.volume->setRange(0, 127);
        pc.volume->setValue(100);
        pc.volume->setToolTip(tr("Channel volume (CC 7)"));

        grid->addWidget(pc.enabled, i, 0);
        grid->addWidget(pc.program, i, 1);
        grid->addWidget(pc.volume, i, 2);

        connect(pc.enabled, &QCheckBox::toggled, this, [this, part](bool on) { m_engine->setPartEnabled(part, on); });
        connect(pc.program, &QComboBox::currentIndexChanged, this, [this, part] { sendPart(part); });
        connect(pc.volume, &QSlider::valueChanged, this,
                [this, part](int v) { m_midi->controlChange(BandEngine::channel(part), 7, v); });
        m_engine->setPartEnabled(part, pc.enabled->isChecked());
    }
    grid->setColumnStretch(1, 2);
    grid->setColumnStretch(2, 1);
    return box;
}

QWidget *MainWindow::buildConnectionBox()
{
    auto *box = new QGroupBox(tr("MIDI Connections"));
    auto *form = new QFormLayout(box);
    m_outputCombo = new QComboBox;
    m_inputCombo = new QComboBox;
    m_outputCombo->setToolTip(tr("Synthesizer that plays the band"));
    m_inputCombo->setToolTip(tr("Keyboard used for live chords and MIDI thru"));
    connect(m_outputCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onOutputChosen);
    connect(m_inputCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onInputChosen);
    form->addRow(tr("Output:"), m_outputCombo);
    form->addRow(tr("Input:"), m_inputCombo);
    auto *refresh = new QPushButton(tr("Refresh"));
    connect(refresh, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    form->addRow(QString(), refresh);
    return box;
}

// ---------------------------------------------------------------- transport

ChordMode MainWindow::currentMode() const
{
    return m_tabs->currentIndex() == 1 ? ChordMode::Live : ChordMode::Progression;
}

void MainWindow::togglePlay()
{
    if (m_engine->isRunning())
        stopBand();
    else
        startBand(m_countIn->isChecked());
}

void MainWindow::startBand(bool countIn)
{
    if (m_engine->isRunning())
        return;
    if (currentMode() == ChordMode::Progression && !applyProgression()) {
        statusBar()->showMessage(tr("Fix the chord progression first"), 4000);
        return;
    }
    if (m_outputId.isEmpty())
        statusBar()->showMessage(tr("No MIDI output connected – see Help ▸ Sound Setup"), 6000);

    m_engine->setStyle(currentStyle());
    m_engine->setTempo(m_tempo->value());
    m_engine->setSwing(m_swing->value() / 100.0);
    m_engine->setMode(currentMode());
    sendMixer();
    m_engine->start(countIn);
    updatePlayActions();
}

void MainWindow::stopBand()
{
    m_engine->stop();
    m_beatIndicator->clear();
    m_positionLabel->setText(tr("Stopped"));
    m_progressionView->setCurrentBar(-1);
    m_chordLabel->setText(QStringLiteral("—"));
    updatePlayActions();
}

void MainWindow::updatePlayActions()
{
    const bool running = m_engine->isRunning();
    m_playAction->setIcon(style()->standardIcon(running ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
    m_playAction->setText(running ? tr("Stop") : tr("Play"));
    m_stopAction->setEnabled(running);
}

void MainWindow::onPosition(int bar, int beat, int beatsPerBar)
{
    if (!m_engine->isRunning())
        return; // late event from a band thread that has just stopped
    m_beatIndicator->setBeat(beat, beatsPerBar, bar < 0);
    if (bar < 0) {
        m_positionLabel->setText(tr("Count-in %1").arg(beat + 1));
        return;
    }
    m_positionLabel->setText(tr("Bar %1 · Beat %2").arg(bar + 1).arg(beat + 1));
    if (currentMode() == ChordMode::Progression && !m_progression.empty())
        m_progressionView->setCurrentBar(bar % m_progression.size());
}

void MainWindow::onBandChord(const QString &name)
{
    m_chordLabel->setText(name.isEmpty() ? QStringLiteral("—") : name);
}

// ---------------------------------------------------------------- styles and progressions

// While a preset is selected, a style change only switches the patterns and
// keeps the preset's swing and band settings.
void MainWindow::onStyleChanged(int index)
{
    if (index < 0)
        return;
    m_engine->setStyle(m_styles[index]);
    if (m_presetCombo->currentIndex() <= 0)
        applyStyleDefaults(m_styles[index]);
}

void MainWindow::applyStyleDefaults(const mc::Style &s)
{
    m_swing->setValue(qRound(s.swing * 100));
    m_parts[BandEngine::Piano].program->setCurrentIndex(s.pianoProgram);
    m_parts[BandEngine::Bass].program->setCurrentIndex(s.bassProgram);
    m_parts[BandEngine::Pad].program->setCurrentIndex(s.padProgram);
    m_parts[BandEngine::Pad].enabled->setChecked(s.padDefault);
}

const mc::Style &MainWindow::currentStyle() const
{
    return m_styles[std::max(0, m_styleCombo->currentIndex())];
}

// Adds a style, or replaces the style with the same name (built-in styles
// can be overridden this way).
void MainWindow::addStyle(const mc::Style &style)
{
    for (size_t i = 0; i < m_styles.size(); ++i) {
        if (m_styles[i].name == style.name) {
            m_styles[i] = style;
            if (static_cast<int>(i) == m_styleCombo->currentIndex())
                m_engine->setStyle(style);
            return;
        }
    }
    m_styles.push_back(style);
    m_styleCombo->addItem(QString::fromStdString(style.name));
}

// Returns the number of styles loaded, 0 on error.
int MainWindow::loadStyleFile(const QString &path, bool interactive, QString *firstName)
{
    std::vector<mc::Style> styles;
    QString error;
    if (!readStyleFile(path, styles, error)) {
        const QString message = tr("Could not load styles from %1:\n%2").arg(path, error);
        if (interactive)
            QMessageBox::warning(this, tr("Load Styles"), message);
        else
            statusBar()->showMessage(message.simplified(), 8000);
        return 0;
    }
    for (const auto &style : styles)
        addStyle(style);
    if (firstName)
        *firstName = QString::fromStdString(styles.front().name);
    const QString absolute = QFileInfo(path).absoluteFilePath();
    if (!m_styleFiles.contains(absolute))
        m_styleFiles.append(absolute);
    return static_cast<int>(styles.size());
}

void MainWindow::openStyleFile()
{
    QSettings settings;
    QString dir = settings.value("styleDir").toString();
    if (dir.isEmpty()) {
        // Start in the example styles, both in a build tree and when installed.
        const QString appDir = QCoreApplication::applicationDirPath();
        for (const QString &candidate : {appDir + "/../examples/styles", appDir + "/../share/midicombo/styles"}) {
            if (QFileInfo(candidate).isDir()) {
                dir = QDir(candidate).absolutePath();
                break;
            }
        }
    }
    const QStringList paths = QFileDialog::getOpenFileNames(this, tr("Load Styles"),
                                                            dir.isEmpty() ? QDir::homePath() : dir,
                                                            tr("MidiCombo styles (*.xml);;All files (*)"));
    if (paths.isEmpty())
        return;
    settings.setValue("styleDir", QFileInfo(paths.first()).absolutePath());

    int loaded = 0;
    QString first;
    for (const QString &path : paths)
        loaded += loadStyleFile(path, true, first.isEmpty() ? &first : nullptr);
    if (loaded > 0) {
        // Select the first loaded style so it can be tried right away.
        m_styleCombo->setCurrentIndex(m_styleCombo->findText(first));
        statusBar()->showMessage(tr("Loaded %n style(s)", nullptr, loaded), 4000);
    }
}

void MainWindow::forgetLoadedStyles()
{
    const QString current = m_styleCombo->currentText();
    m_styleFiles.clear();
    m_styles = mc::builtinStyles();
    {
        QSignalBlocker block(m_styleCombo);
        m_styleCombo->clear();
        for (const auto &s : m_styles)
            m_styleCombo->addItem(QString::fromStdString(s.name));
    }
    const int idx = m_styleCombo->findText(current);
    m_styleCombo->setCurrentIndex(std::max(idx, 0));
    onStyleChanged(m_styleCombo->currentIndex());
    statusBar()->showMessage(tr("Removed all loaded styles"), 4000);
}

void MainWindow::onPresetChosen(int index)
{
    bool ok = false;
    const int presetIdx = m_presetCombo->itemData(index).toInt(&ok);
    if (ok && presetIdx >= 0 && presetIdx < m_presets.size())
        applyPreset(m_presets[presetIdx]);
}

void MainWindow::applyPreset(const PresetData &preset)
{
    // The style defaults go first; the preset's values override them.
    const int styleIdx = m_styleCombo->findText(preset.style);
    if (styleIdx >= 0) {
        QSignalBlocker block(m_styleCombo);
        m_styleCombo->setCurrentIndex(styleIdx);
    } else if (!preset.style.isEmpty()) {
        statusBar()->showMessage(tr("Unknown style '%1' – keeping the current style").arg(preset.style), 5000);
    }
    m_engine->setStyle(currentStyle());
    applyStyleDefaults(currentStyle());
    if (preset.tempo > 0)
        m_tempo->setValue(preset.tempo);
    if (preset.swing >= 0)
        m_swing->setValue(preset.swing);

    for (int i = 0; i < PresetData::PartCount; ++i) {
        const PresetData::Part &part = preset.parts[i];
        if (!part.present)
            continue;
        PartControls &pc = m_parts[i];
        pc.enabled->setChecked(part.enabled);
        const int programIdx = pc.program->findData(part.program);
        if (programIdx >= 0)
            pc.program->setCurrentIndex(programIdx);
        pc.volume->setValue(part.volume);
    }

    {
        QSignalBlocker block(m_transpose); // applyProgression() below picks it up
        m_transpose->setValue(preset.transpose);
    }
    m_progressionEdit->setPlainText(preset.progression);
    applyProgression();
}

PresetData MainWindow::currentPreset(const QString &name) const
{
    PresetData preset;
    preset.name = name;
    preset.style = m_styleCombo->currentText();
    preset.tempo = m_tempo->value();
    preset.swing = m_swing->value();
    preset.transpose = m_transpose->value();
    preset.progression = m_progressionEdit->toPlainText().trimmed();
    for (int i = 0; i < PresetData::PartCount; ++i) {
        PresetData::Part &part = preset.parts[i];
        part.present = true;
        part.enabled = m_parts[i].enabled->isChecked();
        part.program = m_parts[i].program->currentData().toInt();
        part.volume = m_parts[i].volume->value();
    }
    return preset;
}

// Adds a preset to the preset list, replacing a loaded preset of the same
// name, and returns its index in the combo box.
int MainWindow::addPreset(const PresetData &preset)
{
    const int builtinCount = static_cast<int>(mc::progressionPresets().size());
    for (int i = builtinCount; i < m_presets.size(); ++i) {
        if (m_presets[i].name == preset.name) {
            m_presets[i] = preset;
            return m_presetCombo->findData(i);
        }
    }
    if (m_presets.size() == builtinCount && builtinCount > 0)
        m_presetCombo->insertSeparator(m_presetCombo->count());
    m_presets.append(preset);
    m_presetCombo->addItem(preset.name, static_cast<int>(m_presets.size() - 1));
    return m_presetCombo->count() - 1;
}

void MainWindow::openPresetFile()
{
    QSettings settings;
    const QString path = QFileDialog::getOpenFileName(this, tr("Open Presets"),
                                                      settings.value("presetDir", QDir::homePath()).toString(),
                                                      tr("MidiCombo presets (*.xml);;All files (*)"));
    if (path.isEmpty())
        return;
    settings.setValue("presetDir", QFileInfo(path).absolutePath());

    QList<PresetData> presets;
    QString error;
    if (!readPresetFile(path, presets, error)) {
        QMessageBox::warning(this, tr("Open Presets"), tr("Could not read %1:\n%2").arg(path, error));
        return;
    }

    int firstIdx = -1;
    for (PresetData &p : presets) {
        if (p.name.isEmpty())
            p.name = QFileInfo(path).completeBaseName();
        const int idx = addPreset(p);
        if (firstIdx < 0)
            firstIdx = idx;
    }
    m_presetCombo->setCurrentIndex(firstIdx);
    applyPreset(presets.first());
    statusBar()->showMessage(tr("Loaded %n preset(s) from %1", nullptr, presets.size())
                                 .arg(QFileInfo(path).fileName()), 4000);
}

void MainWindow::savePresetFile()
{
    QSettings settings;
    const int presetIdx = m_presetCombo->currentData().isValid() ? m_presetCombo->currentData().toInt() : -1;
    const QString suggestedName = presetIdx >= 0 ? m_presets[presetIdx].name : tr("My Song");
    const QString dir = settings.value("presetDir", QDir::homePath()).toString();

    QFileDialog dialog(this, tr("Save Preset As"), dir, tr("MidiCombo presets (*.xml)"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDefaultSuffix(QStringLiteral("xml"));
    dialog.selectFile(suggestedName + QStringLiteral(".xml"));
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty())
        return;
    const QString path = dialog.selectedFiles().first();
    settings.setValue("presetDir", QFileInfo(path).absolutePath());

    const PresetData preset = currentPreset(QFileInfo(path).completeBaseName());
    QString error;
    if (!writePresetFile(path, {preset}, error)) {
        QMessageBox::warning(this, tr("Save Preset"), tr("Could not write %1:\n%2").arg(path, error));
        return;
    }
    m_presetCombo->setCurrentIndex(addPreset(preset));
    statusBar()->showMessage(tr("Saved preset to %1").arg(QFileInfo(path).fileName()), 4000);
}

bool MainWindow::applyProgression()
{
    m_applyTimer->stop();
    mc::Progression p;
    std::string error;
    if (!mc::Progression::parse(m_progressionEdit->toPlainText().toStdString(), p, error)) {
        m_progressionError->setText(QString::fromStdString(error));
        return false;
    }
    m_progressionError->clear();
    m_progression = p.transposed(m_transpose->value());
    m_engine->setProgression(m_progression);
    m_progressionView->setProgression(m_progression);
    return true;
}

// ---------------------------------------------------------------- MIDI ports

void MainWindow::refreshPorts()
{
    auto fill = [this](QComboBox *combo, const QList<MidiPort> &ports, QString &currentId) {
        QSignalBlocker block(combo);
        combo->clear();
        combo->addItem(tr("— not connected —"), QString());
        for (const auto &p : ports)
            combo->addItem(p.label(), p.id());
        const int idx = combo->findData(currentId);
        combo->setCurrentIndex(std::max(idx, 0));
        if (idx < 0)
            currentId.clear(); // the port has disappeared
    };
    fill(m_outputCombo, m_midi->outputDestinations(), m_outputId);
    fill(m_inputCombo, m_midi->inputSources(), m_inputId);
}

void MainWindow::onOutputChosen(int index)
{
    const QString id = m_outputCombo->itemData(index).toString();
    if (id == m_outputId)
        return;
    int client, port;
    if (parsePortId(m_outputId, client, port))
        m_midi->disconnectOutput(client, port);
    m_outputId.clear();
    if (parsePortId(id, client, port)) {
        if (m_midi->connectOutput(client, port)) {
            m_outputId = id;
            sendMixer();
        } else {
            statusBar()->showMessage(tr("Could not connect to %1").arg(m_outputCombo->itemText(index)), 5000);
            QSignalBlocker block(m_outputCombo);
            m_outputCombo->setCurrentIndex(0);
        }
    }
}

void MainWindow::onInputChosen(int index)
{
    const QString id = m_inputCombo->itemData(index).toString();
    if (id == m_inputId)
        return;
    int client, port;
    if (parsePortId(m_inputId, client, port))
        m_midi->disconnectInput(client, port);
    m_inputId.clear();
    m_held.clear();
    updateHeldLabel();
    if (parsePortId(id, client, port)) {
        if (m_midi->connectInput(client, port)) {
            m_inputId = id;
        } else {
            statusBar()->showMessage(tr("Could not connect to %1").arg(m_inputCombo->itemText(index)), 5000);
            QSignalBlocker block(m_inputCombo);
            m_inputCombo->setCurrentIndex(0);
        }
    }
}

void MainWindow::sendPart(BandEngine::Part part)
{
    const int ch = BandEngine::channel(part);
    const PartControls &pc = m_parts[part];
    m_midi->programChange(ch, pc.program->currentData().toInt());
    m_midi->controlChange(ch, 7, pc.volume->value());
    m_midi->controlChange(ch, 91, 40); // a little reverb
}

void MainWindow::sendMixer()
{
    for (int i = 0; i < BandEngine::PartCount; ++i)
        sendPart(static_cast<BandEngine::Part>(i));
    m_midi->programChange(BandEngine::ThruChannel, m_thruProgram->currentIndex());
    m_midi->controlChange(BandEngine::ThruChannel, 7, 110);
    m_midi->controlChange(BandEngine::ThruChannel, 91, 40);
}

// ---------------------------------------------------------------- live input

bool MainWindow::inChordZone(int note) const
{
    return !m_useSplit->isChecked() || note < m_split->value();
}

void MainWindow::onNoteOn(int, int note, int velocity)
{
    const bool inZone = inChordZone(note);
    const int thru = m_thruMode->currentIndex();
    if (thru == 1 || (thru == 2 && !inZone)) {
        m_midi->noteOn(BandEngine::ThruChannel, note, velocity);
        m_thruNotes.insert(note);
    }
    if (inZone) {
        m_held.insert(note);
        updateDetection();
    }
}

void MainWindow::onNoteOff(int, int note)
{
    if (m_thruNotes.erase(note))
        m_midi->noteOff(BandEngine::ThruChannel, note);
    // The detected chord is latched: releasing keys keeps the band playing it.
    if (m_held.erase(note))
        updateHeldLabel();
}

void MainWindow::onControl(int, int controller, int value)
{
    if (m_thruMode->currentIndex() != 0)
        m_midi->controlChange(BandEngine::ThruChannel, controller, value);
}

void MainWindow::updateHeldLabel()
{
    QStringList names;
    for (int n : m_held)
        names << QString::fromStdString(mc::noteName(n));
    m_heldLabel->setText(names.isEmpty() ? QStringLiteral("—") : names.join(QStringLiteral("  ")));
}

void MainWindow::updateDetection()
{
    updateHeldLabel();
    if (static_cast<int>(m_held.size()) < m_minNotes->value())
        return;
    auto chord = m_detector.detect(std::vector<int>(m_held.begin(), m_held.end()));
    if (!chord)
        return;
    if (!m_bassInversion->isChecked())
        chord->bass = -1;
    m_detectedLabel->setText(QString::fromStdString(chord->name()));
    m_engine->setLiveChord(*chord);
    if (currentMode() == ChordMode::Live && m_autoStart->isChecked() && !m_engine->isRunning())
        startBand(false);
}

// ---------------------------------------------------------------- settings

void MainWindow::loadSettings()
{
    QSettings s;
    const QStringList styleFiles = s.value("styleFiles").toStringList();
    for (const QString &path : styleFiles)
        loadStyleFile(path, false);

    m_styleCombo->setCurrentIndex(std::max(0, m_styleCombo->findText(s.value("styleName").toString())));
    m_engine->setStyle(currentStyle());
    applyStyleDefaults(currentStyle()); // the saved settings below override them

    m_tempo->setValue(s.value("tempo", currentStyle().tempo).toInt());
    if (s.contains("swing"))
        m_swing->setValue(s.value("swing").toInt());
    m_countIn->setChecked(s.value("countIn", true).toBool());

    const QString defaultText = QString::fromUtf8(mc::progressionPresets()[3].text);
    m_progressionEdit->setPlainText(s.value("progression", defaultText).toString());
    m_transpose->setValue(s.value("transpose", 0).toInt());
    m_tabs->setCurrentIndex(s.value("mode", 0).toInt());

    m_useSplit->setChecked(s.value("live/useSplit", true).toBool());
    m_split->setValue(s.value("live/split", 60).toInt());
    m_minNotes->setValue(s.value("live/minNotes", 3).toInt());
    m_bassInversion->setChecked(s.value("live/bassInversion", false).toBool());
    m_autoStart->setChecked(s.value("live/autoStart", true).toBool());
    m_thruMode->setCurrentIndex(s.value("live/thruMode", 0).toInt());
    m_thruProgram->setCurrentIndex(s.value("live/thruProgram", 0).toInt());

    for (int i = 0; i < BandEngine::PartCount; ++i) {
        const QString key = QStringLiteral("part%1/").arg(i);
        PartControls &pc = m_parts[i];
        if (s.contains(key + "enabled"))
            pc.enabled->setChecked(s.value(key + "enabled").toBool());
        if (s.contains(key + "program"))
            pc.program->setCurrentIndex(s.value(key + "program").toInt());
        pc.volume->setValue(s.value(key + "volume", 100).toInt());
    }

    // Ports are matched by name because ALSA client numbers can change.
    const QString out = s.value("outputPort").toString();
    int idx = out.isEmpty() ? -1 : m_outputCombo->findText(out);
    if (idx < 0 && out.isEmpty()) {
        for (int i = 1; i < m_outputCombo->count() && idx < 0; ++i) {
            const QString t = m_outputCombo->itemText(i).toLower();
            if (!t.contains("through")
                && (t.contains("fluid") || t.contains("timidity") || t.contains("synth")))
                idx = i;
        }
    }
    if (idx > 0)
        m_outputCombo->setCurrentIndex(idx);
    const int inIdx = m_inputCombo->findText(s.value("inputPort").toString());
    if (inIdx > 0)
        m_inputCombo->setCurrentIndex(inIdx);
}

void MainWindow::saveSettings()
{
    QSettings s;
    s.setValue("styleName", m_styleCombo->currentText());
    s.setValue("styleFiles", m_styleFiles);
    s.setValue("tempo", m_tempo->value());
    s.setValue("swing", m_swing->value());
    s.setValue("countIn", m_countIn->isChecked());
    s.setValue("progression", m_progressionEdit->toPlainText());
    s.setValue("transpose", m_transpose->value());
    s.setValue("mode", m_tabs->currentIndex());

    s.setValue("live/useSplit", m_useSplit->isChecked());
    s.setValue("live/split", m_split->value());
    s.setValue("live/minNotes", m_minNotes->value());
    s.setValue("live/bassInversion", m_bassInversion->isChecked());
    s.setValue("live/autoStart", m_autoStart->isChecked());
    s.setValue("live/thruMode", m_thruMode->currentIndex());
    s.setValue("live/thruProgram", m_thruProgram->currentIndex());

    for (int i = 0; i < BandEngine::PartCount; ++i) {
        const QString key = QStringLiteral("part%1/").arg(i);
        s.setValue(key + "enabled", m_parts[i].enabled->isChecked());
        s.setValue(key + "program", m_parts[i].program->currentIndex());
        s.setValue(key + "volume", m_parts[i].volume->value());
    }
    s.setValue("outputPort", m_outputCombo->currentIndex() > 0 ? m_outputCombo->currentText() : QString());
    s.setValue("inputPort", m_inputCombo->currentIndex() > 0 ? m_inputCombo->currentText() : QString());
}
