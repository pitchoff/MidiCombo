#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include <alsa/asoundlib.h>
#include <mutex>

class QSocketNotifier;

struct MidiPort {
    int client = -1;
    int port = -1;
    QString clientName;
    QString portName;

    QString id() const { return QStringLiteral("%1:%2").arg(client).arg(port); }
    QString label() const { return clientName + QStringLiteral(": ") + portName; }
};

// ALSA sequencer client with one output port ("Band Out") and one input
// port ("Chord In"). Output calls are thread safe so the band thread and
// the GUI thread (MIDI thru) can share the client.
class MidiEngine : public QObject {
    Q_OBJECT
public:
    explicit MidiEngine(QObject *parent = nullptr);
    ~MidiEngine() override;

    bool open(const QString &clientName);
    QString lastError() const { return m_error; }

    void noteOn(int channel, int note, int velocity);
    void noteOff(int channel, int note);
    void controlChange(int channel, int controller, int value);
    void programChange(int channel, int program);
    void allNotesOff();

    QList<MidiPort> outputDestinations() const;
    QList<MidiPort> inputSources() const;
    bool connectOutput(int client, int port);
    void disconnectOutput(int client, int port);
    bool connectInput(int client, int port);
    void disconnectInput(int client, int port);

signals:
    void noteOnReceived(int channel, int note, int velocity);
    void noteOffReceived(int channel, int note);
    void controlReceived(int channel, int controller, int value);

private:
    void send(snd_seq_event_t &ev);
    void readInput();
    QList<MidiPort> listPorts(unsigned int requiredCaps) const;

    snd_seq_t *m_seq = nullptr;
    int m_clientId = -1;
    int m_outPort = -1;
    int m_inPort = -1;
    mutable std::mutex m_mutex;
    QList<QSocketNotifier *> m_notifiers;
    QString m_error;
};
