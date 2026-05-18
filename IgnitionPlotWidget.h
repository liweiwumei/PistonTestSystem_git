// ignitionplotwidget.h
#ifndef IGNITIONPLOTWIDGET_H
#define IGNITIONPLOTWIDGET_H

#include <QWidget>

class IgnitionPlotWidget : public QWidget
{
    Q_OBJECT
public:
    explicit IgnitionPlotWidget(QWidget *parent = nullptr);
    // 提供外部触发绘图的接口
    void drawIgnitionPlot();


protected:
    void paintEvent(QPaintEvent *event) override;
private:
    bool m_isPlotDrawn; // 标记是否已绘制时序图
};

#endif // IGNITIONPLOTWIDGET_H
