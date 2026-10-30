#include "StyleEditor.h"

#include "StyleFile.h"
#include "Widgets.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {

QString drumLabel(int note)
{
    const QString name = drumName(note);
    return name.isEmpty() ? QStringLiteral("note %1").arg(note) : QStringLiteral("%1 (%2)").arg(name).arg(note);
}

QColor withAlpha(QColor c, int alpha)
{
    c.setAlpha(alpha);
    return c;
}

QComboBox *programCombo()
{
    auto *combo = new QComboBox;
    combo->addItems(gmProgramNames()); // the index is the program number
    return combo;
}

mc::Style defaultStyle(const QString &name)
{
    mc::Style s;
    s.name = name.toStdString();
    s.drums = {{42, "xxxxxxxx"}, {38, "..X...X."}, {36, "X...X..."}};
    s.bass = "R-..5-..";
    s.comp = "C-..C-..";
    return s;
}

} // namespace

// ---------------------------------------------------------------- StepGrid

StepGrid::StepGrid(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(false);
}

void StepGrid::setMeter(int beatsPerBar, int stepsPerBeat, int steps)
{
    m_beatsPerBar = beatsPerBar;
    m_stepsPerBeat = stepsPerBeat;
    m_steps = steps;
    updateSize();
    update();
}

int StepGrid::stepAt(double x) const
{
    if (x < LabelWidth)
        return -1;
    const int step = static_cast<int>((x - LabelWidth) / CellWidth);
    return step < m_steps ? step : -1;
}

void StepGrid::drawSteps(QPainter &p, int top, int bottom) const
{
    const QPalette &pal = palette();
    const int stepsPerBar = m_beatsPerBar * m_stepsPerBeat;
    for (int beat = 0; beat * m_stepsPerBeat < m_steps; ++beat) {
        const int x = stepX(beat * m_stepsPerBeat);
        p.fillRect(x, top, m_stepsPerBeat * CellWidth, bottom - top,
                   beat % 2 ? pal.alternateBase() : pal.base());
    }
    for (int i = 0; i <= m_steps; ++i) {
        int alpha = 40, width = 1;
        if (i % stepsPerBar == 0) {
            alpha = 220;
            width = 2;
        } else if (i % m_stepsPerBeat == 0) {
            alpha = 110;
        }
        p.setPen(QPen(withAlpha(pal.text().color(), alpha), width));
        p.drawLine(stepX(i), top, stepX(i), bottom);
    }
}

void StepGrid::drawCursor(QPainter &p, int step, int top, int height) const
{
    if (!hasFocus() || step < 0 || step >= m_steps)
        return;
    p.setPen(QPen(palette().highlight().color().darker(130), 2));
    p.setBrush(Qt::NoBrush);
    p.drawRect(stepX(step) + 1, top + 1, CellWidth - 2, height - 2);
}

// ---------------------------------------------------------------- DrumGrid

DrumGrid::DrumGrid(QWidget *parent)
    : StepGrid(parent)
{
    updateSize();
}

void DrumGrid::setRows(const std::vector<mc::DrumRow> &rows)
{
    m_rows = rows;
    for (auto &r : m_rows)
        r.steps.resize(static_cast<size_t>(m_steps), '.');
    m_cursorRow = std::clamp(m_cursorRow, 0, std::max(0, static_cast<int>(m_rows.size()) - 1));
    updateSize();
    update();
}

void DrumGrid::updateSize()
{
    for (auto &r : m_rows)
        r.steps.resize(static_cast<size_t>(m_steps), '.');
    m_cursorStep = std::clamp(m_cursorStep, 0, std::max(0, m_steps - 1));
    setFixedSize(stepX(m_steps) + 2, HeaderHeight + RowHeight * (static_cast<int>(m_rows.size()) + 1) + 1);
}

int DrumGrid::rowAt(double y) const
{
    if (y < HeaderHeight)
        return -1;
    return std::min(static_cast<int>((y - HeaderHeight) / RowHeight), static_cast<int>(m_rows.size()));
}

void DrumGrid::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPalette &pal = palette();
    const int rows = static_cast<int>(m_rows.size());
    const int bottom = HeaderHeight + RowHeight * rows;
    p.fillRect(rect(), pal.window());

    // Header: bar numbers in bold, the other beats in grey
    QFont font = p.font();
    for (int beat = 0; beat * m_stepsPerBeat < m_steps; ++beat) {
        const bool barStart = beat % m_beatsPerBar == 0;
        font.setBold(barStart);
        p.setFont(font);
        p.setPen(barStart ? pal.text().color() : pal.placeholderText().color());
        const QString text = QString::number(barStart ? beat / m_beatsPerBar + 1 : beat % m_beatsPerBar + 1);
        p.drawText(QRect(stepX(beat * m_stepsPerBeat) + 3, 0, CellWidth * m_stepsPerBeat, HeaderHeight),
                   Qt::AlignLeft | Qt::AlignVCenter, text);
    }
    font.setBold(false);
    p.setFont(font);

    drawSteps(p, HeaderHeight, bottom);
    const QColor hit = pal.highlight().color();
    for (int r = 0; r < rows; ++r) {
        const int top = HeaderHeight + r * RowHeight;
        p.setPen(pal.buttonText().color());
        p.fillRect(0, top, LabelWidth - 1, RowHeight, pal.button());
        p.drawText(QRect(6, top, LabelWidth - 10, RowHeight), Qt::AlignLeft | Qt::AlignVCenter,
                   drumLabel(m_rows[r].note));
        p.setPen(withAlpha(pal.text().color(), 40));
        p.drawLine(0, top + RowHeight, stepX(m_steps), top + RowHeight);

        for (int s = 0; s < m_steps; ++s) {
            const char c = m_rows[r].steps[static_cast<size_t>(s)];
            const QRectF cell(stepX(s) + 3, top + 3, CellWidth - 6, RowHeight - 6);
            p.setPen(Qt::NoPen);
            switch (c) {
            case 'X': p.setBrush(hit.darker(115)); p.drawRoundedRect(cell, 3, 3); break;
            case 'x': p.setBrush(withAlpha(hit, 190)); p.drawRoundedRect(cell, 3, 3); break;
            case 'o': p.setBrush(withAlpha(hit, 100)); p.drawRoundedRect(cell, 3, 3); break;
            case 'g': p.setBrush(withAlpha(hit, 120)); p.drawEllipse(cell.center(), 3.5, 3.5); break;
            default: break;
            }
        }
    }

    // "Add" row
    const int addTop = bottom;
    p.setPen(pal.placeholderText().color());
    p.drawText(QRect(6, addTop, LabelWidth - 10, RowHeight), Qt::AlignLeft | Qt::AlignVCenter, tr("+ add drum…"));

    if (rows > 0)
        drawCursor(p, m_cursorStep, HeaderHeight + m_cursorRow * RowHeight, RowHeight);
}

