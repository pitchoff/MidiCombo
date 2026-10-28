#pragma once

#include "Progression.h"

#include <QSpinBox>
#include <QStringList>
#include <QWidget>

// Grid of bars showing the chord progression, with the playing bar highlighted.
class ProgressionView : public QWidget {
    Q_OBJECT
public:
    explicit ProgressionView(QWidget *parent = nullptr);
    void setProgression(const mc::Progression &progression);
    void setCurrentBar(int bar);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    static constexpr int BarsPerRow = 4;
    static constexpr int RowHeight = 44;
    QStringList m_labels;
    int m_current = -1;
};

// Row of dots, one per beat; the current beat is lit.
class BeatIndicator : public QWidget {
    Q_OBJECT
public:
    explicit BeatIndicator(QWidget *parent = nullptr);
    void setBeat(int beat, int beatsPerBar, bool countIn);
    void clear();
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_beat = -1;
    int m_beats = 4;
    bool m_countIn = false;
};

// Spin box that shows MIDI note numbers as note names (C4 = 60).
class NoteSpinBox : public QSpinBox {
    Q_OBJECT
public:
    explicit NoteSpinBox(QWidget *parent = nullptr);

protected:
    QString textFromValue(int value) const override;
    int valueFromText(const QString &text) const override;
    QValidator::State validate(QString &text, int &pos) const override;
};

QStringList gmProgramNames();
