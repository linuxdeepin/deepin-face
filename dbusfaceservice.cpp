#include "dbusfaceservice.h"
#include <iostream>

DbusFaceService::DbusFaceService(QObject *parent)
    : QObject(parent)
    , m_spDriverManger(new DriverManger())
    , m_spTimer(new QTimer)
{
    this->m_spDriverManger->init();
    connect(m_spTimer, &QTimer::timeout, this, &DbusFaceService::exitApp);
    m_spTimer->start(15 * 1000);
}

QDBusUnixFileDescriptor DbusFaceService::EnrollStart(QString chara,
                                                     qint32 charaType,
                                                     QString actionId)
{
    qDebug() << "EnrollStart chara" << chara << " charaType" << charaType << " actionId"
             << actionId;

    m_spTimer->stop();

    ErrMsgInfo errMSsgInfo;
    auto fileDescriptor = m_spDriverManger->enrollStart(chara, charaType, actionId, errMSsgInfo);
    if (!errMSsgInfo.msg.isEmpty()) {
        sendErrorReply(errMSsgInfo.errType, errMSsgInfo.msg);
        return QDBusUnixFileDescriptor(-1);
    }

    return fileDescriptor;
}

void DbusFaceService::EnrollStop(QString actionId)
{
    qDebug() << "EnrollStop action id: " << actionId;

    m_spTimer->start();

    ErrMsgInfo errMSsgInfo;
    m_spDriverManger->enrollStop(actionId, errMSsgInfo);

    if (!errMSsgInfo.msg.isEmpty()) {
        sendErrorReply(errMSsgInfo.errType, errMSsgInfo.msg);
    }
}

QDBusUnixFileDescriptor DbusFaceService::VerifyStart(QStringList charas, QString actionId)
{
    qDebug() << "VerifyStart chara" << charas << " actionId" << actionId;

    m_spTimer->stop();

    ErrMsgInfo errMSsgInfo;
    auto fileDescriptor = m_spDriverManger->verifyStart(charas, actionId, errMSsgInfo);
    if (!errMSsgInfo.msg.isEmpty()) {
        sendErrorReply(errMSsgInfo.errType, errMSsgInfo.msg);
        return QDBusUnixFileDescriptor(-1);
    }

    return fileDescriptor;
}
void DbusFaceService::VerifyStop(QString actionId)
{
    qDebug() << "VerifyStop actionId" << actionId;

    m_spTimer->start();

    ErrMsgInfo errMSsgInfo;
    m_spDriverManger->verifyStop(actionId, errMSsgInfo);
    if (!errMSsgInfo.msg.isEmpty()) {
        sendErrorReply(errMSsgInfo.errType, errMSsgInfo.msg);
    }
}

void DbusFaceService::Delete(QString chara)
{
    qDebug() << "Delete chara" << chara;

    m_spTimer->start();

    ErrMsgInfo errMSsgInfo;
    m_spDriverManger->deleteChara(chara, errMSsgInfo);
    if (!errMSsgInfo.msg.isEmpty()) {
        sendErrorReply(errMSsgInfo.errType, errMSsgInfo.msg);
    }
}

bool DbusFaceService::getClaim() const
{
    qDebug() << "getClaim";

    m_spTimer->start();

    return m_spDriverManger->getClaim();
}

QStringList DbusFaceService::getList() const
{
    qDebug() << "getList";

    m_spTimer->start();

    return m_spDriverManger->getCharaList();
}

qint32 DbusFaceService::getCharaType() const
{
    qDebug() << "getCharaType";

    m_spTimer->start();

    return m_spDriverManger->getCharaType();
}

void DbusFaceService::exitApp()
{
    qDebug() << "idle time out exit app";

    QCoreApplication::quit();
}