void DrumGrid::setStep(int row, int step, char value)
{
    if (row < 0 || row >= static_cast<int>(m_rows.size()) || step < 0 || step >= m_steps)
        return;
    char &c = m_rows[row].steps[static_cast<size_t>(step)];
    if (c == value)
        return;
    c = value;
    update();
    emit changed();
}

void DrumGrid::mousePressEvent(QMouseEvent *event)
{
    const QPointF pos = event->position();
    const int row = rowAt(pos.y());
    if (row < 0)
        return;
    const QPoint global = event->globalPosition().toPoint();
    if (row == static_cast<int>(m_rows.size())) {
        addRowMenu(global);
        return;
    }
    const int step = stepAt(pos.x());
    if (step < 0) {
        if (pos.x() < LabelWidth)
            rowMenu(row, global);
        return;
    }
    m_cursorRow = row;
    m_cursorStep = step;
    const char current = m_rows[row].steps[static_cast<size_t>(step)];
    if (event->button() == Qt::RightButton) {
        // Cycle the velocity: normal, accent, soft, ghost
        static const std::string cycle = "xXog";
        const size_t i = cycle.find(current);
        setStep(row, step, i == std::string::npos ? 'X' : cycle[(i + 1) % cycle.size()]);
    } else if (event->button() == Qt::LeftButton) {
        if (event->modifiers() & Qt::ShiftModifier)
            m_paint = 'X';
        else
            m_paint = current == '.' ? 'x' : '.';
        setStep(row, step, m_paint);
    }
    update();
}

void DrumGrid::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_paint)
        return;
    const int row = rowAt(event->position().y());
    const int step = stepAt(event->position().x());
    if (row >= 0 && row < static_cast<int>(m_rows.size()) && step >= 0) {
        m_cursorRow = row;
        m_cursorStep = step;
        setStep(row, step, m_paint);
    }
}

void DrumGrid::mouseReleaseEvent(QMouseEvent *)
{
    m_paint = 0;
}

void DrumGrid::keyPressEvent(QKeyEvent *event)
{
    const int rows = static_cast<int>(m_rows.size());
    if (rows == 0) {
        StepGrid::keyPressEvent(event);
        return;
    }
    switch (event->key()) {
    case Qt::Key_Left: m_cursorStep = std::max(0, m_cursorStep - 1); break;
    case Qt::Key_Right: m_cursorStep = std::min(m_steps - 1, m_cursorStep + 1); break;
    case Qt::Key_Up: m_cursorRow = std::max(0, m_cursorRow - 1); break;
    case Qt::Key_Down: m_cursorRow = std::min(rows - 1, m_cursorRow + 1); break;
    case Qt::Key_Home: m_cursorStep = 0; break;
    case Qt::Key_End: m_cursorStep = m_steps - 1; break;
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
    case Qt::Key_Period: setStep(m_cursorRow, m_cursorStep, '.'); break;
    case Qt::Key_Space: {
        const char c = m_rows[m_cursorRow].steps[static_cast<size_t>(m_cursorStep)];
        setStep(m_cursorRow, m_cursorStep, c == '.' ? 'x' : '.');
        break;
    }
    default: {
        const QString text = event->text();
        if (text.size() == 1 && QStringLiteral("xXog").contains(text[0])) {
            setStep(m_cursorRow, m_cursorStep, text[0].toLatin1());
            m_cursorStep = std::min(m_steps - 1, m_cursorStep + 1);
        } else {
            StepGrid::keyPressEvent(event);
            return;
        }
    }
    }
    update();
}

