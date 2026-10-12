// SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "drivermanger.h"
#include "modelmanger.h"
#include <sys/socket.h>
#include <sys/types.h>

#include <DConfig>

DCORE_USE_NAMESPACE

DriverManger::DriverManger()
    : m_spCharaDataManger(new CharaDataManger)
    , m_eroll(new QThread(this))
    , m_verify(new QThread(this))
{
}

DriverManger::~DriverManger()
{
    m_eroll->quit();
    m_eroll->wait(1000);
    m_verify->quit();
    m_verify->wait(1000);
}
void DriverManger::init()
{
    m_spErollthread = QSharedPointer<ErollThread>(new ErollThread());
    m_spVerifyThread = QSharedPointer<VerifyThread>(new VerifyThread());
    m_spCharaDataManger->loadCharaData();
    // 监听相机隐私开关（dde-system-daemon 的 DConfig）：FN 键/接口关闭摄像头时
    // 第一时间结束正在进行的录入/验证，无需等看门狗轮询或设备从 Qt 列表消失
    initCameraPrivacyWatch();
    m_bClaim = false;
    m_charalist = this->m_spCharaDataManger->getCharaList();
    // 人脸能力与摄像头开关状态解耦：摄像头 FN 键关闭（设备消失）时仍上报人脸能力，
    // 由控制中心始终展示生物认证（人脸）入口，实际操作时再上报摄像头未开启。
    m_charaType = FACECHARATYPE;
    m_spErollthread->moveToThread(m_eroll);
    m_spVerifyThread->moveToThread(m_verify);
    connect(m_spErollthread.data(), &ErollThread::processStatus, this, &DriverManger::processStatus);
    connect(m_spVerifyThread.data(), &VerifyThread::processStatus, this, &DriverManger::processStatus);
    m_eroll->start();
    m_verify->start();
    qDebug() << "DriverManger::init thread:" << QThread::currentThreadId();
}

// 监听 dde-system-daemon 持久化的相机隐私开关（SetCameraPrivacy 写入的
// org.deepin.dde.daemon / org.deepin.dde.daemon.system / cameraPrivacyEnabled）。
// 开关打开时立即按“摄像头未开启”结束当前操作；DConfig 不可用（旧版本系统）时
// 静默跳过，仍由 GetCameraPrivacy 查询与运行期看门狗兜底。
void DriverManger::initCameraPrivacyWatch()
{
    auto *config = DConfig::create(QStringLiteral("org.deepin.dde.daemon"),
                                   QStringLiteral("org.deepin.dde.daemon.system"),
                                   QString(), this);
    if (!config || !config->isValid()) {
        qWarning() << "camera privacy dconfig unavailable, fallback to watchdog";
        return;
    }

    connect(config, &DConfig::valueChanged, this, [this, config](const QString &key) {
        if (key != QStringLiteral("cameraPrivacyEnabled"))
            return;
        if (!config->value(key).toBool())
            return;

        qWarning() << "camera privacy switch on, abort active face action";
        const auto actionIds = m_actionMap.keys();
        for (const auto &actionId : actionIds) {
            const auto actionType = m_actionMap.value(actionId).actionType;
            const qint32 status = (actionType == Enroll) ? FaceEnrollCameraNotEnabled
                                                         : FaceVerifyCameraNotEnabled;
            processStatus(actionId, status);
        }
    });
}

QDBusUnixFileDescriptor DriverManger::enrollStart(QString chara,
                                                  qint32 charaType,
                                                  QString actionId,
                                                  ErrMsgInfo &errMsgInfo)
{
    if (m_bClaim) {
        errMsgInfo.errType = QDBusError::Failed;
        errMsgInfo.msg = "face claim";
        return QDBusUnixFileDescriptor(-1);
    }

    if (this->m_charaType != charaType) {
        errMsgInfo.errType = QDBusError::InvalidArgs;
        errMsgInfo.msg = "charaType not match";
        return QDBusUnixFileDescriptor(-1);
    }

    auto iter = std::find(m_charalist.begin(), m_charalist.end(), chara);
    if (iter != m_charalist.end()) {
        errMsgInfo.errType = QDBusError::Failed;
        errMsgInfo.msg = "face had same chara";
        return QDBusUnixFileDescriptor(-1);
    }

    int fd[2];
    int r = socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, fd);
    if (r < 0) {
        errMsgInfo.errType = QDBusError::Failed;
        errMsgInfo.msg = "create socketpair err";
        return QDBusUnixFileDescriptor(-1);
    }
    ModelManger::getSingleInstanceModel().load();

    m_bClaim = true;

    ActionInfo info;
    info.chara = chara;
    info.actionType = Enroll;
    m_actionMap[actionId] = info;

    QMetaObject::invokeMethod(m_spErollthread.data(),
                              "Start",
                              Qt::QueuedConnection,
                              Q_ARG(QString, actionId),
                              Q_ARG(int, fd[1]));

    return QDBusUnixFileDescriptor(fd[0]);
}

