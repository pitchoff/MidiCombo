#include "Widgets.h"

#include <QPainter>

ProgressionView::ProgressionView(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
}

void ProgressionView::setProgression(const mc::Progression &progression)
{
    m_labels.clear();
    for (const auto &bar : progression.bars) {
        QStringList names;
        for (size_t i = 0; i < bar.size(); ++i)
            names << (i > 0 && bar[i] == bar[i - 1] ? QStringLiteral("/") : QString::fromStdString(bar[i].name()));
        m_labels << names.join(QStringLiteral("  "));
    }
    m_current = -1;
    updateGeometry();
    update();
}

void ProgressionView::setCurrentBar(int bar)
{
    if (bar == m_current)
        return;
    m_current = bar;
    update();
}

QSize ProgressionView::sizeHint() const
{
    return minimumSizeHint().expandedTo(QSize(400, 0));
}

QSize ProgressionView::minimumSizeHint() const
{
    const int rows = std::max<int>(1, (m_labels.size() + BarsPerRow - 1) / BarsPerRow);
    return QSize(240, rows * RowHeight + 8);
}

void ProgressionView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPalette pal = palette();
    const qreal cellW = (width() - 8) / qreal(BarsPerRow);
    QFont f = font();
    f.setPointSizeF(f.pointSizeF() * 1.3);
    f.setBold(true);
    p.setFont(f);

    for (int i = 0; i < m_labels.size(); ++i) {
        const QRectF r(4 + (i % BarsPerRow) * cellW, 4 + (i / BarsPerRow) * RowHeight, cellW - 4, RowHeight - 6);
        const bool current = i == m_current;
        p.setPen(QPen(pal.color(QPalette::Mid), 1));
        p.setBrush(current ? pal.color(QPalette::Highlight) : pal.color(QPalette::Base));
        p.drawRoundedRect(r, 5, 5);
        p.setPen(current ? pal.color(QPalette::HighlightedText) : pal.color(QPalette::Text));
        p.drawText(r, Qt::AlignCenter, m_labels[i]);
    }
}

BeatIndicator::BeatIndicator(QWidget *parent)
    : QWidget(parent)
{
}

void BeatIndicator::setBeat(int beat, int beatsPerBar, bool countIn)
{
    m_beat = beat;
    m_beats = beatsPerBar;
    m_countIn = countIn;
    update();
}

void BeatIndicator::clear()
{
    m_beat = -1;
    update();
}

QSize BeatIndicator::sizeHint() const
{
    return QSize(4 * 28, 28);
}

void BeatIndicator::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int d = std::min(height() - 6, 20);
    for (int i = 0; i < m_beats; ++i) {
        const QRectF r(4 + i * (d + 8), (height() - d) / 2.0, d, d);
        QColor c = palette().color(QPalette::Mid);
        if (i == m_beat)
            c = m_countIn ? QColor(230, 160, 30) : (i == 0 ? QColor(220, 60, 50) : QColor(60, 180, 75));
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(r);
    }
}

NoteSpinBox::NoteSpinBox(QWidget *parent)
    : QSpinBox(parent)
{
    setRange(0, 127);
}

QString NoteSpinBox::textFromValue(int value) const
{
    return QString::fromStdString(mc::noteName(value));
}

int NoteSpinBox::valueFromText(const QString &text) const
{
    const int n = mc::parseNoteName(text.toStdString());
    return n >= 0 ? n : value();
}

QValidator::State NoteSpinBox::validate(QString &text, int &) const
{
    if (mc::parseNoteName(text.toStdString()) >= 0)
        return QValidator::Acceptable;
    return text.size() <= 4 ? QValidator::Intermediate : QValidator::Invalid;
}

QStringList gmProgramNames()
{
    static const char *const names[128] = {
        "Acoustic Grand Piano", "Bright Acoustic Piano", "Electric Grand Piano", "Honky-tonk Piano",
        "Electric Piano 1", "Electric Piano 2", "Harpsichord", "Clavinet",
        "Celesta", "Glockenspiel", "Music Box", "Vibraphone",
        "Marimba", "Xylophone", "Tubular Bells", "Dulcimer",
        "Drawbar Organ", "Percussive Organ", "Rock Organ", "Church Organ",
        "Reed Organ", "Accordion", "Harmonica", "Tango Accordion",
        "Acoustic Guitar (nylon)", "Acoustic Guitar (steel)", "Electric Guitar (jazz)", "Electric Guitar (clean)",
        "Electric Guitar (muted)", "Overdriven Guitar", "Distortion Guitar", "Guitar Harmonics",
        "Acoustic Bass", "Electric Bass (finger)", "Electric Bass (pick)", "Fretless Bass",
        "Slap Bass 1", "Slap Bass 2", "Synth Bass 1", "Synth Bass 2",
        "Violin", "Viola", "Cello", "Contrabass",
        "Tremolo Strings", "Pizzicato Strings", "Orchestral Harp", "Timpani",
        "String Ensemble 1", "String Ensemble 2", "Synth Strings 1", "Synth Strings 2",
        "Choir Aahs", "Voice Oohs", "Synth Choir", "Orchestra Hit",
        "Trumpet", "Trombone", "Tuba", "Muted Trumpet",
        "French Horn", "Brass Section", "Synth Brass 1", "Synth Brass 2",
        "Soprano Sax", "Alto Sax", "Tenor Sax", "Baritone Sax",
        "Oboe", "English Horn", "Bassoon", "Clarinet",
        "Piccolo", "Flute", "Recorder", "Pan Flute",
        "Blown Bottle", "Shakuhachi", "Whistle", "Ocarina",
        "Lead 1 (square)", "Lead 2 (sawtooth)", "Lead 3 (calliope)", "Lead 4 (chiff)",
        "Lead 5 (charang)", "Lead 6 (voice)", "Lead 7 (fifths)", "Lead 8 (bass + lead)",
        "Pad 1 (new age)", "Pad 2 (warm)", "Pad 3 (polysynth)", "Pad 4 (choir)",
        "Pad 5 (bowed)", "Pad 6 (metallic)", "Pad 7 (halo)", "Pad 8 (sweep)",
        "FX 1 (rain)", "FX 2 (soundtrack)", "FX 3 (crystal)", "FX 4 (atmosphere)",
        "FX 5 (brightness)", "FX 6 (goblins)", "FX 7 (echoes)", "FX 8 (sci-fi)",
        "Sitar", "Banjo", "Shamisen", "Koto",
        "Kalimba", "Bagpipe", "Fiddle", "Shanai",
        "Tinkle Bell", "Agogo", "Steel Drums", "Woodblock",
        "Taiko Drum", "Melodic Tom", "Synth Drum", "Reverse Cymbal",
        "Guitar Fret Noise", "Breath Noise", "Seashore", "Bird Tweet",
        "Telephone Ring", "Helicopter", "Applause", "Gunshot",
    };
    QStringList list;
    for (int i = 0; i < 128; ++i)
        list << QStringLiteral("%1 %2").arg(i + 1, 3).arg(QString::fromLatin1(names[i]));
    return list;
}