// Returns the chosen GM drum note, or -1.
int DrumGrid::chooseDrum(const QPoint &globalPos, int current)
{
    QMenu menu(this);
    const QStringList names = drumNames();
    QMenu *more = nullptr;
    for (const QString &name : names) {
        const int note = drumNote(name);
        // The kit pieces first, percussion in a submenu
        QMenu *target = &menu;
        if (note >= 54 && note != 55 && note != 57 && note != 59) {
            if (!more)
                more = menu.addMenu(tr("Percussion"));
            target = more;
        }
        QAction *a = target->addAction(drumLabel(note));
        a->setData(note);
        a->setCheckable(true);
        a->setChecked(note == current);
    }
    menu.addSeparator();
    QAction *other = menu.addAction(tr("Other note…"));
    QAction *chosen = menu.exec(globalPos);
    if (!chosen)
        return -1;
    if (chosen == other) {
        bool ok = false;
        const int note = QInputDialog::getInt(this, tr("Drum Note"), tr("GM percussion note (1–127):"),
                                              current > 0 ? current : 36, 1, 127, 1, &ok);
        return ok ? note : -1;
    }
    return chosen->data().toInt();
}

void DrumGrid::addRowMenu(const QPoint &globalPos)
{
    const int note = chooseDrum(globalPos, -1);
    if (note <= 0)
        return;
    m_rows.push_back({note, std::string(static_cast<size_t>(m_steps), '.')});
    m_cursorRow = static_cast<int>(m_rows.size()) - 1;
    updateSize();
    update();
    emit changed();
}

void DrumGrid::rowMenu(int row, const QPoint &globalPos)
{
    QMenu menu(this);
    QAction *change = menu.addAction(tr("Change Instrument…"));
    QAction *up = menu.addAction(tr("Move Up"));
    QAction *down = menu.addAction(tr("Move Down"));
    up->setEnabled(row > 0);
    down->setEnabled(row + 1 < static_cast<int>(m_rows.size()));
    menu.addSeparator();
    QAction *clear = menu.addAction(tr("Clear Row"));
    QAction *remove = menu.addAction(tr("Remove Row"));
    QAction *chosen = menu.exec(globalPos);
    if (!chosen)
        return;
    if (chosen == change) {
        const int note = chooseDrum(globalPos, m_rows[row].note);
        if (note <= 0)
            return;
        m_rows[row].note = note;
    } else if (chosen == up) {
        std::swap(m_rows[row], m_rows[row - 1]);
    } else if (chosen == down) {
        std::swap(m_rows[row], m_rows[row + 1]);
    } else if (chosen == clear) {
        m_rows[row].steps.assign(static_cast<size_t>(m_steps), '.');
    } else if (chosen == remove) {
        m_rows.erase(m_rows.begin() + row);
        m_cursorRow = std::clamp(m_cursorRow, 0, std::max(0, static_cast<int>(m_rows.size()) - 1));
        updateSize();
    }
    update();
    emit changed();
}

// ---------------------------------------------------------------- StepTrack

StepTrack::StepTrack(const QString &name, std::vector<Token> tokens, QWidget *parent)
    : StepGrid(parent)
    , m_name(name)
    , m_tokens(std::move(tokens))
{
    updateSize();
}

void StepTrack::setPattern(const std::string &pattern)
{
    m_pattern = pattern;
    updateSize();
    update();
}

void StepTrack::updateSize()
{
    m_pattern.resize(static_cast<size_t>(m_steps), '.');
    m_cursor = std::clamp(m_cursor, 0, std::max(0, m_steps - 1));
    setFixedSize(stepX(m_steps) + 2, TrackHeight + 1);
}

int StepTrack::noteStart(int step) const
{
    for (int i = step; i >= 0; --i) {
        const char c = m_pattern[static_cast<size_t>(i)];
        if (c == '.')
            return -1;
        if (c != '-')
            return i;
    }
    return -1;
}

void StepTrack::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPalette &pal = palette();
    p.fillRect(rect(), pal.window());
    p.fillRect(0, 0, LabelWidth - 1, TrackHeight, pal.button());
    p.setPen(pal.buttonText().color());
    QFont font = p.font();
    font.setBold(true);
    p.setFont(font);
    p.drawText(QRect(6, 0, LabelWidth - 10, TrackHeight), Qt::AlignLeft | Qt::AlignVCenter, m_name);
    drawSteps(p, 0, TrackHeight);

    const QColor note = pal.highlight().color();
    for (int i = 0; i < m_steps; ++i) {
        const char c = m_pattern[static_cast<size_t>(i)];
        if (c == '-' && noteStart(i) < 0) {
            // A hold without a note before it: shown, but it plays nothing
            p.setPen(QPen(pal.placeholderText().color(), 2, Qt::DotLine));
            p.drawLine(stepX(i) + 3, TrackHeight / 2, stepX(i + 1) - 3, TrackHeight / 2);
            continue;
        }
        if (!isNote(c))
            continue;
        int end = i + 1;
        while (end < m_steps && m_pattern[static_cast<size_t>(end)] == '-')
            ++end;
        QColor color = note;
        if (c == 'c' || c == 'A')
            color = withAlpha(note, 140);
        const QRectF block(stepX(i) + 2, 4, stepX(end) - stepX(i) - 4, TrackHeight - 8);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(block, 4, 4);
        p.setPen(pal.highlightedText().color());
        p.drawText(block.adjusted(4, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, QString(QLatin1Char(c)));
    }
    font.setBold(false);
    p.setFont(font);
    drawCursor(p, m_cursor, 0, TrackHeight);
}

void StepTrack::set(int step, char value)
{
    if (step < 0 || step >= m_steps || m_pattern[static_cast<size_t>(step)] == value)
        return;
    m_pattern[static_cast<size_t>(step)] = value;
    update();
    emit changed();
}

