#ifndef CHARADATAMANGER_H
#define CHARADATAMANGER_H

#include <QObject>
#include <QMap>
#include <memory>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <utility>
#include <QFile>
#include <QDir>

class CharaDataManger
{
public:
    CharaDataManger();
    void loadCharaData();
    bool insertCharaDate(QString chara, float* data, const int& size);
    bool deleteCharaData(QString chara);
    QStringList getCharaList();
    QVector<float*> getCharaData(QStringList charas);
    QMap<QString,QPair<int,float*>>& getCharaDataMap();
private:
    void saveCharaData();

private:
     QMap<QString,QPair<int,float*>> m_charaData;
     const QString                   m_charaFilePath;
};

#endif // CHARADATAMANGER_H
