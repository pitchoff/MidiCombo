#pragma once

#include "Style.h"

#include <QDialog>
#include <QString>
#include <QWidget>

#include <string>
#include <vector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPainter;
class QPushButton;
class QSpinBox;
class QTimer;

// Common geometry of the drum grid and the tracks, so that their steps line
// up when they are stacked in the editor.
class StepGrid : public QWidget {
    Q_OBJECT
public:
    static constexpr int LabelWidth = 150;
    static constexpr int CellWidth = 22;

    explicit StepGrid(QWidget *parent = nullptr);
    void setMeter(int beatsPerBar, int stepsPerBeat, int steps);

signals:
    void changed();

protected:
    int stepX(int step) const { return LabelWidth + step * CellWidth; }
    int stepAt(double x) const; // -1 outside the steps
    // Background stripes per beat and the step, beat and bar lines.
    void drawSteps(QPainter &p, int top, int bottom) const;
    void drawCursor(QPainter &p, int step, int top, int height) const;
    virtual void updateSize() = 0;

    int m_beatsPerBar = 4;
    int m_stepsPerBeat = 2;
    int m_steps = 8;
};

// Drum pattern editor: one row per instrument, one column per step.
// Click to set or clear a step, shift-click for an accent, right-click to
// cycle the velocity; dragging paints. A click on a row name opens a menu
// to change, move or remove the instrument; the last row adds one.
class DrumGrid : public StepGrid {
    Q_OBJECT
public:
    explicit DrumGrid(QWidget *parent = nullptr);
    void setRows(const std::vector<mc::DrumRow> &rows);
    const std::vector<mc::DrumRow> &rows() const { return m_rows; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void updateSize() override;

private:
    static constexpr int HeaderHeight = 18;
    static constexpr int RowHeight = 22;

    int rowAt(double y) const; // -1 header, rows().size() = "add" row
    void setStep(int row, int step, char value);
    void rowMenu(int row, const QPoint &globalPos);
    void addRowMenu(const QPoint &globalPos);
    int chooseDrum(const QPoint &globalPos, int current);

    std::vector<mc::DrumRow> m_rows;
    int m_cursorRow = 0;
    int m_cursorStep = 0;
    char m_paint = 0; // value painted while dragging, 0 = not dragging
};

// Single-lane step track for bass or comp. A note starts with its token and
// lasts over the following hold steps ('-'). Click a step and type a token,
// '-' to hold, '.' or Delete for a rest; drag a note to change its length;
// double-click toggles a note; right-click opens the list of tokens.
class StepTrack : public StepGrid {
    Q_OBJECT
public:
    struct Token {
        char token;
        QString description;
    };
    StepTrack(const QString &name, std::vector<Token> tokens, QWidget *parent = nullptr);
    void setPattern(const std::string &pattern);
    const std::string &pattern() const { return m_pattern; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void updateSize() override;

private:
    static constexpr int TrackHeight = 30;

    bool isNote(char c) const { return c != '.' && c != '-'; }
    int noteStart(int step) const; // the note a step belongs to, -1 for rests
    void set(int step, char value);
    void setRest(int step);
    void tokenMenu(int step, const QPoint &globalPos);
    char tokenForKey(const QString &text) const;

    QString m_name;
    std::vector<Token> m_tokens;
    std::string m_pattern;
    int m_cursor = 0;
    int m_anchor = -1; // start of the note being resized
    int m_pressStep = -1;
    std::string m_pressPattern;
};

// Window to create and change band styles. "Apply" hands the style to the
// main window, "Save to File" also writes it to a style file.
class StyleEditor : public QDialog {
    Q_OBJECT
public:
    explicit StyleEditor(QWidget *parent = nullptr);

    // The styles offered in the "Edit" list.
    void setStyles(const std::vector<mc::Style> &styles);
    // Loads a style unless there are unapplied changes.
    void editStyle(const QString &name);
    bool isModified() const { return m_modified; }
    mc::Style style() const;
    bool isTesting() const;
    // Switches the Test button off without emitting testStopped(), for when
    // the band was stopped elsewhere.
    void setTesting(bool testing);

signals:
    void applyRequested(const mc::Style &style);
    void savedToFile(const QString &path, const QString &styleName);
    // While testing, the band plays the edited style and the variation open
    // in the editor (0 = main); testStyle() is emitted again after every change.
    void testStyle(const mc::Style &style, int variation);
    void testStopped();

protected:
    void hideEvent(QHideEvent *event) override;

private:
    void load(const mc::Style &style);
    void setModified(bool modified);
    void onMeterChanged();
    void onFillToggled(bool on);
    void storePatterns(mc::Style &style) const; // the grids into the open variation
    void showVariation(int index);
    void chooseVariation(int index);
    void addVariation();
    void removeVariation();
    void renameVariation();
    void updateVariationCombo();
    void ownPartToggled();
    bool confirmDiscard();
    void chooseStyle(int index);
    void newStyle();
    void revert();
    bool validated(mc::Style &style);
    bool apply();
    void saveToFile();
    void updateTitle();

    std::vector<mc::Style> m_styles;
    QString m_originalName; // name of the style being edited, empty for a new one
    bool m_modified = false;
    bool m_loading = false;
    int m_beats = 4, m_stepsPerBeat = 2, m_bars = 1; // meter of the patterns in the widgets
    mc::Style m_patterns; // the patterns; those of the open variation are in the grids
    int m_variation = 0;  // the open variation, 0 = main

    QComboBox *m_styleCombo = nullptr;
    QLineEdit *m_name = nullptr;
    QSpinBox *m_tempo = nullptr;
    QSpinBox *m_beatsSpin = nullptr;
    QComboBox *m_stepsCombo = nullptr;
    QSpinBox *m_barsSpin = nullptr;
    QSpinBox *m_swing = nullptr;
    QComboBox *m_swingUnit = nullptr;
    QComboBox *m_crash = nullptr;
    QComboBox *m_compProgram = nullptr;
    QSpinBox *m_compVelocity = nullptr;
    QComboBox *m_voicing = nullptr;
    QComboBox *m_bassProgram = nullptr;
    QSpinBox *m_bassVelocity = nullptr;
    QComboBox *m_padProgram = nullptr;
    QCheckBox *m_padDefault = nullptr;
    QComboBox *m_variationCombo = nullptr;
    QPushButton *m_removeVariation = nullptr;
    QPushButton *m_renameVariation = nullptr;
    QCheckBox *m_ownDrums = nullptr;
    QCheckBox *m_ownBass = nullptr;
    QCheckBox *m_ownComp = nullptr;
    DrumGrid *m_drums = nullptr;
    DrumGrid *m_fill = nullptr;
    QCheckBox *m_fillEnabled = nullptr;
    StepTrack *m_bass = nullptr;
    StepTrack *m_comp = nullptr;
    QPushButton *m_testButton = nullptr;
    QTimer *m_testTimer = nullptr; // collects quick edits into one update
};