// Removing a note also removes its holds.
void StepTrack::setRest(int step)
{
    if (step < 0 || step >= m_steps)
        return;
    const bool wasNote = isNote(m_pattern[static_cast<size_t>(step)]);
    m_pattern[static_cast<size_t>(step)] = '.';
    for (int i = step + 1; wasNote && i < m_steps && m_pattern[static_cast<size_t>(i)] == '-'; ++i)
        m_pattern[static_cast<size_t>(i)] = '.';
    update();
    emit changed();
}

void StepTrack::mousePressEvent(QMouseEvent *event)
{
    const int step = stepAt(event->position().x());
    if (step < 0)
        return;
    m_cursor = step;
    if (event->button() == Qt::RightButton) {
        update();
        tokenMenu(step, event->globalPosition().toPoint());
        return;
    }
    if (event->button() == Qt::LeftButton) {
        m_pressStep = step;
        m_anchor = noteStart(step);
        m_pressPattern = m_pattern;
    }
    update();
}

void StepTrack::mouseMoveEvent(QMouseEvent *event)
{
    if (m_anchor < 0 || m_pressStep < 0)
        return;
    int step = stepAt(event->position().x());
    if (step < 0)
        step = event->position().x() < LabelWidth ? 0 : m_steps - 1;
    if (step == m_pressStep && m_pattern == m_pressPattern)
        return;
    // The note now ends at `step`; steps that held it (or a note it now
    // covers) beyond the new end become rests.
    std::string p = m_pressPattern;
    const int end = std::max(step, m_anchor);
    for (int i = m_anchor + 1; i <= end; ++i)
        p[static_cast<size_t>(i)] = '-';
    for (int i = end + 1; i < m_steps && m_pressPattern[static_cast<size_t>(i)] == '-'; ++i)
        p[static_cast<size_t>(i)] = '.';
    m_cursor = end;
    if (p != m_pattern) {
        m_pattern = p;
        emit changed();
    }
    update();
}

void StepTrack::mouseReleaseEvent(QMouseEvent *)
{
    m_anchor = -1;
    m_pressStep = -1;
}

void StepTrack::mouseDoubleClickEvent(QMouseEvent *event)
{
    const int step = stepAt(event->position().x());
    if (step < 0 || event->button() != Qt::LeftButton)
        return;
    m_cursor = step;
    const int start = noteStart(step);
    if (start >= 0)
        setRest(start);
    else if (!m_tokens.empty())
        set(step, m_tokens.front().token);
}

void StepTrack::tokenMenu(int step, const QPoint &globalPos)
{
    QMenu menu(this);
    for (const Token &t : m_tokens) {
        QAction *a = menu.addAction(QStringLiteral("%1\t%2").arg(QLatin1Char(t.token)).arg(t.description));
        a->setData(QChar(QLatin1Char(t.token)));
    }
    menu.addSeparator();
    menu.addAction(tr("-\tHold the previous note"))->setData(QChar(QLatin1Char('-')));
    menu.addAction(tr(".\tRest"))->setData(QChar(QLatin1Char('.')));
    menu.addSeparator();
    QAction *clear = menu.addAction(tr("Clear Track"));
    QAction *chosen = menu.exec(globalPos);
    if (!chosen)
        return;
    if (chosen == clear) {
        m_pattern.assign(static_cast<size_t>(m_steps), '.');
        update();
        emit changed();
        return;
    }
    const char c = chosen->data().toChar().toLatin1();
    if (c == '.')
        setRest(step);
    else
        set(step, c);
}

char StepTrack::tokenForKey(const QString &text) const
{
    if (text.size() != 1)
        return 0;
    const char c = text[0].toLatin1();
    for (const Token &t : m_tokens)
        if (t.token == c)
            return c;
    // Bass tokens are upper case only, so 'r' and 'a' work as well.
    const char upper = text[0].toUpper().toLatin1();
    for (const Token &t : m_tokens)
        if (t.token == upper)
            return upper;
    return 0;
}

void StepTrack::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Left: m_cursor = std::max(0, m_cursor - 1); break;
    case Qt::Key_Right: m_cursor = std::min(m_steps - 1, m_cursor + 1); break;
    case Qt::Key_Home: m_cursor = 0; break;
    case Qt::Key_End: m_cursor = m_steps - 1; break;
    case Qt::Key_Delete: setRest(m_cursor); break;
    case Qt::Key_Backspace:
        m_cursor = std::max(0, m_cursor - 1);
        setRest(m_cursor);
        break;
    case Qt::Key_Period:
    case Qt::Key_Space:
        setRest(m_cursor);
        m_cursor = std::min(m_steps - 1, m_cursor + 1);
        break;
    case Qt::Key_Minus:
        set(m_cursor, '-');
        m_cursor = std::min(m_steps - 1, m_cursor + 1);
        break;
    default:
        if (const char c = tokenForKey(event->text())) {
            set(m_cursor, c);
            m_cursor = std::min(m_steps - 1, m_cursor + 1);
        } else {
            StepGrid::keyPressEvent(event);
            return;
        }
    }
    update();
}

// ---------------------------------------------------------------- StyleEditor

