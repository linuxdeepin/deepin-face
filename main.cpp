
#include "dbusfaceservice.h"
#include "definehead.h"

#include <QCoreApplication>
#include <DLog>

DCORE_USE_NAMESPACE
static QString logPath="/var/log/deepin-face.log";
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    DLogManager::setlogFilePath(logPath);
    DLogManager::registerConsoleAppender();
    DLogManager::registerFileAppender();

    DbusFaceService w;
    QDBusConnection connection = QDBusConnection::systemBus();
    if (!connection.registerService(SERVERNAME) ||
        !connection.registerObject(SERVERPATH,&w,QDBusConnection::ExportAllSlots|QDBusConnection::ExportScriptableProperties|
                                   QDBusConnection::ExportAllSignals)) {
        qDebug() << "dbus service already registered!";
        return -1;
    }
    return a.exec();
}