void DriverManger::enrollStop(QString actionId, ErrMsgInfo &errMsgInfo)
{
    if (m_actionMap.count(actionId) != 1) {
        errMsgInfo.errType = QDBusError::InvalidArgs;
        errMsgInfo.msg = "not found action";
        return;
    }

    m_bClaim = false;

    auto actionInfo = m_actionMap[actionId];
    if (actionInfo.status == FaceEnrollSuccess) {
        m_spCharaDataManger->insertCharaDate(actionInfo.chara,
                                             actionInfo.faceChara,
                                             actionInfo.size);
        if (actionInfo.faceChara != nullptr) {
            free(actionInfo.faceChara);
        }
    }
    qDebug() << "start Erollthread stop";
    m_spErollthread->m_stopCapture = true;
    QMetaObject::invokeMethod(m_spErollthread.data(), "Stop",
                              Qt::BlockingQueuedConnection);
    m_actionMap.remove(actionId);
    ModelManger::getSingleInstanceModel().unLoad();
    qDebug() << "ModelManger::unLoad";
    auto charaList = m_spCharaDataManger->getCharaList();
    setCharaList(charaList);

    qDebug() << "EnrollStop finish";
}

QDBusUnixFileDescriptor DriverManger::verifyStart(QStringList charas,
                                                  QString actionId,
                                                  ErrMsgInfo &errMsgInfo)
{
    if (m_bClaim) {
        errMsgInfo.errType = QDBusError::Failed;
        errMsgInfo.msg = "face claim";
        return QDBusUnixFileDescriptor(-1);
    }

    QVector<float*> faceCharas = m_spCharaDataManger->getCharaData(charas);

    m_bClaim = true;
    ActionInfo info;
    info.actionType = Verify;
    m_actionMap[actionId] = info;
    qDebug() << "claim=true;";
    ModelManger::getSingleInstanceModel().load();

    qRegisterMetaType<QVector<float*>>("QVector<float*>");
    QMetaObject::invokeMethod(m_spVerifyThread.data(),
                              "Start",
                              Qt::QueuedConnection,
                              Q_ARG(QString, actionId),
                              Q_ARG(QVector<float*>, faceCharas));
    return QDBusUnixFileDescriptor(0);
}

void DriverManger::verifyStop(QString actionId, ErrMsgInfo &errMsgInfo)
{
    qDebug() << __LINE__ << "DriverManger::verifyStop";
    if (m_actionMap.count(actionId) != 1) {
        errMsgInfo.errType = QDBusError::InvalidArgs;
        errMsgInfo.msg = "not found action";
        return;
    }

    m_bClaim = false;
    // 线程结束后才释放资源
    QMetaObject::invokeMethod(m_spVerifyThread.data(),
                              "Stop",
                              Qt::BlockingQueuedConnection);
    ModelManger::getSingleInstanceModel().unLoad();
    m_actionMap.remove(actionId);
}

void DriverManger::deleteChara(QString chara, ErrMsgInfo &errMsgInfo)
{
    bool bSuccess = m_spCharaDataManger->deleteCharaData(chara);
    if (!bSuccess) {
        errMsgInfo.errType = QDBusError::Failed;
        errMsgInfo.msg = "delete error";
        return;
    }

    auto charaList = m_spCharaDataManger->getCharaList();
    setCharaList(charaList);
}

bool DriverManger::getClaim()
{
    return this->m_bClaim;
}

void DriverManger::setClaim(bool bClaim)
{
    this->m_bClaim = bClaim;
}

qint32 DriverManger::getCharaType()
{
    return this->m_charaType;
}

void DriverManger::setCharaType(qint32 qCharaType)
{
    if (this->m_charaType != qCharaType) {
        qDebug() << "set chara Type" << qCharaType;
        this->m_charaType = qCharaType;
        QVariantMap qChangedProps;
        qChangedProps.insert("CharaType", this->m_charaType);
        emitPropertiesChanged(qChangedProps);
    }
}

QStringList DriverManger::getCharaList()
{
    return this->m_charalist;
}

void DriverManger::setCharaList(QStringList lCharaList)
{
    if (this->m_charalist.size() != lCharaList.size()) {
        this->m_charalist = lCharaList;
        QVariantMap qChangedProps;
        qChangedProps.insert("List", this->m_charalist);

        emitPropertiesChanged(qChangedProps);
    }
}