StyleEditor::StyleEditor(QWidget *parent)
    : QDialog(parent)
{
    setWindowFlag(Qt::Window);
    setModal(false);
    setSizeGripEnabled(true);
    auto *layout = new QVBoxLayout(this);

    // Style selection
    auto *top = new QHBoxLayout;
    top->addWidget(new QLabel(tr("Edit:")));
    m_styleCombo = new QComboBox;
    m_styleCombo->setPlaceholderText(tr("— new style —"));
    m_styleCombo->setToolTip(tr("Load a style into the editor. Apply it under a new name to make a copy."));
    connect(m_styleCombo, &QComboBox::activated, this, &StyleEditor::chooseStyle);
    top->addWidget(m_styleCombo, 1);
    auto *newButton = new QPushButton(tr("New"));
    connect(newButton, &QPushButton::clicked, this, &StyleEditor::newStyle);
    top->addWidget(newButton);
    auto *revertButton = new QPushButton(tr("Revert"));
    revertButton->setToolTip(tr("Discard the changes since the style was loaded or applied"));
    connect(revertButton, &QPushButton::clicked, this, &StyleEditor::revert);
    top->addWidget(revertButton);
    layout->addLayout(top);

    // General settings
    auto *general = new QGroupBox(tr("Style"));
    auto *grid = new QGridLayout(general);
    m_name = new QLineEdit;
    m_tempo = new QSpinBox;
    m_tempo->setRange(30, 320);
    m_tempo->setSuffix(tr(" BPM"));
    m_tempo->setToolTip(tr("Suggested tempo"));
    m_beatsSpin = new QSpinBox;
    m_beatsSpin->setRange(1, 16);
    m_stepsCombo = new QComboBox;
    const std::pair<int, const char *> grids[] = {{1, "quarters"}, {2, "eighths"},      {3, "triplets"},
                                                  {4, "sixteenths"}, {6, "sextuplets"}, {12, "12 per beat"}};
    for (const auto &[steps, name] : grids)
        m_stepsCombo->addItem(QStringLiteral("%1 (%2)").arg(steps).arg(tr(name)), steps);
    m_barsSpin = new QSpinBox;
    m_barsSpin->setRange(1, 16);
    m_swing = new QSpinBox;
    m_swing->setRange(0, 100);
    m_swing->setSuffix(QStringLiteral(" %"));
    m_swingUnit = new QComboBox;
    m_swingUnit->addItem(tr("8ths"), 8);
    m_swingUnit->addItem(tr("16ths"), 16);
    m_crash = new QComboBox;
    m_crash->setToolTip(tr("Cymbal played on the first beat of every phrase"));

    grid->addWidget(new QLabel(tr("Name:")), 0, 0);
    grid->addWidget(m_name, 0, 1, 1, 3);
    grid->addWidget(new QLabel(tr("Tempo:")), 0, 4);
    grid->addWidget(m_tempo, 0, 5);
    grid->addWidget(new QLabel(tr("Beats per bar:")), 1, 0);
    grid->addWidget(m_beatsSpin, 1, 1);
    grid->addWidget(new QLabel(tr("Steps per beat:")), 1, 2);
    grid->addWidget(m_stepsCombo, 1, 3);
    grid->addWidget(new QLabel(tr("Bars:")), 1, 4);
    grid->addWidget(m_barsSpin, 1, 5);
    grid->addWidget(new QLabel(tr("Swing:")), 2, 0);
    auto *swingRow = new QHBoxLayout;
    swingRow->addWidget(m_swing);
    swingRow->addWidget(m_swingUnit);
    grid->addLayout(swingRow, 2, 1);
    grid->addWidget(new QLabel(tr("Crash:")), 2, 2);
    grid->addWidget(m_crash, 2, 3);
    grid->setColumnStretch(3, 1);
    layout->addWidget(general);

    // Instruments
    auto *instruments = new QGroupBox(tr("Instruments"));
    auto *ig = new QGridLayout(instruments);
    m_compProgram = programCombo();
    m_compVelocity = new QSpinBox;
    m_compVelocity->setRange(1, 127);
    m_compVelocity->setPrefix(tr("velocity "));
    m_voicing = new QComboBox;
    m_voicing->addItem(tr("Close chords"));
    m_voicing->addItem(tr("Power chords"));
    m_voicing->setToolTip(tr("Power chords: root, fifth and octave in guitar range"));
    m_bassProgram = programCombo();
    m_bassVelocity = new QSpinBox;
    m_bassVelocity->setRange(1, 127);
    m_bassVelocity->setPrefix(tr("velocity "));
    m_padProgram = programCombo();
    m_padDefault = new QCheckBox(tr("On by default"));
    ig->addWidget(new QLabel(tr("Comp:")), 0, 0);
    ig->addWidget(m_compProgram, 0, 1);
    ig->addWidget(m_compVelocity, 0, 2);
    ig->addWidget(m_voicing, 0, 3);
    ig->addWidget(new QLabel(tr("Bass:")), 1, 0);
    ig->addWidget(m_bassProgram, 1, 1);
    ig->addWidget(m_bassVelocity, 1, 2);
    ig->addWidget(new QLabel(tr("Pad:")), 2, 0);
    ig->addWidget(m_padProgram, 2, 1);
    ig->addWidget(m_padDefault, 2, 2);
    ig->setColumnStretch(1, 1);
    layout->addWidget(instruments);

    // Patterns
    auto *tabs = new QTabWidget;
    auto scrolled = [](QWidget *content) {
        auto *scroll = new QScrollArea;
        scroll->setWidget(content);
        scroll->setWidgetResizable(true);
        return scroll;
    };

    auto *groove = new QWidget;
    auto *gl = new QVBoxLayout(groove);
    m_drums = new DrumGrid;
    m_bass = new StepTrack(tr("Bass"), {{'R', tr("Root (or the bass note of a slash chord)")},
                                        {'3', tr("Third")},
                                        {'5', tr("Fifth")},
                                        {'6', tr("Sixth")},
                                        {'7', tr("Seventh")},
                                        {'8', tr("Octave")},
                                        {'A', tr("Chromatic approach to the next chord")}});
    m_comp = new StepTrack(tr("Comp"), {{'C', tr("Full chord, accented")},
                                        {'c', tr("Full chord, soft")},
                                        {'1', tr("Chord tone 1 (lowest)")},
                                        {'2', tr("Chord tone 2")},
                                        {'3', tr("Chord tone 3")},
                                        {'4', tr("Chord tone 4")},
                                        {'5', tr("Chord tone 5")},
                                        {'6', tr("Chord tone 6")},
                                        {'7', tr("Chord tone 7")},
                                        {'8', tr("Chord tone 8")},
                                        {'9', tr("Chord tone 9")}});
    gl->addWidget(m_drums);
    gl->addSpacing(8);
    gl->addWidget(m_bass);
    gl->addWidget(m_comp);
    gl->addStretch();
    tabs->addTab(scrolled(groove), tr("Groove"));

    auto *fillPage = new QWidget;
    auto *fl = new QVBoxLayout(fillPage);
    m_fillEnabled = new QCheckBox(tr("Play a fill on the last bar of every phrase"));
    m_fill = new DrumGrid;
    fl->addWidget(m_fillEnabled);
    fl->addWidget(m_fill);
    fl->addStretch();
    tabs->addTab(scrolled(fillPage), tr("Fill"));
    layout->addWidget(tabs, 1);

    auto *help = new QLabel(
        tr("<b>Drums:</b> click to set or clear a step, shift-click for an accent, right-click to change the "
           "velocity (accent, normal, soft, ghost), drag to paint. Click an instrument name to change it.<br>"
           "<b>Tracks:</b> click a step and type a token (bass: R 3 5 6 7 8 A, comp: C c 1–9), <i>-</i> holds, "
           "<i>.</i> or Del rests. Drag a note to change its length, double-click to add or remove a note, "
           "right-click for all tokens."));
    help->setWordWrap(true);
    help->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(help);

    auto *buttons = new QDialogButtonBox;
    m_testButton = buttons->addButton(tr("Test"), QDialogButtonBox::ActionRole);
    m_testButton->setCheckable(true);
    m_testButton->setIcon(QWidget::style()->standardIcon(QStyle::SP_MediaPlay));
    m_testButton->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    m_testButton->setToolTip(tr("Play the edited style with the band, with its tempo, swing and instruments, "
                                "without changing the style list. Changes are heard at the next bar. (Ctrl+T)"));
    m_testTimer = new QTimer(this);
    m_testTimer->setSingleShot(true);
    m_testTimer->setInterval(150);
    connect(m_testTimer, &QTimer::timeout, this, [this] {
        if (isTesting())
            emit testStyle(style());
    });
    connect(m_testButton, &QPushButton::toggled, this, [this](bool on) {
        m_testButton->setIcon(QWidget::style()->standardIcon(on ? QStyle::SP_MediaStop : QStyle::SP_MediaPlay));
        m_testTimer->stop();
        if (on)
            emit testStyle(style());
        else
            emit testStopped();
    });
    QPushButton *applyButton = buttons->addButton(tr("Apply"), QDialogButtonBox::ApplyRole);
    applyButton->setToolTip(tr("Use the style in the band (Ctrl+Return)"));
    applyButton->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));
    QPushButton *saveButton = buttons->addButton(tr("Save to File…"), QDialogButtonBox::ActionRole);
    saveButton->setToolTip(tr("Apply the style and save it in a style file, which is loaded again on the next start"));
    buttons->addButton(QDialogButtonBox::Close);
    connect(applyButton, &QPushButton::clicked, this, &StyleEditor::apply);
    connect(saveButton, &QPushButton::clicked, this, &StyleEditor::saveToFile);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::hide);
    layout->addWidget(buttons);

    // Change tracking
    auto modified = [this] {
        if (!m_loading)
            setModified(true);
    };
    connect(m_name, &QLineEdit::textEdited, this, modified);
    for (QSpinBox *spin : {m_tempo, m_swing, m_compVelocity, m_bassVelocity})
        connect(spin, &QSpinBox::valueChanged, this, modified);
    for (QComboBox *combo : {m_swingUnit, m_crash, m_compProgram, m_voicing, m_bassProgram, m_padProgram})
        connect(combo, &QComboBox::currentIndexChanged, this, modified);
    connect(m_padDefault, &QCheckBox::toggled, this, modified);
    for (StepGrid *g : std::initializer_list<StepGrid *>{m_drums, m_fill, m_bass, m_comp})
        connect(g, &StepGrid::changed, this, modified);
    connect(m_beatsSpin, &QSpinBox::valueChanged, this, &StyleEditor::onMeterChanged);
    connect(m_barsSpin, &QSpinBox::valueChanged, this, &StyleEditor::onMeterChanged);
    connect(m_stepsCombo, &QComboBox::currentIndexChanged, this, &StyleEditor::onMeterChanged);
    connect(m_fillEnabled, &QCheckBox::toggled, this, &StyleEditor::onFillToggled);

    load(defaultStyle(tr("New Style")));
    m_originalName.clear();
    resize(900, 760);
}

