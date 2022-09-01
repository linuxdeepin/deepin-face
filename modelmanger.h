// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef MODELMANGER_H
#define MODELMANGER_H

#include "drivermanger.h"
#include "seeta/Common/Struct.h"

#include <string>
#include <QObject>

class ModelManger
{
private:
    ModelManger();
    ModelManger(const ModelManger &) = delete;
    ModelManger &operator=(const ModelManger &model) = delete;

public:
    static ModelManger &getSingleInstanceModel();
    void load();
    void unLoad();
    bool avaliable();
    QSharedPointer<seeta::FaceDetector> &getFaceDetector();
    QSharedPointer<seeta::FaceLandmarker> &getFaceLandmarker();
    QSharedPointer<seeta::FaceAntiSpoofing> &getFaceAntiSpoofing();
    QSharedPointer<seeta::FaceRecognizer> &getFaceRecognizer();
    QSharedPointer<seeta::FaceTracker> &getFaceTracker();
    QSharedPointer<seeta::QualityAssessor> &getQualityAssessor();
    QSharedPointer<seeta::QualityOfPoseEx> &getQualityOfPoseEx();
    void setFaceTrackSize(int height, int width);

private:
    QSharedPointer<seeta::FaceDetector> m_spFaceDetector;       // 人脸探测器
    QSharedPointer<seeta::FaceLandmarker> m_spFaceLandmarker;   // 关键点定位器
    QSharedPointer<seeta::FaceAntiSpoofing> m_spFaceAntiSpoof;  // 活体检测识别器
    QSharedPointer<seeta::FaceRecognizer> m_spFaceRecognizer;   // 人脸特征提取、对比器
    QSharedPointer<seeta::FaceTracker> m_spFaceTracker;         // 人脸跟踪
    QSharedPointer<seeta::QualityAssessor> m_spQualityAssessor; // 评估器
    QSharedPointer<seeta::QualityOfPoseEx> m_spQualityOfPose;   // 姿态评估
    bool m_bAvaliable;
};

#endif // MODELMANGER_H
