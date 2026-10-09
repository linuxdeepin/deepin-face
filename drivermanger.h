// SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef DRIVERMANGER_H
#define DRIVERMANGER_H

#include "charadatamanger.h"
#include "definehead.h"
#include "seeta/CTrackingFaceInfo.h"
#include "seeta/FaceAntiSpoofing.h"
#include "seeta/FaceDetector.h"
#include "seeta/FaceLandmarker.h"
#include "seeta/FaceRecognizer.h"
#include "seeta/FaceTracker.h"
#include "seeta/QualityAssessor.h"
#include "seeta/QualityOfPoseEx.h"
#include "workmodule.h"

#include <QSharedPointer>
#include <QtDBus/QtDBus>

#include <memory>

struct ActionInfo {
    QString chara; // DA生成特征值
    qint32 status;
    ActionType actionType;
    float *faceChara;
    int size;
    bool cameraNotEnabledReported = false; // “摄像头未开启”是否已上报（DConfig 事件与看门狗去重）
};

struct ErrMsgInfo {
    QDBusError::ErrorType errType;
    QString msg;
};

class DriverManger : public QObject, public QEnableSharedFromThis<DriverManger>
{
    Q_OBJECT
public:
    DriverManger();
    virtual ~DriverManger();
    void init();
    QDBusUnixFileDescriptor enrollStart(QString chara,
                                        qint32 charaType,
                                        QString actionId,
                                        ErrMsgInfo &errMsgInfo);
    void enrollStop(QString actionId, ErrMsgInfo &errMsgInfo);
    QDBusUnixFileDescriptor verifyStart(QStringList charas,
                                        QString actionId,
                                        ErrMsgInfo &errMsgInfo);
    void verifyStop(QString actionId, ErrMsgInfo &errMsgInfo);
    void deleteChara(QString chara, ErrMsgInfo &errMsgInfo);
public Q_SLOTS:
    void processStatus(QString actionId, qint32 status, float *faceChara = nullptr, int size = 0);

public:
    bool getClaim();
    void setClaim(bool bClaim);
    qint32 getCharaType();
    void setCharaType(qint32 qCharaType);
    QStringList getCharaList();
    void setCharaList(QStringList lCharaList);

private:
    QString getStatusMsg(ActionType actionType, qint32 status);
    void emitPropertiesChanged(QVariantMap &qVariantMap);
    void emitStatus(ActionType action, QString actionId, qint32 status, QString msg);
    void initCameraPrivacyWatch();

private:
    QSharedPointer<ErollThread> m_spErollthread;
    QSharedPointer<VerifyThread> m_spVerifyThread;
    QSharedPointer<CharaDataManger> m_spCharaDataManger;
    QMap<QString, ActionInfo> m_actionMap;
    bool m_bClaim; //是否被占用
    qint32 m_charaType;
    QStringList m_charalist;
    QThread *m_eroll;
    QThread *m_verify;
};

#endif
