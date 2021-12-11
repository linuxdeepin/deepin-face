#ifndef DBUSFACESERVICE_H
#define DBUSFACESERVICE_H

#include "drivermanger.h"
#include "definehead.h"

#include <QtDBus/QtDBus>
#include <QDebug>
#include <QSharedPointer>


class DbusFaceService:public QObject,public QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", SERVERINTERFACE)
    Q_PROPERTY(bool Claim READ getClaim)
    Q_PROPERTY(QStringList List READ getList)
    Q_PROPERTY(qint32 CharaType READ getCharaType)

Q_SIGNALS:
    // Standard Notifications dbus implementation
    void ErollStatus(QString, qint32, QString);
    void VerifyStatus(QString, qint32, QString);

public:
    DbusFaceService(QObject *parent = nullptr);
    void charaTypeChange();
    bool getClaim() const;
    QStringList getList() const;
    qint32 getCharaType() const;

public Q_SLOTS:
     QDBusUnixFileDescriptor EnrollStart(QString chara, qint32 charaType, QString actionId);
     void EnrollStop(QString actionId);
     QDBusUnixFileDescriptor VerifyStart(QStringList charas, QString actionId);
     void VerifyStop(QString actionId);
     void Delete(QString chara);

protected Q_SLOTS:
     void exitApp();
private:
     QSharedPointer<DriverManger>   m_spDriverManger;
     QTimer*                        m_spTimer;
};

#endif // DBUSFACESERVICE_H
