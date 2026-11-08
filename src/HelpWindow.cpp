#include "HelpWindow.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QTextBrowser>
#include <QVBoxLayout>

void showHelpWindow(QWidget *parent, const QString &id, const QString &title, const QString &html)
{
    auto *dialog = parent->findChild<QDialog *>(id, Qt::FindDirectChildrenOnly);
    if (!dialog) {
        dialog = new QDialog(parent);
        dialog->setObjectName(id);
        dialog->setWindowFlag(Qt::Window);
        dialog->setWindowTitle(title);
        auto *layout = new QVBoxLayout(dialog);
        auto *text = new QTextBrowser;
        text->setOpenLinks(true);
        text->setHtml(html);
        layout->addWidget(text);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
        QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::hide);
        layout->addWidget(buttons);
        dialog->resize(720, 760);
    }
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}
