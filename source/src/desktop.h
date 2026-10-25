#pragma once
#include <QProcess>
#include <QProcessEnvironment>
QProcessEnvironment desktopEnvironment();
QProcess *openDesktopFolder(const QString &path,QObject *parent);
