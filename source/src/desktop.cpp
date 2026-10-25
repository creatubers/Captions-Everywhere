#include "desktop.h"
#include <QStandardPaths>
QProcessEnvironment desktopEnvironment() {
    auto env=QProcessEnvironment::systemEnvironment();
    // A host file manager must never load this bundle's Qt plugins or font config.
    for(const char *name:{"QT_PLUGIN_PATH","QT_QPA_PLATFORM_PLUGIN_PATH","QT_QPA_FONTDIR",
        "QT_QPA_PLATFORM","QT_QUICK_BACKEND","QT_OPENGL","FONTCONFIG_FILE","FONTCONFIG_PATH",
        "XKB_CONFIG_ROOT","LD_LIBRARY_PATH","LD_PRELOAD","APPIMAGE","APPDIR","OWD",
        "LC_PORTABLE_ROOT","CAPTIONS_EVERYWHERE_STATE_DIR","CAPTIONS_EVERYWHERE_SYSTEM_LOCALE"})env.remove(QString::fromLatin1(name));
    return env;
}
QProcess *openDesktopFolder(const QString &path,QObject *parent) {
    auto process=new QProcess(parent);
    process->setProcessEnvironment(desktopEnvironment());
    const auto executable=QStandardPaths::findExecutable("xdg-open");
    process->setProgram(executable.isEmpty()?QString("xdg-open"):executable);
    process->setArguments({path});
    return process; // Caller connects error/completion before start().
}
