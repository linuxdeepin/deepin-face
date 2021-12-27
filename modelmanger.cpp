#include "modelmanger.h"

// /usr/share/face-models/
static std::string modelPath = "/usr/share/seetaface-models/";
ModelManger::ModelManger()
    : m_bAvaliable(false)
{}

ModelManger &ModelManger::getSingleInstanceModel()
{
    static ModelManger model;
    return model;
}

void ModelManger::load()
{
    seeta::ModelSetting faceDetectorModel;
    faceDetectorModel.append(modelPath + "face_detector.csta");
    faceDetectorModel.set_device(seeta::ModelSetting::CPU);
    faceDetectorModel.set_id(0);
    this->m_spFaceDetector = QSharedPointer<seeta::FaceDetector>(
        new seeta::FaceDetector(faceDetectorModel));

    seeta::ModelSetting faceLandMarkModel;
    faceLandMarkModel.append(modelPath + "face_landmarker_pts5.csta");
    faceLandMarkModel.set_device(seeta::ModelSetting::CPU);
    faceLandMarkModel.set_id(0);
    this->m_spFaceLandmarker = QSharedPointer<seeta::FaceLandmarker>(
        new seeta::FaceLandmarker(faceLandMarkModel));
    faceLandMarkModel.clear();

    seeta::ModelSetting faceAntiSpooModel;
    faceAntiSpooModel.append(modelPath + "fas_first.csta");
    faceAntiSpooModel.append(modelPath + "fas_second.csta");
    faceAntiSpooModel.set_device(seeta::ModelSetting::CPU);
    faceAntiSpooModel.set_id(0);
    this->m_spFaceAntiSpoof = QSharedPointer<seeta::FaceAntiSpoofing>(
        new seeta::FaceAntiSpoofing(faceAntiSpooModel));
    this->m_spFaceAntiSpoof->SetThreshold(float(0.3), float(0.8));
    faceAntiSpooModel.clear();

    seeta::ModelSetting faceRecognizerModel;
    faceRecognizerModel.append(modelPath + "face_recognizer.csta");
    faceRecognizerModel.set_device(seeta::ModelSetting::CPU);
    faceRecognizerModel.set_id(0);
    this->m_spFaceRecognizer = QSharedPointer<seeta::FaceRecognizer>(
        new seeta::FaceRecognizer(faceRecognizerModel));
    faceRecognizerModel.clear();

    seeta::ModelSetting faceTrackerModel;
    faceTrackerModel.append(modelPath + "face_detector.csta");
    faceTrackerModel.set_device(seeta::ModelSetting::CPU);
    faceTrackerModel.set_id(0);
    this->m_spFaceTracker = QSharedPointer<seeta::FaceTracker>(
        new seeta::FaceTracker(faceTrackerModel, 1920, 1080));
    this->m_spFaceTracker->SetVideoSize(800, 600);
    this->m_spFaceTracker->SetMinFaceSize(100);
    this->m_spFaceTracker->SetThreshold(float(0.8));

    seeta::ModelSetting facePoseModel;
    facePoseModel.append(modelPath + "pose_estimation.csta");
    facePoseModel.set_device(SEETA_DEVICE_CPU);
    facePoseModel.set_id(0);
    this->m_spQualityOfPose = QSharedPointer<seeta::QualityOfPoseEx>(
        new seeta::QualityOfPoseEx(facePoseModel));

    this->m_spQualityAssessor = QSharedPointer<seeta::QualityAssessor>(new seeta::QualityAssessor);
    this->m_spQualityAssessor->add_rule(seeta::INTEGRITY);
    this->m_spQualityAssessor->add_rule(seeta::RESOLUTION);
    this->m_spQualityAssessor->add_rule(seeta::BRIGHTNESS);
    this->m_spQualityAssessor->add_rule(seeta::CLARITY);
    this->m_spQualityAssessor->add_rule(seeta::POSE_EX, this->m_spQualityOfPose.get(), true);

    m_bAvaliable = true;
}
void ModelManger::unLoad()
{
    m_bAvaliable = false;

    this->m_spFaceTracker.clear();
    this->m_spFaceDetector.clear();
    this->m_spFaceAntiSpoof.clear();
    this->m_spFaceLandmarker.clear();
    this->m_spFaceRecognizer.clear();
    this->m_spQualityAssessor->remove_rule(seeta::POSE_EX);
    this->m_spQualityAssessor.clear();
    this->m_spQualityOfPose.clear();
}

bool ModelManger::avaliable()
{
    return this->m_bAvaliable;
}

QSharedPointer<seeta::FaceDetector> &ModelManger::getFaceDetector()
{
    return this->m_spFaceDetector;
}

QSharedPointer<seeta::FaceLandmarker> &ModelManger::getFaceLandmarker()
{
    return this->m_spFaceLandmarker;
}

QSharedPointer<seeta::FaceAntiSpoofing> &ModelManger::getFaceAntiSpoofing()
{
    return this->m_spFaceAntiSpoof;
}

QSharedPointer<seeta::FaceRecognizer> &ModelManger::getFaceRecognizer()
{
    return this->m_spFaceRecognizer;
}

QSharedPointer<seeta::FaceTracker> &ModelManger::getFaceTracker()
{
    return this->m_spFaceTracker;
}

QSharedPointer<seeta::QualityAssessor> &ModelManger::getQualityAssessor()
{
    return this->m_spQualityAssessor;
}

QSharedPointer<seeta::QualityOfPoseEx> &ModelManger::getQualityOfPoseEx()
{
    return this->m_spQualityOfPose;
}

void ModelManger::setFaceTrackSize(int width, int height)
{
    this->m_spFaceTracker->SetVideoSize(width, height);
}
