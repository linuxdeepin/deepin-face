#ifndef CHARADATAMANGER_H
#define CHARADATAMANGER_H

#include <utility>
#include <QDBusInterface>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QSharedPointer>

#include <memory>

class CharaDataManger
{
public:
    CharaDataManger();
    void loadCharaData();
    bool insertCharaDate(QString chara, float *data, const int &size);
    bool deleteCharaData(QString chara);
    QStringList getCharaList();
    QVector<float *> getCharaData(QStringList charas);
    QMap<QString, QPair<int, float *>> &getCharaDataMap();

private:
    bool saveCharaData();
    bool setCharaToUadp(QByteArray &dataArray);
    QByteArray getCharaFromUadp();

private:
    QMap<QString, QPair<int, float *>> m_charaData;
    QSharedPointer<QDBusInterface> m_spUadpInterface;
};

#endif // CHARADATAMANGER_H
