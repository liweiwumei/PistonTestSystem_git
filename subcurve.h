#ifndef SUBCURVE_H
#define SUBCURVE_H

#include <QWidget>
#include "qcustomplot.h"
#include <QDateTime>
#include <QVector>

class SubCurve : public QCustomPlot
{
    Q_OBJECT
public:
    explicit SubCurve(QWidget *parent = nullptr);
    virtual ~SubCurve();

    // 初始化温度曲线（12路热电偶+1路统计值）
    void initTemperatureCurve();

    // 重置曲线：清空+重建13条
    void resetTemperatureGraphs();

    // 实时更新数据
    // 传入12个温度，第13个可以外部传，也可以传-1让内部自动算最大/平均
    void updateTemperatureData(const QVector<float>& thermocoupleTemps, float computedValue = -1);

    // ------------------------------
    // 扩展2：设置X轴显示范围（秒）
    // ------------------------------
    void setXRangeSeconds(double seconds);

    // ------------------------------
    // 扩展3：设置保留数据时长（秒），自动清理旧数据
    // ------------------------------
    void setKeepDataSeconds(double seconds);

    // ------------------------------
    // 扩展4：单独显示/隐藏某条曲线
    // ------------------------------
    void setCurveVisible(int index, bool visible);
    void showAllCurves();
    void hideAllCurves();

    // 通用设置
    void setYAxisRange(double lower, double upper);
    void setComputedMode(bool useMax); // true=最大值，false=平均值

private:
    void initUiStyle();
    void addTemperatureGraph(int index);
    float calculateComputedValue(const QVector<float>& temps);

private:
    bool m_useMax = true;               // 统计值：true=最大 false=平均
    double m_xRangeSec = 60.0;          // X轴范围
    double m_keepDataSec = 300.0;        // 默认保留5分钟数据
    QColor m_lineColors[13];
};

#endif // SUBCURVE_H
