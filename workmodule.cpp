#include "workmodule.h"
#include "drivermanger.h"
#include "modelmanger.h"

#include <QCameraInfo>

ErollThread::ErollThread(QSharedPointer<DriverManger> spDriver)
    :m_wpDriver(spDriver)
    ,m_spCapture(new cv::VideoCapture)
    ,m_bRun(false)
{

}

void ErollThread::Start(QString actionId, int socket){
    this->m_actionId=actionId;
    this->m_bRun=true;
    this->m_fileSocket=socket;
    this->m_bFirst=true;
    this->start();
}

void ErollThread::run(){

    qDebug()<<"ErollThread run";

    QSharedPointer<DriverManger> driverManger;
    if(m_wpDriver.isNull()){
        return;
    }else{
        driverManger=m_wpDriver.lock();
    }

    QList<QCameraInfo> cameraInfo = QCameraInfo::availableCameras();
    bool bOpen = false;
    QList<QCameraInfo>::iterator iter=cameraInfo.begin();
    while(iter != cameraInfo.end()){
        bOpen = m_spCapture->open(iter->deviceName().toStdString());
        if(bOpen){
            break;
        }
        iter++;
    }
    if(!bOpen){
        qDebug()<<"open camera fail";
        driverManger->processStatus(m_actionId,FaceEnrollException);
        return;
    }

    bool bTrack=true;
    m_spCapture->set( cv::CAP_PROP_FRAME_WIDTH, 800 );
    m_spCapture->set( cv::CAP_PROP_FRAME_HEIGHT, 600 );
    cv::Mat mat;

    while(m_bRun){
        bool bRead=m_spCapture->read(mat);
        if(!bRead){
            qDebug()<<"read camera fail";
            driverManger->processStatus(m_actionId,FaceEnrollException);
            return;
        }
        if(!bTrack){
            continue;
        }
        if(mat.channels() == 4){
            cv::cvtColor(mat, mat, cv::COLOR_RGBA2BGR);
        }

        cv::cvtColor(mat, mat, cv::COLOR_BGR2RGB);

        if(m_bFirst){
            sendCapture(mat);
            m_bFirst=false;
        }else {
            char buf[1];
            if(read(m_fileSocket,buf,1)==1){
                sendCapture(mat);
            }
        }

        cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);

        SeetaImageData image;
        image.height = mat.rows;
        image.width = mat.cols;
        image.channels = mat.channels();
        image.data = mat.data;

        if(!ModelManger::getSingleInstanceModel().avaliable()){
            continue;
        }
        ModelManger::getSingleInstanceModel().setFaceTrackSize(image.width,image.height);
        auto faces= ModelManger::getSingleInstanceModel().getFaceTracker()->Track(image);
        if(faces.size==0){
            driverManger->processStatus(m_actionId,FaceEnrollNoFace);
        }else if(faces.size>1)
        {
            driverManger->processStatus(m_actionId,FaceEnrollTooManyFace);
        }
        else
        {
            qDebug()<<"faces :"<<faces.size;

            auto points = ModelManger::getSingleInstanceModel().getFaceLandmarker()->mark(image,faces.data[0].pos);
            ModelManger::getSingleInstanceModel().getQualityAssessor()->feed(image, faces.data[0].pos, points.data(), 5);

            seeta::QualityResult quality = ModelManger::getSingleInstanceModel().getQualityAssessor()->query(seeta::BRIGHTNESS);
            if(quality.level<seeta::MEDIUM){
                driverManger->processStatus(m_actionId,FaceEnrollBrightness);
                continue;
            }
            quality = ModelManger::getSingleInstanceModel().getQualityAssessor()->query(seeta::RESOLUTION);
            if(quality.level<seeta::MEDIUM){
                driverManger->processStatus(m_actionId,FaceEnrollFaceNotClear);
                continue;
            }
            quality = ModelManger::getSingleInstanceModel().getQualityAssessor()->query(seeta::CLARITY);
            if(quality.level<seeta::MEDIUM){
                driverManger->processStatus(m_actionId,FaceEnrollFaceNotClear);
                continue;
            }

            quality = ModelManger::getSingleInstanceModel().getQualityAssessor()->query(seeta::INTEGRITY);
            if(quality.level<seeta::MEDIUM){
                driverManger->processStatus(m_actionId,FaceEnrollFaceTooBig);
                continue;
            }
            quality=ModelManger::getSingleInstanceModel().getQualityAssessor()->query(seeta::POSE_EX);
            if(quality.level<seeta::MEDIUM){
                driverManger->processStatus(m_actionId,FaceEnrollFaceNotCenter);
                continue;
            }

            auto status= ModelManger::getSingleInstanceModel().getFaceAntiSpoofing()->Predict(image,faces.data[0].pos, points.data());
            if( status == seeta::FaceAntiSpoofing::SPOOF)
            {
                driverManger->processStatus(m_actionId,FaceEnrollNotRealHuman);
                continue;

            }else {
                qDebug()<<"antispoofing ok";
            }

            seeta::ImageData cropface = ModelManger::getSingleInstanceModel().getFaceRecognizer()->CropFaceV2(image,  points.data());
            int size=ModelManger::getSingleInstanceModel().getFaceRecognizer()->GetExtractFeatureSize();
            float* features=static_cast<float*>(malloc(sizeof(float)*size));
            bool bFound=ModelManger::getSingleInstanceModel().getFaceRecognizer()->ExtractCroppedFace(cropface, features);
            if (bFound){
                qDebug()<<"ExtractCroppedFace ok"<<features;
                bTrack=false;
                driverManger->processStatus(m_actionId,FaceEnrollSuccess,features,size);
            }
            free(features);
        }
    }
    m_spCapture->release();
}

