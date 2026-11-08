#pragma once

#include <QString>

class QWidget;

// Shows HTML help in a separate, non-modal window. There is one window per
// parent and `id`; calling again raises it.
void showHelpWindow(QWidget *parent, const QString &id, const QString &title, const QString &html);
