// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef DEFINEHEAD_H
#define DEFINEHEAD_H

#define SERVERNAME "com.deepin.face"
#define SERVERPATH "/com/deepin/face"
#define SERVERINTERFACE "com.deepin.face"
#define EMPTYCHARATYPE 0
#define FACECHARATYPE 4
// 轻量级人脸识别模型阈值
#define FACETlIGHTHRESHOLD 0.55

enum ActionType {
    Enroll,
    Verify
};

enum EnrollStatus {
    FaceEnrollSuccess,
    FaceEnrollNotRealHuman,
    FaceEnrollFaceNotCenter,
    FaceEnrollFaceTooBig,
    FaceEnrollFaceTooSmall,
    FaceEnrollNoFace,
    FaceEnrollTooManyFace,
    FaceEnrollFaceNotClear,
    FaceEnrollBrightness,
    FaceEnrollFaceCovered,
    FaceEnrollCancel,
    FaceEnrollError,
    FaceEnrollException
};

enum VerifyStatus {
    FaceVerifySuccess,
    FaceVerifyNotRealHuman,
    FaceVerifyFaceNotCenter,
    FaceVerifyFaceTooBig,
    FaceVerifyFaceTooSmall,
    FaceVerifyNoFace,
    FaceVerifyTooManyFace,
    FaceVerifyFaceNotClear,
    FaceVerifyBrightness,
    FaceVerifyFaceCovered,
    FaceVerifyCancel,
    FaceVerifyError,
    FaceVerifyException
};

#endif // DEFINEHEAD_H
