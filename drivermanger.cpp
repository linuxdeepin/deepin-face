#include "drivermanger.h"
#include "modelmanger.h"
#include <sys/types.h>
#include <sys/socket.h>

DriverManger::DriverManger()
    :m_spCharaDataManger(new CharaDataManger)
{

}
void DriverManger::init(){

    this->m_spFileWatch       = QSharedPointer<QFileSystemWatcher>(new QFileSystemWatcher(this));
    this->m_spErollthread = QSharedPointer<ErollThread>(new ErollThread(sharedFromThis()));
    this->m_spVerifyThread = QSharedPointer<VerifyThread>(new VerifyThread(sharedFromThis()));
    this->m_spCharaDataManger->loadCharaData();
    this->m_bClaim = false;
    this->m_charalist = this->m_spCharaDataManger->getCharaList();
    this->m_charaType = FACECHARATYPE;
    this->m_spFileWatch->addPath("/dev/");

    // 暂不支持热插拔
    connect(m_spFileWatch.get(),&QFileSystemWatcher::directoryChanged,this,&DriverManger::onDirectoryChanged);

    onDirectoryChanged("/dev/");
}

QDBusUnixFileDescriptor DriverManger::enrollStart(QString chara, qint32 charaType, QString actionId, ErrMsgInfo& errMsgInfo){

    if(m_bClaim){
        errMsgInfo.errType = QDBusError::Failed;
        errMsgInfo.msg = "face claim";
        return  QDBusUnixFileDescriptor(-1);
    }

    if(this->m_charaType != charaType){
        errMsgInfo.errType = QDBusError::InvalidArgs;
        errMsgInfo.msg = "charaType not match";
        return  QDBusUnixFileDescriptor(-1);
    }

    auto iter = std::find(m_charalist.begin(),m_charalist.end(),chara);
    if(iter != m_charalist.end()){
        errMsgInfo.errType = QDBusError::Failed;
        errMsgInfo.msg = "face had same chara";
        return  QDBusUnixFileDescriptor(-1);
    }

    int fd[2];
    int r = socketpair(AF_UNIX, SOCK_STREAM|SOCK_NONBLOCK, 0, fd);
    if ( r < 0 ) {
        errMsgInfo.errType = QDBusError::Failed;
        errMsgInfo.msg = "create socketpair err";
        return  QDBusUnixFileDescriptor(-1);
    }
    ModelManger::getSingleInstanceModel().load();

    m_bClaim = true;

    ActionInfo info;
    info.chara = chara;
    info.actionType = Enroll;
    m_actionMap[actionId] = info;

    m_spErollthread->Start(actionId,fd[1]);

    return QDBusUnixFileDescriptor(fd[0]);
}

void DriverManger::enrollStop(QString actionId, ErrMsgInfo& errMsgInfo){

    if(m_actionMap.count(actionId) != 1){
        errMsgInfo.errType = QDBusError::InvalidArgs;
        errMsgInfo.msg = "not found action";
        return;
    }

    m_bClaim=false;

    auto actionInfo = m_actionMap[actionId];
    if(actionInfo.status == FaceEnrollSuccess){
        m_spCharaDataManger->insertCharaDate(actionInfo.chara,actionInfo.faceChara,actionInfo.size);
        if(actionInfo.faceChara != nullptr){
            free(actionInfo.faceChara);
        }
    }

    ModelManger::getSingleInstanceModel().unLoad();

    m_spErollthread->Stop();
    m_actionMap.remove(actionId);
    auto charaList = m_spCharaDataManger->getCharaList();
    setCharaList(charaList);

    qDebug()<<"EnrollStop finish";
}

QDBusUnixFileDescriptor DriverManger::verifyStart(QStringList charas, QString actionId, ErrMsgInfo& errMsgInfo){

    if(m_bClaim){
        errMsgInfo.errType = QDBusError::Failed;
        errMsgInfo.msg = "face claim";
        return  QDBusUnixFileDescriptor(-1);
    }

    QVector<float*>  faceCharas = m_spCharaDataManger->getCharaData(charas);

    m_bClaim = true;
    ActionInfo info;
    info.actionType = Verify;
    m_actionMap[actionId] = info;
    qDebug()<<"claim=true;";
    ModelManger::getSingleInstanceModel().load();

    m_spVerifyThread->Start(actionId,faceCharas);

    return QDBusUnixFileDescriptor(0);
}
void DriverManger::verifyStop(QString actionId, ErrMsgInfo& errMsgInfo){

    if(m_actionMap.count(actionId) != 1){
        errMsgInfo.errType = QDBusError::InvalidArgs;
        errMsgInfo.msg = "not found action";
        return;
    }

    m_bClaim = false;
    ModelManger::getSingleInstanceModel().unLoad();
    m_spVerifyThread->Stop();
    m_actionMap.remove(actionId);
}

void DriverManger::deleteChara(QString chara,ErrMsgInfo& errMsgInfo){

    bool bSuccess = m_spCharaDataManger->deleteCharaData(chara);
    if(!bSuccess){
        errMsgInfo.errType = QDBusError::Failed;
        errMsgInfo.msg = "delete error";
        return;
    }

    auto charaList = m_spCharaDataManger->getCharaList();
    setCharaList(charaList);
}
bool DriverManger::getClaim(){

    return this->m_bClaim;
}
void DriverManger::setClaim(bool bClaim){

     this->m_bClaim=bClaim;
}
qint32 DriverManger::getCharaType(){

    return this->m_charaType;
}
void DriverManger::setCharaType(qint32 qCharaType){

    if(this->m_charaType != qCharaType){
        qDebug()<<"set chara Type"<<qCharaType;
        this->m_charaType = qCharaType;
        QVariantMap qChangedProps;
        qChangedProps.insert("CharaType",this->m_charaType);
        emitPropertiesChanged(qChangedProps);
    }
}
QStringList DriverManger::getCharaList(){

    return this->m_charalist;
}
void  DriverManger::setCharaList(QStringList lCharaList){

    if(this->m_charalist.size() != lCharaList.size()){
        this->m_charalist = lCharaList;
        QVariantMap qChangedProps;
        qChangedProps.insert("List",this->m_charalist);

        emitPropertiesChanged(qChangedProps);
    }
}

