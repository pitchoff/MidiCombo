#include "MidiEngine.h"

#include <QSocketNotifier>

#include <vector>

MidiEngine::MidiEngine(QObject *parent)
    : QObject(parent)
{
}

MidiEngine::~MidiEngine()
{
    qDeleteAll(m_notifiers);
    if (m_seq) {
        allNotesOff();
        snd_seq_close(m_seq);
    }
}

bool MidiEngine::open(const QString &clientName)
{
    int err = snd_seq_open(&m_seq, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK);
    if (err < 0) {
        m_error = QString::fromLocal8Bit(snd_strerror(err));
        m_seq = nullptr;
        return false;
    }
    snd_seq_set_client_name(m_seq, clientName.toUtf8().constData());
    m_clientId = snd_seq_client_id(m_seq);

    m_outPort = snd_seq_create_simple_port(m_seq, "Band Out",
                                           SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
                                           SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    m_inPort = snd_seq_create_simple_port(m_seq, "Chord In",
                                          SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
                                          SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    if (m_outPort < 0 || m_inPort < 0) {
        m_error = QString::fromLocal8Bit(snd_strerror(m_outPort < 0 ? m_outPort : m_inPort));
        return false;
    }

    const int count = snd_seq_poll_descriptors_count(m_seq, POLLIN);
    std::vector<pollfd> fds(count);
    snd_seq_poll_descriptors(m_seq, fds.data(), count, POLLIN);
    for (const pollfd &fd : fds) {
        auto *notifier = new QSocketNotifier(fd.fd, QSocketNotifier::Read);
        connect(notifier, &QSocketNotifier::activated, this, &MidiEngine::readInput);
        m_notifiers.append(notifier);
    }
    return true;
}

void MidiEngine::send(snd_seq_event_t &ev)
{
    if (!m_seq)
        return;
    snd_seq_ev_set_source(&ev, m_outPort);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    std::lock_guard<std::mutex> lock(m_mutex);
    snd_seq_event_output_direct(m_seq, &ev);
}

void MidiEngine::noteOn(int channel, int note, int velocity)
{
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_noteon(&ev, channel, note, velocity);
    send(ev);
}

void MidiEngine::noteOff(int channel, int note)
{
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_noteoff(&ev, channel, note, 0);
    send(ev);
}

void MidiEngine::controlChange(int channel, int controller, int value)
{
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_controller(&ev, channel, controller, value);
    send(ev);
}

void MidiEngine::programChange(int channel, int program)
{
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_pgmchange(&ev, channel, program);
    send(ev);
}

void MidiEngine::allNotesOff()
{
    for (int ch = 0; ch < 16; ++ch) {
        controlChange(ch, 64, 0);  // sustain off
        controlChange(ch, 123, 0); // all notes off
    }
}

void MidiEngine::readInput()
{
    std::vector<snd_seq_event_t> events;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        snd_seq_event_t *ev = nullptr;
        for (;;) {
            const int r = snd_seq_event_input(m_seq, &ev);
            if (r == -ENOSPC)
                continue; // input overrun, events were dropped
            if (r < 0 || !ev)
                break;
            events.push_back(*ev);
        }
    }

    for (const auto &ev : events) {
        switch (ev.type) {
        case SND_SEQ_EVENT_NOTEON:
            if (ev.data.note.velocity > 0)
                emit noteOnReceived(ev.data.note.channel, ev.data.note.note, ev.data.note.velocity);
            else
                emit noteOffReceived(ev.data.note.channel, ev.data.note.note);
            break;
        case SND_SEQ_EVENT_NOTEOFF:
            emit noteOffReceived(ev.data.note.channel, ev.data.note.note);
            break;
        case SND_SEQ_EVENT_CONTROLLER:
            emit controlReceived(ev.data.control.channel, ev.data.control.param, ev.data.control.value);
            break;
        default:
            break;
        }
    }
}

QList<MidiPort> MidiEngine::listPorts(unsigned int requiredCaps) const
{
    QList<MidiPort> result;
    if (!m_seq)
        return result;

    std::lock_guard<std::mutex> lock(m_mutex);
    snd_seq_client_info_t *cinfo;
    snd_seq_port_info_t *pinfo;
    snd_seq_client_info_alloca(&cinfo);
    snd_seq_port_info_alloca(&pinfo);

    snd_seq_client_info_set_client(cinfo, -1);
    while (snd_seq_query_next_client(m_seq, cinfo) >= 0) {
        const int client = snd_seq_client_info_get_client(cinfo);
        if (client == SND_SEQ_CLIENT_SYSTEM || client == m_clientId)
            continue;
        snd_seq_port_info_set_client(pinfo, client);
        snd_seq_port_info_set_port(pinfo, -1);
        while (snd_seq_query_next_port(m_seq, pinfo) >= 0) {
            const unsigned int caps = snd_seq_port_info_get_capability(pinfo);
            if ((caps & requiredCaps) != requiredCaps || (caps & SND_SEQ_PORT_CAP_NO_EXPORT))
                continue;
            MidiPort p;
            p.client = client;
            p.port = snd_seq_port_info_get_port(pinfo);
            p.clientName = QString::fromUtf8(snd_seq_client_info_get_name(cinfo));
            p.portName = QString::fromUtf8(snd_seq_port_info_get_name(pinfo)).trimmed();
            result.append(p);
        }
    }
    return result;
}

QList<MidiPort> MidiEngine::outputDestinations() const
{
    return listPorts(SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE);
}

QList<MidiPort> MidiEngine::inputSources() const
{
    return listPorts(SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ);
}

bool MidiEngine::connectOutput(int client, int port)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_seq && snd_seq_connect_to(m_seq, m_outPort, client, port) >= 0;
}

void MidiEngine::disconnectOutput(int client, int port)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_seq)
        snd_seq_disconnect_to(m_seq, m_outPort, client, port);
}

bool MidiEngine::connectInput(int client, int port)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_seq && snd_seq_connect_from(m_seq, m_inPort, client, port) >= 0;
}

void MidiEngine::disconnectInput(int client, int port)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_seq)
        snd_seq_disconnect_from(m_seq, m_inPort, client, port);
}
