// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "workmodule.h"
#include "modelmanger.h"

#include <QCameraInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QtConcurrent>


ErollThread::ErollThread(QObject *parent)
    : QObject(parent)
    , m_camera(nullptr)
    , m_imageCapture(nullptr)
    , m_bFirst(false)
    , m_stopCapture(false)
    , m_checkDone(true)
{
}

void ErollThread::Start(QString actionId, int socket)
{
    qDebug() << "ErollThread::Start thread:" << QThread::currentThreadId();
    m_stopCapture = false;
    m_actionId = actionId;
    m_fileSocket = socket;
    m_bFirst = true;
    run();
}

void ErollThread::run()
{
    m_camera.reset();
    auto cameras = QCameraInfo::availableCameras();
    for (const auto &obj : qAsConst(cameras)) {
        m_camera.reset(new QCamera(obj));
        if (m_camera->isAvailable())
            break;
    }


    if (m_camera.isNull()) {
        qDebug() << "open camera fail";
        Q_EMIT processStatus(m_actionId, FaceEnrollException);
        return;
    }

    m_camera->setCaptureMode(QCamera::CaptureStillImage);
    m_imageCapture.reset(new QCameraImageCapture(m_camera.data()));
    m_imageCapture->setCaptureDestination(QCameraImageCapture::CaptureToBuffer);
    QList<QSize> supportedResolutions = m_imageCapture->supportedResolutions();
    if (supportedResolutions.contains(QSize(800, 600))) {
        auto settings = m_imageCapture->encodingSettings();
        settings.setResolution(QSize(800, 600));
        m_imageCapture->setEncodingSettings(settings);
    }
    connect(m_camera.data(), &QCamera::stateChanged, this, &ErollThread::updateCameraState);
    connect(m_imageCapture.data(), &QCameraImageCapture::readyForCaptureChanged, this, &ErollThread::readyForCapture);
    connect(m_imageCapture.data(), &QCameraImageCapture::imageCaptured, this, &ErollThread::processCapturedImage);
    connect(m_imageCapture.data(), QOverload<int, QCameraImageCapture::Error, const QString &>::of(&QCameraImageCapture::error),
            this, &ErollThread::captureError);
    m_camera->start();
}

void ErollThread::Stop()
{
    qDebug() << "ErollThread::Stop thread:" << QThread::currentThreadId();
    m_imageCapture->cancelCapture();
    m_stopCapture = true;
    m_camera->stop();
    m_camera->unload();
    m_camera.reset();
    close(m_fileSocket);
}

void ErollThread::sendCapture(QImage &img)
{
    QImage image(img.convertToFormat(QImage::Format_RGB888));
    uchar *buf;
    unsigned long size = 0;
    if (m_bFirst) {
        qDebug() << "in sendCapture: first";
        size = sizeof(uchar)
               * (5 * sizeof(int32_t) + static_cast<unsigned long>(image.sizeInBytes()));
        buf = static_cast<uchar *>(malloc(size));
        qint32 *myBuf = reinterpret_cast<qint32 *>(buf);
        myBuf[0] = 0;
        myBuf[1] = 0;
        myBuf[2] = image.width();
        myBuf[3] = image.height();
        myBuf[4] = static_cast<qint32>(image.sizeInBytes());

        memcpy(&buf[5 * sizeof(int32_t)], image.bits(), static_cast<unsigned long>(myBuf[4]));
    } else {
        size = sizeof(uchar) * (sizeof(int32_t) + static_cast<unsigned long>(image.sizeInBytes()));
        buf = static_cast<uchar *>(malloc(size));

        qint32 *myBuf = reinterpret_cast<qint32 *>(buf);
        myBuf[0] = static_cast<qint32>(image.sizeInBytes());

        memcpy(&buf[1 * sizeof(int32_t)], image.bits(), static_cast<unsigned long>(myBuf[0]));
    }

    unsigned long countSize = size;
    while (countSize > 0 && !m_stopCapture) {
        long sendSize = write(m_fileSocket, &buf[size - countSize], static_cast<size_t>(countSize));
        if (sendSize < 0) {
            QCoreApplication::processEvents();
            continue;
        }
        countSize -= static_cast<unsigned long>(sendSize);
    }
    free(buf);
}

void ErollThread::updateCameraState(QCamera::State state)
{
    qInfo() << "updateCameraState" << state;
}

void ErollThread::readyForCapture(bool ready)
{
    if (m_imageCapture && ready) {
        m_imageCapture->capture();
        qInfo() << "ErollThread::readyForCapture";
    }
}

void ErollThread::captureError(int, QCameraImageCapture::Error, const QString &errorString)
{
    qDebug() << "read camera fail:" << errorString;
    Q_EMIT processStatus(m_actionId, FaceEnrollException);
    return;
}

