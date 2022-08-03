#ifndef WORKMODULE_H
#define WORKMODULE_H

#include "seeta/Common/CStruct.h"

#include <memory>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <unistd.h>
#include <QDebug>
#include <QImage>
#include <QPixmap>
#include <QThread>

QT_BEGIN_NAMESPACE
class QMutex;
QT_END_NAMESPACE

class DriverManger;
class ErollThread : public QThread
{
public:
    ErollThread(QSharedPointer<DriverManger> spDriver);
    void Stop();
    void Start(QString m_actionId, int socket);

protected:
    void run();
    void sendCapture(cv::Mat mat);

private:
    QWeakPointer<DriverManger> m_wpDriver;
    QSharedPointer<cv::VideoCapture> m_spCapture;
    QString m_actionId;
    bool m_bRun;
    int m_fileSocket;
    bool m_bFirst;
};


class VerifyThread : public QThread
{
public:
    VerifyThread(QSharedPointer<DriverManger> spDriver);
    void Stop();
    void Start(QString m_actionId, QVector<float *> charas);

protected:
    void run();

private:
    QWeakPointer<DriverManger> m_wpDriver;
    QSharedPointer<cv::VideoCapture> m_spCapture;
    QString m_actionId;
    bool m_bRun;
    QVector<float *> m_charaDatas;
    QSharedPointer<QMutex> m_mutex;
};

#endif // WORKMODULE_H
