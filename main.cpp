// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dbusfaceservice.h"
#include "definehead.h"

#include <DLog>
#include <QCoreApplication>

DCORE_USE_NAMESPACE
static QString logPath = "/var/log/deepin-face.log";
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    DLogManager::setlogFilePath(logPath);
    DLogManager::registerConsoleAppender();
    DLogManager::registerFileAppender();

    DbusFaceService w;
    QDBusConnection connection = QDBusConnection::systemBus();
      if ( !connection.registerObject(SERVERPATH,
                                      &w,
                                      QDBusConnection::ExportAllSlots
                                          | QDBusConnection::ExportScriptableProperties
                                          | QDBusConnection::ExportAllSignals)) {
        qWarning() << "dbus object registere error!";
        return -1;
    }
 
    auto reply = connection.registerService(SERVERNAME);
    if (reply != QDBusConnectionInterface::ServiceRegistered)
    {
        qWarning() << "dbus service registere error!";
        /* code */
        return -1;
    }
  
    return a.exec();
}