void StyleEditor::setStyles(const std::vector<mc::Style> &styles)
{
    m_styles = styles;
    QSignalBlocker block(m_styleCombo);
    m_styleCombo->clear();
    for (const auto &s : m_styles)
        m_styleCombo->addItem(QString::fromStdString(s.name));
    m_styleCombo->setCurrentIndex(m_originalName.isEmpty() ? -1 : m_styleCombo->findText(m_originalName));
}

void StyleEditor::editStyle(const QString &name)
{
    if (m_modified)
        return;
    for (const auto &s : m_styles) {
        if (QString::fromStdString(s.name) == name) {
            load(s);
            return;
        }
    }
}

void StyleEditor::load(const mc::Style &style)
{
    m_loading = true;
    const mc::Style s = mc::normalizedStyle(style);
    m_originalName = QString::fromStdString(s.name);
    m_name->setText(m_originalName);
    m_tempo->setValue(s.tempo);
    m_beats = s.beatsPerBar;
    m_stepsPerBeat = s.stepsPerBeat;
    m_bars = s.bars;
    m_beatsSpin->setValue(s.beatsPerBar);
    m_stepsCombo->setCurrentIndex(std::max(0, m_stepsCombo->findData(s.stepsPerBeat)));
    m_barsSpin->setValue(s.bars);
    m_swing->setValue(qRound(s.swing * 100));
    m_swingUnit->setCurrentIndex(s.swingUnit == 16 ? 1 : 0);

    m_crash->clear();
    m_crash->addItem(tr("none"), 0);
    for (const QString &name : drumNames()) {
        const int note = drumNote(name);
        if (name.contains(QLatin1String("crash")) || name.contains(QLatin1String("ride"))
            || name == QLatin1String("splash") || name == QLatin1String("china"))
            m_crash->addItem(drumLabel(note), note);
    }
    if (m_crash->findData(s.crashNote) < 0)
        m_crash->addItem(drumLabel(s.crashNote), s.crashNote);
    m_crash->setCurrentIndex(m_crash->findData(s.crashNote));

    m_compProgram->setCurrentIndex(s.pianoProgram);
    m_compVelocity->setValue(s.compVelocity);
    m_voicing->setCurrentIndex(s.powerChords ? 1 : 0);
    m_bassProgram->setCurrentIndex(s.bassProgram);
    m_bassVelocity->setValue(s.bassVelocity);
    m_padProgram->setCurrentIndex(s.padProgram);
    m_padDefault->setChecked(s.padDefault);

    for (StepGrid *g : std::initializer_list<StepGrid *>{m_drums, m_bass, m_comp})
        g->setMeter(s.beatsPerBar, s.stepsPerBeat, s.stepCount());
    m_fill->setMeter(s.beatsPerBar, s.stepsPerBeat, s.stepsPerBar());
    m_drums->setRows(s.drums);
    m_fill->setRows(s.fill);
    m_fillEnabled->setChecked(!s.fill.empty());
    m_fill->setEnabled(!s.fill.empty());
    m_bass->setPattern(s.bass);
    m_comp->setPattern(s.comp);

    {
        QSignalBlocker block(m_styleCombo);
        m_styleCombo->setCurrentIndex(m_styleCombo->findText(m_originalName));
    }
    m_loading = false;
    setModified(false);
}