void ErollThread::processCapturedImage(int id, const QImage &preview)
{
    if (m_stopCapture)
        return;

    QImage img;
    if (preview.size() == QSize(800, 600))
        img = preview;
    else
        img = preview.scaled(QSize(800, 600));

    if (1 == id) {
        sendCapture(img);
        m_bFirst = false;
    } else {
        char buf[1];
        while (read(m_fileSocket, buf, 1) == 0) {
            usleep(1);
        }
        sendCapture(img);
    }


    m_imageCapture->capture();

    if (!m_checkDone) {
        return;
    } else {
        m_checkDone = false;
    }

    QFutureWatcher<void> *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, [this, watcher] {
        m_checkDone = true;
        watcher->deleteLater();
    });
    QFuture<void> future = QtConcurrent::run([=]() {
        qDebug() << "thread id:" << QThread::currentThreadId();
        QImage img1 = img.convertToFormat(QImage::Format_RGB888).rgbSwapped();
        SeetaImageData image;
        image.height = img1.height();
        image.width = img1.width();
        image.channels = 3;
        image.data = img1.scanLine(0);

        if (!ModelManger::getSingleInstanceModel().avaliable()) {
            return;
        }
        ModelManger::getSingleInstanceModel().setFaceTrackSize(image.width, image.height);
        auto faces = ModelManger::getSingleInstanceModel().getFaceTracker()->Track(image);
        if (faces.size == 0) {
            QMetaObject::invokeMethod(this, "processStatus", Qt::QueuedConnection, Q_ARG(QString, m_actionId), Q_ARG(qint32, FaceEnrollNoFace));
        } else if (faces.size > 1) {
            QMetaObject::invokeMethod(this, "processStatus", Qt::QueuedConnection, Q_ARG(QString, m_actionId), Q_ARG(qint32, FaceEnrollTooManyFace));
        } else {
            qDebug() << "faces :" << faces.size;

            auto points = ModelManger::getSingleInstanceModel()
                              .getFaceLandmarker()
                              ->mark(image, faces.data[0].pos);
            ModelManger::getSingleInstanceModel().getQualityAssessor()->feed(image,
                                                                             faces.data[0].pos,
                                                                             points.data(),
                                                                             5);

            seeta::QualityResult quality = ModelManger::getSingleInstanceModel()
                                               .getQualityAssessor()
                                               ->query(seeta::BRIGHTNESS);
            if (quality.level < seeta::MEDIUM) {
                QMetaObject::invokeMethod(this, "processStatus", Qt::QueuedConnection, Q_ARG(QString, m_actionId), Q_ARG(qint32, FaceEnrollBrightness));
                return;
            }
            quality = ModelManger::getSingleInstanceModel().getQualityAssessor()->query(
                seeta::RESOLUTION);
            if (quality.level < seeta::MEDIUM) {
                QMetaObject::invokeMethod(this, "processStatus", Qt::QueuedConnection, Q_ARG(QString, m_actionId), Q_ARG(qint32, FaceEnrollFaceNotClear));
                return;
            }
            quality = ModelManger::getSingleInstanceModel().getQualityAssessor()->query(
                seeta::CLARITY);
            if (quality.level < seeta::MEDIUM) {
                QMetaObject::invokeMethod(this, "processStatus", Qt::QueuedConnection, Q_ARG(QString, m_actionId), Q_ARG(qint32, FaceEnrollFaceNotClear));
                return;
            }

            quality = ModelManger::getSingleInstanceModel().getQualityAssessor()->query(
                seeta::INTEGRITY);
            if (quality.level < seeta::MEDIUM) {
                QMetaObject::invokeMethod(this, "processStatus", Qt::QueuedConnection, Q_ARG(QString, m_actionId), Q_ARG(qint32, FaceEnrollFaceTooBig));
                return;
            }
            quality = ModelManger::getSingleInstanceModel().getQualityAssessor()->query(
                seeta::POSE_EX);
            if (quality.level < seeta::MEDIUM) {
                QMetaObject::invokeMethod(this, "processStatus", Qt::QueuedConnection, Q_ARG(QString, m_actionId), Q_ARG(qint32, FaceEnrollFaceNotCenter));
                return;
            }

            auto status = ModelManger::getSingleInstanceModel()
                              .getFaceAntiSpoofing()
                              ->Predict(image, faces.data[0].pos, points.data());
            if (status == seeta::FaceAntiSpoofing::SPOOF) {
                QMetaObject::invokeMethod(this, "processStatus", Qt::QueuedConnection, Q_ARG(QString, m_actionId), Q_ARG(qint32, FaceEnrollNotRealHuman));
                return;

            } else {
                qDebug() << "antispoofing ok";
            }

            seeta::ImageData cropface = ModelManger::getSingleInstanceModel()
                                            .getFaceRecognizer()
                                            ->CropFaceV2(image, points.data());
            int size = ModelManger::getSingleInstanceModel().getFaceRecognizer()->GetExtractFeatureSize();
            float *features = static_cast<float *>(
                malloc(sizeof(float) * static_cast<unsigned long>(size)));
            bool bFound = ModelManger::getSingleInstanceModel()
                              .getFaceRecognizer()
                              ->ExtractCroppedFace(cropface, features);
            if (bFound) {
                qDebug() << "ExtractCroppedFace ok" << features;
                m_stopCapture = true;
                qRegisterMetaType<float*>("float*");
                QMetaObject::invokeMethod(this, "processStatus", Qt::QueuedConnection
                                          , Q_ARG(QString, m_actionId)
                                          , Q_ARG(qint32, FaceEnrollSuccess)
                                          , Q_ARG(float*, features)
                                          , Q_ARG(int, size));
                return;
            }
            free(features);
        }
    });
    watcher->setFuture(future);
}

