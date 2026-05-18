#include "utils.h"

Utils::Utils(){}

// ---------------- 保存/读取点火参数 ----------------
bool Utils::saveFireParams(const QString &filePath, const FireParams &params)
{
    QJsonObject obj;
    obj["ngMax"] = params.ngMax;
    obj["ngOpenTime"] = params.ngOpenTime;
    obj["ngCloseTime"] = params.ngCloseTime;
    obj["o2Max"] = params.o2Max;
    obj["o2OpenTime"] = params.o2OpenTime;
    obj["o2CloseTime"] = params.o2CloseTime;
    obj["o2Delay"] = params.o2Delay;

    QJsonDocument doc(obj);
    QFile file(filePath);
    if(!file.open(QIODevice::WriteOnly)) return false;
    file.write(doc.toJson());
    file.close();
    return true;
}

bool Utils::loadFireParams(const QString &filePath, FireParams &params)
{
    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if(!doc.isObject()) return false;
    QJsonObject obj = doc.object();
    params.ngMax = obj["ngMax"].toDouble();
    params.ngOpenTime = obj["ngOpenTime"].toDouble();
    params.ngCloseTime = obj["ngCloseTime"].toDouble();
    params.o2Max = obj["o2Max"].toDouble();
    params.o2OpenTime = obj["o2OpenTime"].toDouble();
    params.o2CloseTime = obj["o2CloseTime"].toDouble();
    params.o2Delay = obj["o2Delay"].toDouble();
    return true;
}

// ---------------- 保存/读取冷却水恒温参数 ----------------
bool Utils::saveCoolingParams(const QString &filePath, const CoolingParams &params)
{
    QJsonObject obj;
    obj["temperature"] = params.temperature;
    obj["pumpOn"] = params.pumpOn;
    obj["fanOn"] = params.fanOn;

    QJsonDocument doc(obj);
    QFile file(filePath);
    if(!file.open(QIODevice::WriteOnly)) return false;
    file.write(doc.toJson());
    file.close();
    return true;
}

bool Utils::loadCoolingParams(const QString &filePath, CoolingParams &params)
{
    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if(!doc.isObject()) return false;
    QJsonObject obj = doc.object();
    params.temperature = obj["temperature"].toDouble();
    params.pumpOn = obj["pumpOn"].toBool();
    params.fanOn = obj["fanOn"].toBool();
    return true;
}

// ---------------- 列出符合条件的试验文件夹 ----------------
QStringList Utils::listExperimentFolders(const QString &dirPath,
                                         const QString &experimentType,
                                         const QString &material,
                                         const QString &workpiece)
{
    QStringList result;
    QDir dir(dirPath);
    if(!dir.exists()) return result;
    QFileInfoList folders = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for(const QFileInfo &folder : folders)
    {
        QString name = folder.fileName();
        if(!experimentType.isEmpty() && !name.contains(experimentType)) continue;
        if(!material.isEmpty() && !name.contains(material)) continue;
        if(!workpiece.isEmpty() && !name.contains(workpiece)) continue;
        result.append(folder.absoluteFilePath());
    }
    return result;
}
