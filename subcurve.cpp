#include "subcurve.h"
#include <QDebug>

SubCurve::SubCurve(QWidget *parent)
    : QCustomPlot(parent)
{
}

SubCurve::~SubCurve()
{
}

void SubCurve::initTemperatureCurve()
{
    initUiStyle();

    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
    dateTicker->setDateTimeFormat("hh:mm:ss");
    dateTicker->setDateTimeSpec(Qt::LocalTime);
    dateTicker->setTickCount(10);
    xAxis->setTicker(dateTicker);

    double now = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    xAxis->setRange(now - m_xRangeSec/2, now + m_xRangeSec/2);

    yAxis->setRange(0, 150);
}

void SubCurve::resetTemperatureGraphs()
{
    clearGraphs();
    for (int i = 0; i < 12; ++i) {
        addTemperatureGraph(i);
    }
    replot(QCustomPlot::rpQueuedReplot);
}

void SubCurve::updateTemperatureData(const QVector<float>& thermocoupleTemps, float computedValue)
{
    if (thermocoupleTemps.size() != 12) {
        qWarning() << "必须传入12个温度值";
        return;
    }

    double now = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    double currentMaxVal = 0;

    // 12路热电偶
    for (int i = 0; i < 12; ++i) {
        float t = thermocoupleTemps[i];
        graph(i)->addData(now, t);
        //currentMaxVal = qMax(currentMaxVal, (double)t);
    }

    // 第13路：计算值
//    float calcVal = (computedValue < 0) ? calculateComputedValue(thermocoupleTemps) : computedValue;
//    graph(12)->addData(now, calcVal);
//    currentMaxVal = qMax(currentMaxVal, (double)calcVal);

    // ------------------------------
    // 🔥 彻底移除崩溃风险的数据清理
    // ------------------------------

    // Y轴自动上移
    QCPRange yRange = yAxis->range();
    if (currentMaxVal > yRange.upper) {
        yAxis->moveRange(currentMaxVal - yRange.upper);
    }

    // X轴自动滚动
    QCPRange xRange = xAxis->range();
    if (now > xRange.upper) {
        xAxis->moveRange(now - xRange.upper);
    }

    replot(QCustomPlot::rpQueuedReplot);
}

void SubCurve::setXRangeSeconds(double seconds)
{
    if (seconds < 10) seconds = 10;
    m_xRangeSec = seconds;
    double now = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    xAxis->setRange(now - m_xRangeSec/2, now + m_xRangeSec/2);
}

void SubCurve::setKeepDataSeconds(double seconds)
{
    if (seconds < 30) seconds = 30;
    m_keepDataSec = seconds;
}

void SubCurve::setCurveVisible(int index, bool visible)
{
    if (index < 0 || index >= 13) return;
    graph(index)->setVisible(visible);
    replot();
}

void SubCurve::showAllCurves()
{
    for (int i = 0; i < 13; ++i) graph(i)->setVisible(true);
    replot();
}

void SubCurve::hideAllCurves()
{
    for (int i = 0; i < 13; ++i) graph(i)->setVisible(false);
    replot();
}

void SubCurve::setYAxisRange(double lower, double upper)
{
    yAxis->setRange(lower, upper);
    replot();
}

void SubCurve::setComputedMode(bool useMax)
{
    m_useMax = useMax;
}

float SubCurve::calculateComputedValue(const QVector<float>& temps)
{
    if (temps.isEmpty()) return 0;
    if (m_useMax) {
        float m = temps[0];
        for (float t : temps) m = qMax(m, t);
        return m;
    } else {
        float sum = 0;
        for (float t : temps) sum += t;
        return sum / temps.size();
    }
}

void SubCurve::initUiStyle()
{
    connect(xAxis, SIGNAL(rangeChanged(QCPRange)), xAxis2, SLOT(setRange(QCPRange)));
    connect(yAxis, SIGNAL(rangeChanged(QCPRange)), yAxis2, SLOT(setRange(QCPRange)));
    connect(yAxis2, SIGNAL(rangeChanged(QCPRange)), yAxis, SLOT(setRange(QCPRange)));

    setInteractions(QCP::iRangeZoom | QCP::iRangeDrag);
    setBackground(QColor(0,0,0));
    axisRect()->setBackground(Qt::transparent);

    QColor gridColor(0, 57, 63);
    xAxis->grid()->setPen(QPen(gridColor, 1));
    yAxis->grid()->setPen(QPen(gridColor, 1, Qt::DashLine));
    xAxis->grid()->setSubGridVisible(false);
    yAxis->grid()->setSubGridVisible(true);
    xAxis->grid()->setZeroLinePen(gridColor);
    yAxis->grid()->setZeroLinePen(gridColor);

    xAxis->setBasePen(gridColor);
    yAxis->setBasePen(gridColor);
    yAxis2->setBasePen(gridColor);

    xAxis->setTickPen(gridColor);
    yAxis->setTickPen(gridColor);
    yAxis2->setTickPen(gridColor);

    xAxis->setTickLabelFont(QFont("Microsoft YaHei", 12));
    yAxis->setTickLabelFont(QFont("Microsoft YaHei", 10));
    xAxis->setTickLabelColor(Qt::white);
    yAxis->setTickLabelColor(Qt::white);
    yAxis2->setTickLabelColor(Qt::white);

    xAxis->setUpperEnding(QCPLineEnding::esSpikeArrow);
    yAxis->setUpperEnding(QCPLineEnding::esSpikeArrow);

    xAxis->setSubTicks(false);
    yAxis->setSubTicks(false);
}

void SubCurve::addTemperatureGraph(int index)
{
    QColor c;
    switch(index) {
        case 0: c=QColor(220,80,80); break;
        case 1: c=QColor(80,220,80); break;
        case 2: c=QColor(80,80,220); break;
        case 3: c=QColor(220,220,80); break;
        case 4: c=QColor(220,80,220); break;
        case 5: c=QColor(80,220,220); break;
        case 6: c=QColor(255,180,0); break;
        case 7: c=QColor(180,255,0); break;
        case 8: c=QColor(180,0,255); break;
        case 9: c=QColor(255,255,255); break;
        case 10: c=QColor(255,120,0); break;
        case 11: c=QColor(0,255,180); break;
        case 12: c=QColor(255,255,0); break;
        default:c=Qt::white;
    }

    addGraph();
    QPen pen(c);
    pen.setWidth(index <12 ? 2:3);
    pen.setStyle(index <12 ? Qt::SolidLine:Qt::DotLine);
    graph(index)->setPen(pen);
    graph(index)->setLineStyle(QCPGraph::lsLine);
}