void DriverManger::processStatus(QString actionId, qint32 status, float *faceChara, int size)
{
    if (m_actionMap.count(actionId) != 1) {
        return;
    }
    auto &actionInfo = m_actionMap[actionId];
    // DConfig 开关事件、看门狗与设备检测可能同时上报“摄像头未开启”，
    // 每个操作只上报一次，避免重复结束同一操作
    if (status == FaceEnrollCameraNotEnabled || status == FaceVerifyCameraNotEnabled) {
        if (actionInfo.cameraNotEnabledReported) {
            return;
        }
        actionInfo.cameraNotEnabledReported = true;
    }
    actionInfo.status = status;
    if (faceChara != nullptr) {
        actionInfo.faceChara = static_cast<float *>(
            malloc(sizeof(float) * static_cast<unsigned long>(size)));
        for (int i = 0; i < size; i++) {
            actionInfo.faceChara[i] = faceChara[i];
        }
        actionInfo.size = size;
    }

    QString msg = getStatusMsg(actionInfo.actionType, status);
    emitStatus(actionInfo.actionType, actionId, status, msg);
}

void DriverManger::emitStatus(ActionType action, QString actionId, qint32 status, QString msg)
{
    qDebug() << "emitStatus action" << actionId << "status" << status;

    QString emitSignal;
    if (action == Enroll) {
        emitSignal = "EnrollStatus";
    } else {
        emitSignal = "VerifyStatus";
    }
    QDBusMessage signal = QDBusMessage::createSignal(SERVERPATH, SERVERINTERFACE, emitSignal);
    signal << actionId;
    signal << status;
    signal << msg;
    QDBusConnection::systemBus().send(signal);
}

void DriverManger::emitPropertiesChanged(QVariantMap &qChangedProps)
{
    qDebug() << "emitPropertiesChanged";

    QDBusMessage signal = QDBusMessage::createSignal(SERVERPATH,
                                                     "org.freedesktop.DBus.Properties",
                                                     "PropertiesChanged");
    signal << SERVERINTERFACE;
    signal << qChangedProps;
    QStringList invalidatedProps;
    signal << invalidatedProps;
    QDBusConnection::systemBus().send(signal);
}

QString DriverManger::getStatusMsg(ActionType actionType, qint32 status)
{
    QString retMsg;
    if (actionType == Enroll) {
        EnrollStatus tempStatus = EnrollStatus(status);
        switch (tempStatus) {
        case FaceEnrollSuccess:
            retMsg = "Enroll Success";
            break;
        case FaceEnrollNotRealHuman:
            retMsg = "Enroll Not Real Human";
            break;
        case FaceEnrollFaceNotCenter:
            retMsg = "Enroll Not Center";
            break;
        case FaceEnrollFaceTooBig:
            retMsg = "Enroll Too Big";
            break;
        case FaceEnrollFaceTooSmall:
            retMsg = "Enroll Too Small";
            break;
        case FaceEnrollNoFace:
            retMsg = "Enroll No Face";
            break;
        case FaceEnrollTooManyFace:
            retMsg = "Enroll Too Many Face";
            break;
        case FaceEnrollFaceNotClear:
            retMsg = "Enroll Not Clear";
            break;
        case FaceEnrollBrightness:
            retMsg = "Enroll Brightness";
            break;
        case FaceEnrollFaceCovered:
            retMsg = "Enroll Covered";
            break;
        case FaceEnrollCancel:
            retMsg = "Enroll Cancel";
            break;
        case FaceEnrollError:
            retMsg = "Enroll Error";
            break;
        case FaceEnrollException:
            retMsg = "Enroll Exception";
            break;
        case FaceEnrollCameraNotEnabled:
            retMsg = "Enroll Camera Not Enabled";
            break;
        }
    } else if (actionType == Verify) {
        VerifyStatus tempStatus = VerifyStatus(status);
        switch (tempStatus) {
        case FaceVerifySuccess:
            retMsg = "Verify Success";
            break;
        case FaceVerifyNotRealHuman:
            retMsg = "Verify Not Real Human";
            break;
        case FaceVerifyFaceNotCenter:
            retMsg = "Verify Not Center";
            break;
        case FaceVerifyFaceTooBig:
            retMsg = "Verify Too Big";
            break;
        case FaceVerifyFaceTooSmall:
            retMsg = "Verify Too Small";
            break;
        case FaceVerifyNoFace:
            retMsg = "Verify No Face";
            break;
        case FaceVerifyTooManyFace:
            retMsg = "Verify Too Many Face";
            break;
        case FaceVerifyFaceNotClear:
            retMsg = "Verify Not Clear";
            break;
        case FaceVerifyBrightness:
            retMsg = "Verify Brightness";
            break;
        case FaceVerifyFaceCovered:
            retMsg = "Verify Covered";
            break;
        case FaceVerifyCancel:
            retMsg = "Verify Cancel";
            break;
        case FaceVerifyError:
            retMsg = "Verify Error";
            break;
        case FaceVerifyException:
            retMsg = "Verify Exception";
            break;
        case FaceVerifyCameraNotEnabled:
            retMsg = "Verify Camera Not Enabled";
            break;
        }
    }

    return retMsg;
}
