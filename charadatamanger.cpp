#include "charadatamanger.h"
#include "definehead.h"

CharaDataManger::CharaDataManger()
    : m_spUadpInterface(new QDBusInterface("org.deepin.dde.Uadp1",
                                           "/org/deepin/dde/Uadp1",
                                           "org.deepin.dde.Uadp1",
                                           QDBusConnection::systemBus()))
{
    m_charaData.clear();
}

bool CharaDataManger::insertCharaDate(QString chara, float *data, const int &size)
{
    float *faceData = static_cast<float *>(malloc(sizeof(float) * static_cast<unsigned long>(size)));
    for (int i = 0; i < size; i++) {
        faceData[i] = data[i];
    }
    qDebug() << "insertCharaDate chara" << chara << " data" << data << " size" << size;
    m_charaData[chara] = qMakePair(size, faceData);

    return saveCharaData();
}

bool CharaDataManger::deleteCharaData(QString chara)
{
    if (m_charaData.count(chara) == 0) {
        return false;
    }
    if (m_charaData[chara].second != nullptr) {
        qDebug() << "free charaData[chara].second";
        free(m_charaData[chara].second);
    }
    m_charaData.remove(chara);

    return saveCharaData();
}

void CharaDataManger::loadCharaData()
{
    QByteArray dataArray = getCharaFromUadp();

    if (dataArray.size() == 0) {
        return;
    }

    QJsonParseError jsonParseError;
    QJsonDocument jsonDocument(QJsonDocument::fromJson(dataArray, &jsonParseError));
    if (QJsonParseError::NoError != jsonParseError.error) {
        qDebug() << QString("JsonParseError: %1").arg(jsonParseError.errorString());
        return;
    }
    QJsonObject rootObject = jsonDocument.object();
    QStringList rootKeys = rootObject.keys();
    for (auto chara : rootKeys) {
        qDebug() << "chara :" << chara;
        QJsonValue rootJsonValue = rootObject.value(chara);
        QJsonObject charaObject = rootJsonValue.toObject();
        int faceSize = 0;
        if (charaObject.contains("faceCharaSize")) {
            faceSize = charaObject.value("faceCharaSize").toInt();
            qDebug() << "faceCharaSize :" << faceSize;
        }
        QJsonObject obj;
        QJsonArray jsonArray;
        if (charaObject.contains("faceChara")) {
            jsonArray = charaObject.value("faceChara").toArray();
        }

        float *faceChara = static_cast<float *>(
                               malloc(sizeof(float) * static_cast<unsigned long>(faceSize)));
        for (int i = 0; i < jsonArray.size(); i++) {
            faceChara[i] = jsonArray[i].toString().toFloat();
        }
        m_charaData[chara] = QPair<int, float *>(faceSize, faceChara);
    }
}

bool CharaDataManger::saveCharaData()
{
    qDebug() << "saveCharaData charaData" << m_charaData.size();

    QJsonDocument rootDoc;
    QJsonObject rootObject;
    QMap<QString, QPair<int, float *>>::iterator iter = m_charaData.begin();
    while (iter != m_charaData.end()) {
        QJsonObject charaObject;
        charaObject.insert("faceCharaSize", iter.value().first);
        QJsonArray arr;
        for (int i = 0; i < iter.value().first; i++) {
            arr.push_back(QString("%1").arg(static_cast<double>(iter.value().second[i])));
        }
        charaObject.insert("faceChara", arr);
        rootObject.insert(iter.key(), charaObject);
        iter++;
    }
    rootDoc.setObject(rootObject);
    QByteArray dataArray = rootDoc.toJson(QJsonDocument::Indented); //标准JSON格式

    return setCharaToUadp(dataArray);
}

QStringList CharaDataManger::getCharaList()
{
    QStringList ret;
    QMap<QString, QPair<int, float *>>::iterator iter = m_charaData.begin();
    while (iter != m_charaData.end()) {
        ret.push_back(iter.key());
        iter++;
    }

    return ret;
}

QVector<float *> CharaDataManger::getCharaData(QStringList charas)
{
    QVector<float *> ret;
    float *tempChara;
    for (auto iter : charas) {
        if (m_charaData.count(iter) != 0) {
            tempChara = static_cast<float *>(
                            malloc(sizeof(float) * static_cast<unsigned long>(m_charaData[iter].first)));
            for (int i = 0; i < m_charaData[iter].first; i++) {
                tempChara[i] = m_charaData[iter].second[i];
            }
            ret.push_back(tempChara);
        }
    }

    return ret;
}

QMap<QString, QPair<int, float *>> &CharaDataManger::getCharaDataMap()
{
    return m_charaData;
}

bool CharaDataManger::setCharaToUadp(QByteArray &dataArray)
{
    qDebug() << "save chara to uadp";

    QDBusMessage msg = m_spUadpInterface->call("Set", "face", dataArray);
    if (msg.type() == QDBusMessage::ErrorMessage) {
        qDebug() << msg.errorMessage();
        return false;
    }

    return true;
}

QByteArray CharaDataManger::getCharaFromUadp()
{
    QByteArray dataArray;
    QDBusMessage msg = m_spUadpInterface->call("Get", "face");
    if (msg.type() == QDBusMessage::ErrorMessage) {
        qDebug() << msg.errorMessage();

        return dataArray;
    }

    if (msg.arguments().empty()) {
        return dataArray;
    }

    return msg.arguments()[0].toByteArray();
}
