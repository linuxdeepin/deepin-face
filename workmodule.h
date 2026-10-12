// SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef WORKMODULE_H
#define WORKMODULE_H

#include <QCamera>
#include <QCameraDevice>
#include <atomic>
#include <memory>
#include <unistd.h>
#include <QDebug>
#include <QImage>
#include <QPixmap>
#include <QThread>
#include <QImageCapture>
#include <QMediaCaptureSession>

QT_BEGIN_NAMESPACE
class QMutex;
class QTimer;
QT_END_NAMESPACE

// 查询系统相机隐私开关（org.deepin.dde.Daemon1 的 GetCameraPrivacy）：
// 返回 true 表示摄像头被开关关闭；known 为 false 表示查询不可用（旧版本服务或无相机设备），
// 调用方应回退到设备可用性等本地判断。
bool queryCameraPrivacy(bool *known = nullptr);

class DriverManger;
class QMediaCaptureSession;
class ErollThread : public QObject
{
    Q_OBJECT
public:
    ErollThread(QObject *parent=nullptr);

Q_SIGNALS:
    void processStatus(QString actionId, qint32 status, float *faceChara = nullptr, int size = 0);
public Q_SLOTS:
    void Stop();
    void Start(QString m_actionId, int socket);



protected:
    void run();
    void sendCapture(QImage &img);


private Q_SLOTS:
    // void updateCameraState(QCamera::State state);
    void readyForCapture(bool ready);
    void captureError(int err, QImageCapture::Error, const QString &errorString);
    void processCapturedImage(int id, const QImage &preview);
    void checkCameraAvailable();

public:
    std::atomic<bool> m_stopCapture;

private:
    bool isCameraUnavailable();
    QScopedPointer<QCamera> m_camera;
    QScopedPointer<QImageCapture> m_imageCapture;
    QScopedPointer<QMediaCaptureSession> m_captureSession;
    QString m_actionId;
    int m_fileSocket;
    bool m_bFirst;
    bool m_checkDone;
    std::atomic<int> m_nullCount;
    QCameraDevice m_cameraDevice;
    QTimer *m_cameraWatchTimer = nullptr;
};


class VerifyThread : public QObject
{
    Q_OBJECT
public:
    VerifyThread(QObject *parent=nullptr);

Q_SIGNALS:
    void processStatus(QString actionId, qint32 status, float *faceChara = nullptr, int size = 0);

public Q_SLOTS:
    void Stop();
    void Start(QString m_actionId, QVector<float*> charas);

protected:
    void run();

private Q_SLOTS:
    // void updateCameraState(QCamera::State state);
    void readyForCapture(bool ready);
    void captureError(int err, QImageCapture::Error, const QString &errorString);
    void processCapturedImage(int id, const QImage &preview);
    void checkCameraAvailable();

private:
    bool isCameraUnavailable();
    QScopedPointer<QCamera> m_camera;
    QScopedPointer<QImageCapture> m_imageCapture;
    QScopedPointer<QMediaCaptureSession> m_captureSession;
    QString m_actionId;
    QVector<float*> m_charaDatas;
    QCameraDevice m_cameraDevice;
    QTimer *m_cameraWatchTimer = nullptr;
};

#endif // WORKMODULE_H