mc::Style StyleEditor::style() const
{
    mc::Style s;
    s.name = m_name->text().trimmed().toStdString();
    s.beatsPerBar = m_beats;
    s.stepsPerBeat = m_stepsPerBeat;
    s.bars = m_bars;
    s.tempo = m_tempo->value();
    s.swing = m_swing->value() / 100.0;
    s.swingUnit = m_swingUnit->currentData().toInt();
    s.crashNote = m_crash->currentData().toInt();
    s.drums = m_drums->rows();
    if (m_fillEnabled->isChecked())
        s.fill = m_fill->rows();
    s.bass = m_bass->pattern();
    s.comp = m_comp->pattern();
    s.powerChords = m_voicing->currentIndex() == 1;
    s.pianoProgram = m_compProgram->currentIndex();
    s.bassProgram = m_bassProgram->currentIndex();
    s.padProgram = m_padProgram->currentIndex();
    s.padDefault = m_padDefault->isChecked();
    s.bassVelocity = m_bassVelocity->value();
    s.compVelocity = m_compVelocity->value();
    return s;
}

void StyleEditor::onMeterChanged()
{
    if (m_loading)
        return;
    mc::Style current = style();
    current.fill = m_fill->rows(); // also while the fill is switched off
    const mc::Style resized = mc::resizedStyle(current, m_beatsSpin->value(), m_stepsCombo->currentData().toInt(),
                                               m_barsSpin->value());
    m_beats = resized.beatsPerBar;
    m_stepsPerBeat = resized.stepsPerBeat;
    m_bars = resized.bars;
    for (StepGrid *g : std::initializer_list<StepGrid *>{m_drums, m_bass, m_comp})
        g->setMeter(m_beats, m_stepsPerBeat, resized.stepCount());
    m_fill->setMeter(m_beats, m_stepsPerBeat, resized.stepsPerBar());
    m_drums->setRows(resized.drums);
    m_fill->setRows(resized.fill);
    m_bass->setPattern(resized.bass);
    m_comp->setPattern(resized.comp);
    setModified(true);
}

void StyleEditor::onFillToggled(bool on)
{
    m_fill->setEnabled(on);
    if (m_loading)
        return;
    if (on && m_fill->rows().empty()) {
        // Start from the first bar of the groove
        std::vector<mc::DrumRow> rows = m_drums->rows();
        for (auto &r : rows)
            r.steps.resize(static_cast<size_t>(m_beats * m_stepsPerBeat));
        m_fill->setRows(rows);
    }
    setModified(true);
}

void StyleEditor::setModified(bool modified)
{
    m_modified = modified;
    updateTitle();
    if (isTesting())
        m_testTimer->start();
}

