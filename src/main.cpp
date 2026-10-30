#include "MainWindow.h"
#include "MidiEngine.h"

#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include <QStandardPaths>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("MidiCombo"));
    QApplication::setApplicationName(QStringLiteral("MidiCombo"));
    // Lets Wayland match the window to the installed desktop entry and icon.
    // Without an installed entry the desktop portal rejects the registration
    // and Qt warns, e.g. when running from the build tree.
    if (!QStandardPaths::locate(QStandardPaths::ApplicationsLocation, QStringLiteral("midicombo.desktop")).isEmpty())
        QApplication::setDesktopFileName(QStringLiteral("midicombo"));

    QIcon icon;
    for (int size : {16, 24, 32, 48, 64, 128, 256})
        icon.addFile(QStringLiteral(":/icons/midicombo-%1.png").arg(size), QSize(size, size));
    QApplication::setWindowIcon(icon);

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