VerifyThread::VerifyThread(QObject *parent)
    : QObject(parent)
    , m_camera(nullptr)
    , m_imageCapture(nullptr)
{
}

void VerifyThread::Start(QString actionId, QVector<float*> charas)
{
    qDebug() << "VerifyThread::Start thread:" << QThread::currentThreadId();
    m_actionId = actionId;
    for (int i = 0; i < m_charaDatas.size(); i++) {
        if (m_charaDatas[i] != nullptr) {
            free(m_charaDatas[i]);
        }
    }
    m_charaDatas.clear();
    qDebug() << "charas size" << charas.size();
    m_charaDatas = charas;

    run();
}

void VerifyThread::run()
{
    qDebug() << "Verify run";
    m_camera.reset();
    auto cameras = QCameraInfo::availableCameras();
    for (const auto &obj : qAsConst(cameras)) {
        m_camera.reset(new QCamera(obj));
        if (m_camera->isAvailable())
            break;
    }
    if (m_camera.isNull()) {
        qDebug() << "open camera fail";
        Q_EMIT processStatus(m_actionId, FaceEnrollException);
        return;
    }

    m_camera->setCaptureMode(QCamera::CaptureStillImage);
    m_imageCapture.reset(new QCameraImageCapture(m_camera.data()));
    m_imageCapture->setCaptureDestination(QCameraImageCapture::CaptureToBuffer);
    QList<QSize> supportedResolutions = m_imageCapture->supportedResolutions();
    if (supportedResolutions.contains(QSize(800, 600))) {
        auto settings = m_imageCapture->encodingSettings();
        settings.setResolution(QSize(800, 600));
        m_imageCapture->setEncodingSettings(settings);
    }

    connect(m_camera.data(), &QCamera::stateChanged, this, &VerifyThread::updateCameraState);
    connect(m_imageCapture.data(), &QCameraImageCapture::readyForCaptureChanged, this, &VerifyThread::readyForCapture);
    connect(m_imageCapture.data(), &QCameraImageCapture::imageCaptured, this, &VerifyThread::processCapturedImage);
    connect(m_imageCapture.data(), QOverload<int, QCameraImageCapture::Error, const QString &>::of(&QCameraImageCapture::error),
            this, &VerifyThread::captureError);
    m_camera->start();
}

void VerifyThread::updateCameraState(QCamera::State state)
{
    qDebug() << "updateCameraState" << state;
}

void VerifyThread::readyForCapture(bool ready)
{
    if (m_imageCapture && ready) {
        m_imageCapture->capture();
        qDebug() << "VerifyThread::readyForCapture";
    }
}

void VerifyThread::captureError(int, QCameraImageCapture::Error, const QString &errorString)
{
    qDebug() << "read camera fail:" << errorString;
    Q_EMIT processStatus(m_actionId, FaceEnrollException);
    return;
}