void ErollThread::Stop(){

    this->m_bRun=false;
    close(m_fileSocket);
}

void ErollThread::sendCapture(cv::Mat mat){

    qDebug()<<"sendCapture";

    QImage image((const unsigned char *)mat.data, mat.cols,mat.rows,mat.step, QImage::Format_RGB888);
    uchar* buf;
    unsigned long size=0;
    if(m_bFirst){
        size=sizeof(uchar)*(7*4+image.byteCount());
        buf=static_cast<uchar*>(malloc(size));
        qint32* myBuf= (qint32*)buf;
        myBuf[0]=0;
        myBuf[1]=0;
        myBuf[2]=image.depth();
        myBuf[3]=mat.channels();
        myBuf[4]=image.width();
        myBuf[5]=image.height();
        myBuf[6]=image.byteCount();
        memcpy(&buf[7*4],mat.data,myBuf[6]);
    }
    else {
        size=sizeof(uchar)*(5*4+image.byteCount());
        buf=static_cast<uchar*>(malloc(size));

        qint32* myBuf=(qint32*)buf;

        myBuf[0]=image.depth();
        myBuf[1]=mat.channels();
        myBuf[2]=image.width();
        myBuf[3]=image.height();
        myBuf[4]=image.byteCount();
        memcpy(&buf[5*4],mat.data,myBuf[4]);
    }

    int countSize=size;
    qDebug()<<"total size :"<<size;
    while(countSize>0&&m_bRun){
        int sendSize=write(m_fileSocket,&buf[size-countSize],countSize);
        if(sendSize<0){
            qDebug()<<"send size"<<sendSize;
            continue;
        }
        qDebug()<<"send size :"<<sendSize;
        countSize-=sendSize;
        usleep(1);
    }
    free(buf);
    qDebug()<<"send one capture finish";

}

VerifyThread::VerifyThread(QSharedPointer<DriverManger> spDriver)
    :m_wpDriver(spDriver)
    ,m_spCapture(new cv::VideoCapture)
    ,m_bRun(false)
{

}

void VerifyThread::Start(QString actionId, QVector<float*> charas){

    this->m_actionId=actionId;
    this->m_bRun=true;
    for(int i=0;i<m_charaDatas.size();i++){
        if(m_charaDatas[i]!=nullptr){
            free(m_charaDatas[i]);
        }
    }
    this->m_charaDatas.clear();
    qDebug()<<"charas size"<<charas.size();
    this->m_charaDatas=charas;

    this->start();
}

