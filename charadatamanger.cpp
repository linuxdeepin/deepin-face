#include "charadatamanger.h"
#include "definehead.h"

CharaDataManger::CharaDataManger()
    :m_charaFilePath(CHARAFILEPATH)
{
    m_charaData.clear();
}

bool CharaDataManger::insertCharaDate(QString chara, float* data, const int& size){

    float* faceData=static_cast<float*>(malloc(sizeof(float)*size));
    for(int i=0;i<size;i++){
        faceData[i]=data[i];
    }
    qDebug() << "insertCharaDate chara"<<chara<<" data"<<data<<" size"<<size;
    m_charaData[chara]=qMakePair(size,faceData);

    saveCharaData();

    return true;
}

bool CharaDataManger::deleteCharaData(QString chara){

    if(m_charaData.count(chara)==0){
        return false;
    }
    if(m_charaData[chara].second!=nullptr){
        qDebug()<<"free charaData[chara].second";
        free(m_charaData[chara].second);
    }
    m_charaData.remove(chara);

    saveCharaData();

    return true;
}

void CharaDataManger::loadCharaData(){

    QFile file(m_charaFilePath);
    if (!file.open(QIODevice::Text | QIODevice::ReadWrite)) {
        file.close();
        return;
    }
    QByteArray array = file.readAll();
    file.close();
    if(array.isEmpty()){
        return;
    }
    QJsonParseError jsonParseError;
    QJsonDocument jsonDocument(QJsonDocument::fromJson(array, &jsonParseError));
    if(QJsonParseError::NoError != jsonParseError.error){
       qDebug() << QString("JsonParseError: %1").arg(jsonParseError.errorString());
       return;
    }
    QJsonObject rootObject = jsonDocument.object();
    QStringList rootKeys = rootObject.keys();
    for(auto chara:rootKeys){
        qDebug()<<"chara :"<<chara;
        QJsonValue rootJsonValue = rootObject.value(chara);
        QJsonObject charaObject= rootJsonValue.toObject();
        int faceSize=0;
        if (charaObject.contains("faceCharaSize")){
            faceSize=charaObject.value("faceCharaSize").toInt();
            qDebug()<<"faceCharaSize :"<<faceSize;
        }
        QJsonObject obj;
        QJsonArray jsonArray;
        if (charaObject.contains("faceChara")){
            jsonArray=charaObject.value("faceChara").toArray();
        }

        float* faceChara=static_cast<float*>(malloc(sizeof(float)*faceSize));
        for(int i=0;i<jsonArray.size();i++){
            faceChara[i]=jsonArray[i].toString().toFloat();
        }
        m_charaData[chara]=QPair<int,float*>(faceSize,faceChara);
    }
}

void CharaDataManger::saveCharaData(){

    qDebug() << "saveCharaData charaData" <<m_charaData.size();
    QJsonDocument rootDoc;
    QJsonObject rootObject;
    QMap<QString,QPair<int,float*>>::iterator iter=m_charaData.begin();
    while(iter!=m_charaData.end()){

        QJsonObject charaObject;
        charaObject.insert("faceCharaSize",iter.value().first);
        QJsonArray arr;
        for (int i=0;i<iter.value().first;i++) {
            arr.push_back(QString("%1").arg(iter.value().second[i]));
        }
        charaObject.insert("faceChara",arr);
        rootObject.insert(iter.key(),charaObject);
        iter++;
    }
    rootDoc.setObject(rootObject);
    QByteArray fileText = rootDoc.toJson(QJsonDocument::Indented);   //标准JSON格式

    QFileInfo fileInfo(m_charaFilePath);
    QDir dir(fileInfo.dir());
    if(!dir.exists()){
          dir.mkdir(fileInfo.dir().path());
    }

    QFile file(m_charaFilePath);
    if (file.open(QIODevice::Text | QIODevice::WriteOnly | QIODevice::Truncate)){
        file.write(fileText);
        file.close();
    } 

}

QStringList CharaDataManger::getCharaList(){

    QStringList ret;
    QMap<QString,QPair<int,float*>>::iterator iter=m_charaData.begin();
    while(iter!=m_charaData.end()){
        ret.push_back(iter.key());
        iter++;
    }

    return ret;
}

QVector<float*> CharaDataManger::getCharaData(QStringList charas){

    QVector<float*> ret;
    float* tempChara;
    for(auto iter:charas){
        if(m_charaData.count(iter)!=0){
            tempChara=static_cast<float*>(malloc(sizeof(float)*m_charaData[iter].first));
            for(int i=0;i<m_charaData[iter].first;i++){
                tempChara[i]=m_charaData[iter].second[i];
            }
            ret.push_back(tempChara);
        }
    }

    return ret;

}
QMap<QString,QPair<int,float*>>& CharaDataManger::getCharaDataMap(){

    return m_charaData;
}
