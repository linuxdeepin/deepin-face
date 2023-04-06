// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef WORKMODULE_H
#define WORKMODULE_H

#include "qcamera.h"
#include <memory>
#include <unistd.h>
#include <QDebug>
#include <QImage>
#include <QPixmap>
#include <QThread>
#include <QCameraImageCapture>

QT_BEGIN_NAMESPACE
class QMutex;
QT_END_NAMESPACE

class DriverManger;
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
    void updateCameraState(QCamera::State state);
    void readyForCapture(bool ready);
    void captureError(int, QCameraImageCapture::Error, const QString &errorString);
    void processCapturedImage(int id, const QImage &preview);

private:
    QScopedPointer<QCamera> m_camera;
    QScopedPointer<QCameraImageCapture> m_imageCapture;
    QString m_actionId;
    int m_fileSocket;
    bool m_bFirst;
    bool m_stopCapture;
    bool m_checkDone;
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
    void updateCameraState(QCamera::State state);
    void readyForCapture(bool ready);
    void captureError(int, QCameraImageCapture::Error, const QString &errorString);
    void processCapturedImage(int id, const QImage &preview);

private:
    QScopedPointer<QCamera> m_camera;
    QScopedPointer<QCameraImageCapture> m_imageCapture;
    QString m_actionId;
    QVector<float*> m_charaDatas;
};

#endif // WORKMODULE_H