bool StyleEditor::isTesting() const
{
    return m_testButton->isChecked();
}

void StyleEditor::setTesting(bool testing)
{
    QSignalBlocker block(m_testButton);
    m_testButton->setChecked(testing);
    m_testButton->setIcon(QWidget::style()->standardIcon(testing ? QStyle::SP_MediaStop : QStyle::SP_MediaPlay));
    m_testTimer->stop();
}

void StyleEditor::hideEvent(QHideEvent *event)
{
    m_testButton->setChecked(false); // closing the editor ends the test
    QDialog::hideEvent(event);
}

void StyleEditor::updateTitle()
{
    const QString name = m_name->text().trimmed();
    setWindowTitle(tr("Style Editor – %1%2").arg(name.isEmpty() ? tr("unnamed") : name, m_modified ? "*" : ""));
}

bool StyleEditor::confirmDiscard()
{
    if (!m_modified)
        return true;
    return QMessageBox::question(this, tr("Style Editor"),
                                 tr("Discard the changes to '%1'?").arg(m_name->text().trimmed()),
                                 QMessageBox::Discard | QMessageBox::Cancel)
           == QMessageBox::Discard;
}

void StyleEditor::chooseStyle(int index)
{
    if (index < 0 || index >= static_cast<int>(m_styles.size()))
        return;
    if (!confirmDiscard()) {
        QSignalBlocker block(m_styleCombo);
        m_styleCombo->setCurrentIndex(m_originalName.isEmpty() ? -1 : m_styleCombo->findText(m_originalName));
        return;
    }
    load(m_styles[index]);
}

void StyleEditor::newStyle()
{
    if (!confirmDiscard())
        return;
    QString name = tr("New Style");
    for (int n = 2; m_styleCombo->findText(name) >= 0; ++n)
        name = tr("New Style %1").arg(n);
    load(defaultStyle(name));
    m_originalName.clear();
    m_styleCombo->setCurrentIndex(-1);
    setModified(true);
}

void StyleEditor::revert()
{
    const int index = m_styleCombo->findText(m_originalName);
    if (m_originalName.isEmpty() || index < 0) {
        newStyle();
        return;
    }
    if (confirmDiscard())
        load(m_styles[index]);
}

bool StyleEditor::validated(mc::Style &s)
{
    s = style();
    if (s.name.empty()) {
        QMessageBox::warning(this, tr("Style Editor"), tr("Please give the style a name."));
        m_name->setFocus();
        return false;
    }
    const QString name = QString::fromStdString(s.name);
    if (name != m_originalName && m_styleCombo->findText(name) >= 0
        && QMessageBox::question(this, tr("Style Editor"), tr("A style named '%1' exists. Replace it?").arg(name))
               != QMessageBox::Yes)
        return false;
    std::string error;
    if (!mc::validateStyle(s, error)) {
        QMessageBox::warning(this, tr("Style Editor"), QString::fromStdString(error));
        return false;
    }
    return true;
}

bool StyleEditor::apply()
{
    mc::Style s;
    if (!validated(s))
        return false;
    m_originalName = QString::fromStdString(s.name);
    emit applyRequested(s); // the main window calls setStyles(), which selects the style in the list
    setModified(false);
    return true;
}

void StyleEditor::saveToFile()
{
    mc::Style s;
    if (!validated(s))
        return;

    QSettings settings;
    QString dir = settings.value("styleDir").toString();
    if (dir.isEmpty())
        dir = QDir::homePath();
    QString base = QString::fromStdString(s.name).toLower();
    base.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Style"), QDir(dir).filePath(base + ".xml"),
                                                      tr("MidiCombo styles (*.xml)"), nullptr,
                                                      QFileDialog::DontConfirmOverwrite);
    if (path.isEmpty())
        return;
    settings.setValue("styleDir", QFileInfo(path).absolutePath());

    // An existing style file keeps its other styles.
    std::vector<mc::Style> styles;
    if (QFileInfo::exists(path)) {
        QString error;
        if (!readStyleFile(path, styles, error)) {
            if (QMessageBox::question(this, tr("Save Style"),
                                      tr("%1 cannot be read as a style file:\n%2\n\nOverwrite it?").arg(path, error))
                != QMessageBox::Yes)
                return;
            styles.clear();
        }
    }
    auto same = std::find_if(styles.begin(), styles.end(), [&](const mc::Style &o) { return o.name == s.name; });
    if (same != styles.end()) {
        if (styles.size() > 1
            && QMessageBox::question(this, tr("Save Style"),
                                     tr("%1 already contains a style named '%2'. Replace it?")
                                         .arg(QFileInfo(path).fileName(), QString::fromStdString(s.name)))
                   != QMessageBox::Yes)
            return;
        *same = s;
    } else {
        if (!styles.empty()
            && QMessageBox::question(this, tr("Save Style"),
                                     tr("Add '%1' to the %n style(s) in %2?", nullptr, static_cast<int>(styles.size()))
                                         .arg(QString::fromStdString(s.name), QFileInfo(path).fileName()))
                   != QMessageBox::Yes)
            return;
        styles.push_back(s);
    }

    QString error;
    if (!writeStyleFile(path, styles, error)) {
        QMessageBox::warning(this, tr("Save Style"), tr("Could not save %1:\n%2").arg(path, error));
        return;
    }
    m_originalName = QString::fromStdString(s.name);
    setModified(false);
    emit savedToFile(path, m_originalName);
}