void DriverManger::processStatus(QString actionId,qint32 status,float* faceChara,int size){

    if(m_actionMap.count(actionId) != 1){
        return;
    }
    auto& actionInfo = m_actionMap[actionId];
    actionInfo.status = status;
    if(faceChara != nullptr)
    {
        actionInfo.faceChara = static_cast<float*>(malloc(sizeof(float)*static_cast<unsigned long>(size)));
        for(int i=0;i<size;i++){
            actionInfo.faceChara[i] = faceChara[i];
        }
        actionInfo.size = size;
    }

    QString msg=getStatusMsg(actionInfo.actionType,status);
    emitStatus(actionInfo.actionType,actionId,status,msg);

}

void DriverManger::emitStatus(ActionType action,QString actionId, qint32 status,QString msg){

    qDebug()<<"emitStatus action"<<actionId<<"status"<<status;

    QString emitSignal;
    if(action == Enroll){
        emitSignal="EnrollStatus";
    }else {
        emitSignal="VerifyStatus";
    }
    QDBusMessage signal = QDBusMessage::createSignal(
     SERVERPATH,
     SERVERINTERFACE,
     emitSignal);
    signal<<actionId;
    signal<<status;
    signal<<msg;
    QDBusConnection::systemBus().send(signal);
}

void DriverManger::emitPropertiesChanged(QVariantMap& qChangedProps){

    qDebug()<<"emitPropertiesChanged";

    QDBusMessage signal = QDBusMessage::createSignal(
     SERVERPATH,
     "org.freedesktop.DBus.Properties",
     "PropertiesChanged");
    signal << SERVERINTERFACE;
    signal << qChangedProps;
    QStringList invalidatedProps;
    signal << invalidatedProps;
    QDBusConnection::systemBus().send(signal);

}


void DriverManger::onDirectoryChanged(const QString &path){

    qDebug()<<"HandleFileChanged"<<path;

    QDir dir(path);
    if(!dir.exists()){

       qDebug()<<"dir "<<path<<"not exist";
       return;
    }
    //获取filePath下所有系统文件
    dir.setFilter(QDir::System);
    QFileInfoList filelist = dir.entryInfoList();
    bool bFound = false;
    for(int i=0;i<filelist.size();i++){
        if(filelist[i].fileName().contains("video")){
            bFound = true;
            break;
        }
    }

    if(bFound){
        setCharaType(FACECHARATYPE);
    }else {
        setCharaType(EMPTYCHARATYPE);
    }
}

QString DriverManger::getStatusMsg(ActionType actionType,qint32 status){

    QString retMsg;
    if(actionType == Enroll){
       EnrollStatus tempStatus = EnrollStatus(status);
       switch (tempStatus) {
       case FaceEnrollSuccess:
           retMsg = "Enroll Success";
           break;
       case FaceEnrollNotRealHuman:
           retMsg = "Enroll Not Real Human";
           break;
       case FaceEnrollFaceNotCenter:
           retMsg = "Enroll Not Center";
           break;
       case FaceEnrollFaceTooBig:
           retMsg = "Enroll Too Big";
           break;
       case FaceEnrollFaceTooSmall:
           retMsg = "Enroll Too Small";
           break;
       case FaceEnrollNoFace:
           retMsg = "Enroll No Face";
           break;
       case FaceEnrollTooManyFace:
           retMsg = "Enroll Too Many Face";
           break;
       case FaceEnrollFaceNotClear:
           retMsg = "Enroll Not Clear";
           break;
       case FaceEnrollBrightness:
           retMsg = "Enroll Brightness";
           break;
       case FaceEnrollFaceCovered:
           retMsg = "Enroll Covered";
           break;
       case FaceEnrollCancel:
           retMsg = "Enroll Cancel";
           break;
       case FaceEnrollError:
           retMsg = "Enroll Error";
           break;
       case FaceEnrollException:
           retMsg = "Enroll Exception";
           break;
       }
    }else if(actionType == Verify){
        VerifyStatus tempStatus = VerifyStatus(status);
        switch (tempStatus) {
        case FaceVerifySuccess:
            retMsg = "Verify Success";
            break;
        case FaceVerifyNotRealHuman:
            retMsg = "Verify Not Real Human";
            break;
        case FaceVerifyFaceNotCenter:
            retMsg = "Verify Not Center";
            break;
        case FaceVerifyFaceTooBig:
            retMsg = "Verify Too Big";
            break;
        case FaceVerifyFaceTooSmall:
            retMsg = "Verify Too Small";
            break;
        case FaceVerifyNoFace:
            retMsg = "Verify No Face";
            break;
        case FaceVerifyTooManyFace:
            retMsg = "Verify Too Many Face";
            break;
        case FaceVerifyFaceNotClear:
            retMsg = "Verify Not Clear";
            break;
        case FaceVerifyBrightness:
            retMsg = "Verify Brightness";
            break;
        case FaceVerifyFaceCovered:
            retMsg = "Verify Covered";
            break;
        case FaceVerifyCancel:
            retMsg = "Verify Cancel";
            break;
        case FaceVerifyError:
            retMsg = "Verify Error";
            break;
        case FaceVerifyException:
            retMsg = "Verify Exception";
            break;
        }
    }

    return retMsg;
}
