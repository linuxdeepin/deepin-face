QT += gui dbus dtkcore multimedia

CONFIG += c++11 link_pkgconfig
CONFIG -= app_bundle
TARGET = deepin-face
TEMPLATE = app
# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp \
    dbusfaceservice.cpp \
    drivermanger.cpp \
    modelmanger.cpp \
    workmodule.cpp \
    charadatamanger.cpp

# Default rules for deployment.
#qnx: target.path = /tmp/$${TARGET}/bin
#else: unix:!android: target.path = /opt/$${TARGET}/bin
#!isEmpty(target.path): INSTALLS += target

HEADERS += \
    dbusfaceservice.h \
    drivermanger.h \
    modelmanger.h \
    workmodule.h \
    charadatamanger.h \
    definehead.h


INCLUDEPATH += /usr/include/opencv2 \
               /usr/include/c++/8

LIBS += -L/usr/lib/auto -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_videoio -lopencv_imgcodecs \
        -L/usr/lib -lSeetaFaceDetector600 -lSeetaFaceLandmarker600 \
        -lSeetaFaceAntiSpoofingX600d -lSeetaFaceTracking600 -lSeetaFaceRecognizer610 \
        -lSeetaQualityAssessor300 -lSeetaPoseEstimation600 -lSeetaAuthorize -ltennis

QMAKE_RPATHDIR += /usr/lib/auto/

isEmpty(PREFIX){
    PREFIX = /usr
}

target.path = $${PREFIX}/libexec

dbus_conf.path = $$PREFIX/share/dbus-1/system.d/
dbus_conf.files = $$PWD/msic/dbus-conf/com.deepin.face.conf

dbus_server.path = $$PREFIX/share/dbus-1/system-services/
dbus_server.files = $$PWD/msic/dbus-services/com.deepin.face.service

systemd.path = /lib/systemd/system/
systemd.files = $$PWD/msic/systemd/deepin-face.service

face_conf.path = $$PREFIX/share/deepin-authentication/interfaces/
face_conf.files = $$PWD/msic/deepin-face-driver.json

INSTALLS +=target   \
           dbus_conf \
           dbus_server\
           systemd  \
           face_conf
