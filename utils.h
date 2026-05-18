#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QDir>

struct FireParams
{
    double ngMax;
    double ngOpenTime;
    double ngCloseTime;
    double o2Max;
    double o2OpenTime;
    double o2CloseTime;
    double o2Delay;
};

struct CoolingParams
{
    double temperature;
    bool pumpOn;
    bool fanOn;
};

class Utils
{
public:
    Utils();

    // ---------------- 点火参数文件操作 ----------------
    static bool saveFireParams(const QString &filePath, const FireParams &params);
    static bool loadFireParams(const QString &filePath, FireParams &params);

    // ---------------- 冷却水恒温参数文件操作 ----------------
    static bool saveCoolingParams(const QString &filePath, const CoolingParams &params);
    static bool loadCoolingParams(const QString &filePath, CoolingParams &params);

    // ---------------- 试验记录文件夹操作 ----------------
    static QStringList listExperimentFolders(const QString &dirPath,
                                             const QString &experimentType,
                                             const QString &material,
                                             const QString &workpiece);
};

#endif // UTILS_H