void VerifyThread::processCapturedImage(int id, const QImage &preview)
{
    Q_UNUSED(id)

    QImage img;
    if (preview.size() == QSize(800, 600))
        img = preview.convertToFormat(QImage::Format_RGB888).rgbSwapped();
    else
        img = preview.scaled(QSize(800, 600)).convertToFormat(QImage::Format_RGB888).rgbSwapped();

    SeetaImageData image;
    image.height = img.height();
    image.width = img.width();
    image.channels = 3;
    image.data = img.scanLine(0);

    ModelManger::getSingleInstanceModel().setFaceTrackSize(image.width, image.height);
    auto faces = ModelManger::getSingleInstanceModel().getFaceTracker()->Track(image);
    if (faces.size == 0) {
        Q_EMIT processStatus(m_actionId, FaceVerifyNoFace);
    } else if (faces.size > 1) {
        Q_EMIT processStatus(m_actionId, FaceVerifyTooManyFace);
    } else {
        qDebug() << "faces :" << faces.size;

        auto points = ModelManger::getSingleInstanceModel()
                          .getFaceLandmarker()
                          ->mark(image, faces.data[0].pos);
        ModelManger::getSingleInstanceModel().getQualityAssessor()->feed(image,
                                                                         faces.data[0].pos,
                                                                         points.data(),
                                                                         5);

        seeta::QualityResult quality = ModelManger::getSingleInstanceModel()
                                           .getQualityAssessor()
                                           ->query(seeta::BRIGHTNESS);

        auto resetFaceTracker = [this] (){
            ModelManger::getSingleInstanceModel().getFaceTracker()->Reset();
            m_imageCapture->capture();
        };
        if (quality.level < seeta::MEDIUM) {
            Q_EMIT processStatus(m_actionId, FaceVerifyBrightness);
            resetFaceTracker();
            return;
        }
        quality = ModelManger::getSingleInstanceModel().getQualityAssessor()->query(
            seeta::RESOLUTION);
        if (quality.level < seeta::MEDIUM) {
            Q_EMIT processStatus(m_actionId, FaceVerifyFaceNotClear);
            resetFaceTracker();
            return;
        }
        quality = ModelManger::getSingleInstanceModel().getQualityAssessor()->query(
            seeta::CLARITY);
        if (quality.level < seeta::MEDIUM) {
            Q_EMIT processStatus(m_actionId, FaceVerifyFaceNotClear);
            resetFaceTracker();
            return;
        }

        quality = ModelManger::getSingleInstanceModel().getQualityAssessor()->query(
            seeta::INTEGRITY);
        if (quality.level < seeta::MEDIUM) {
            Q_EMIT processStatus(m_actionId, FaceVerifyFaceTooBig);
            resetFaceTracker();
            return;
        }
        quality = ModelManger::getSingleInstanceModel().getQualityAssessor()->query(
            seeta::POSE_EX);
        if (quality.level < seeta::MEDIUM) {
            Q_EMIT processStatus(m_actionId, FaceVerifyFaceNotCenter);
            resetFaceTracker();
            return;
        }

        auto status = ModelManger::getSingleInstanceModel()
                          .getFaceAntiSpoofing()
                          ->Predict(image, faces.data[0].pos, points.data());
        if (status == seeta::FaceAntiSpoofing::SPOOF) {
            Q_EMIT processStatus(m_actionId, FaceVerifyNotRealHuman);
            resetFaceTracker();
            return;

        } else {
            qDebug() << "antispoofing ok";
        }

        seeta::ImageData cropface = ModelManger::getSingleInstanceModel()
                                        .getFaceRecognizer()
                                        ->CropFaceV2(image, points.data());
        int size = ModelManger::getSingleInstanceModel().getFaceRecognizer()->GetExtractFeatureSize();
        float *features = static_cast<float *>(
            malloc(sizeof(float) * static_cast<unsigned long>(size)));
        bool bFound = ModelManger::getSingleInstanceModel()
                          .getFaceRecognizer()
                          ->ExtractCroppedFace(cropface, features);
        if (bFound) {
            qDebug() << "ExtractCroppedFace ok" << features;
            for (auto iter : qAsConst(m_charaDatas)) {
                float score = ModelManger::getSingleInstanceModel()
                                  .getFaceRecognizer()
                                  ->CalculateSimilarity(features, iter);
                qDebug() << "score :" << score;
                if (score > float(FACETlIGHTHRESHOLD)) {
                    Q_EMIT processStatus(m_actionId, FaceVerifySuccess);
                    return;
                }
            }
        }
    }
    m_imageCapture->capture();
}

// 等待线程结束后返回
void VerifyThread::Stop()
{
    qDebug() << "VerifyThread::Stop thread:" << QThread::currentThreadId();

    m_imageCapture->cancelCapture();
    // 当关闭相机后, 会取消图片的抓取, 此时不需要去处理抓取的图片
    disconnect(m_imageCapture.data(), &QCameraImageCapture::imageCaptured, this, &VerifyThread::processCapturedImage);
    m_camera->stop();
    m_camera->unload();
    for (int i = 0; i < m_charaDatas.size(); i++) {
        if (m_charaDatas[i] != nullptr) {
            free(m_charaDatas[i]);
        }
    }
    m_camera.reset();
    m_charaDatas.clear();
}