void VerifyThread::run(){

    qDebug()<<"Verify run";

    QSharedPointer<DriverManger> driverManger;
    if(m_wpDriver.isNull()){
        return;
    }else{
        driverManger=m_wpDriver.lock();
    }

    QList<QCameraInfo> cameraInfo = QCameraInfo::availableCameras();
    bool bOpen = false;
    QList<QCameraInfo>::iterator iter=cameraInfo.begin();
    while(iter != cameraInfo.end()){
        bOpen = m_spCapture->open(iter->deviceName().toStdString());
        if(bOpen){
            break;
        }
        iter++;
    }
    if(!bOpen){
        qDebug()<<"open camera fail";
        driverManger->processStatus(m_actionId,FaceEnrollException);
        return;
    }

    bool bTrack=true;
    m_spCapture->set( cv::CAP_PROP_FRAME_WIDTH, 800 );
    m_spCapture->set( cv::CAP_PROP_FRAME_HEIGHT, 600 );
    cv::Mat mat;

    while(m_bRun){
        bool bRead=m_spCapture->read(mat);
        if(!bRead){
            driverManger->processStatus(m_actionId,FaceVerifyException);
            return;
        }
        if(mat.channels() == 4){
            cv::cvtColor(mat, mat, cv::COLOR_RGBA2BGR);
        }
        SeetaImageData image;
        image.height = mat.rows;
        image.width = mat.cols;
        image.channels = mat.channels();
        image.data = mat.data;

        if(!bTrack){
            continue;
        }

        ModelManger::getSingleInstanceModel().setFaceTrackSize(image.width,image.height);
        auto faces= ModelManger::getSingleInstanceModel().getFaceTracker()->Track(image);
        if(faces.size==0){
            driverManger->processStatus(m_actionId,FaceVerifyNoFace);
        }else if(faces.size>1)
        {
            driverManger->processStatus(m_actionId,FaceVerifyTooManyFace);
        }
        else
        {
            qDebug()<<"faces :"<<faces.size;

            auto points =  ModelManger::getSingleInstanceModel().getFaceLandmarker()->mark(image,faces.data[0].pos);
            ModelManger::getSingleInstanceModel().getQualityAssessor()->feed(image, faces.data[0].pos, points.data(), 5);

            seeta::QualityResult quality = ModelManger::getSingleInstanceModel().getQualityAssessor()->query(seeta::BRIGHTNESS);
            if(quality.level<seeta::MEDIUM){
                driverManger->processStatus(m_actionId,FaceVerifyBrightness);
                ModelManger::getSingleInstanceModel().getFaceTracker()->Reset();
                continue;
            }
            quality= ModelManger::getSingleInstanceModel().getQualityAssessor()->query(seeta::RESOLUTION);
            if(quality.level<seeta::MEDIUM){
                driverManger->processStatus(m_actionId,FaceVerifyFaceNotClear);
                ModelManger::getSingleInstanceModel().getFaceTracker()->Reset();
                continue;
            }
            quality= ModelManger::getSingleInstanceModel().getQualityAssessor()->query(seeta::CLARITY);
            if(quality.level<seeta::MEDIUM){
                driverManger->processStatus(m_actionId,FaceVerifyFaceNotClear);
                ModelManger::getSingleInstanceModel().getFaceTracker()->Reset();
                continue;
            }

            quality= ModelManger::getSingleInstanceModel().getQualityAssessor()->query(seeta::INTEGRITY);
            if(quality.level<seeta::MEDIUM){
                driverManger->processStatus(m_actionId,FaceVerifyFaceTooBig);
                ModelManger::getSingleInstanceModel().getFaceTracker()->Reset();
                continue;
            }
            quality= ModelManger::getSingleInstanceModel().getQualityAssessor()->query(seeta::POSE_EX);
            if(quality.level<seeta::MEDIUM){
                driverManger->processStatus(m_actionId,FaceVerifyFaceNotCenter);
                ModelManger::getSingleInstanceModel().getFaceTracker()->Reset();
                continue;
            }

            auto status= ModelManger::getSingleInstanceModel().getFaceAntiSpoofing()->Predict(image,faces.data[0].pos, points.data());
            if( status == seeta::FaceAntiSpoofing::SPOOF)
            {
                driverManger->processStatus(m_actionId,FaceVerifyNotRealHuman);
                bTrack=false;
                ModelManger::getSingleInstanceModel().getFaceTracker()->Reset();
                continue;

            }else {
                qDebug()<<"antispoofing ok";
            }

            seeta::ImageData cropface = ModelManger::getSingleInstanceModel().getFaceRecognizer()->CropFaceV2(image,  points.data());
            int size=ModelManger::getSingleInstanceModel().getFaceRecognizer()->GetExtractFeatureSize();
            float* features=static_cast<float*>(malloc(sizeof(float)*size));
            bool bFound=ModelManger::getSingleInstanceModel().getFaceRecognizer()->ExtractCroppedFace(cropface, features);
            if (bFound){
                qDebug()<<"ExtractCroppedFace ok"<<features;
                for(auto iter:m_charaDatas){
                    float score=ModelManger::getSingleInstanceModel().getFaceRecognizer()->CalculateSimilarity(features,iter);
                    qDebug()<<"score :"<<score;
                    if(score>0.62){
                        driverManger->processStatus(m_actionId,FaceVerifySuccess);
                        bTrack=false;
                        break;
                    }
                }

            }

        }
    }

    m_spCapture->release();
}

void VerifyThread::Stop(){

    for(int i=0;i<m_charaDatas.size();i++){
        if(m_charaDatas[i]!=nullptr){
            free(m_charaDatas[i]);
        }
    }
    this->m_charaDatas.clear();

    this->m_bRun=false;

}


