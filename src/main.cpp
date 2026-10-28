#include "MainWindow.h"
#include "MidiEngine.h"

#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("MidiCombo"));
    QApplication::setApplicationName(QStringLiteral("MidiCombo"));

    MidiEngine midi;
    if (!midi.open(QStringLiteral("MidiCombo"))) {
        QMessageBox::critical(nullptr, QStringLiteral("MidiCombo"),
                              QStringLiteral("Could not open the ALSA sequencer: %1").arg(midi.lastError()));
        return 1;
    }

    MainWindow window(&midi);
    window.show();
    return app.exec();
}
