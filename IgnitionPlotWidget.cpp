// ignitionplotwidget.cpp
#include "ignitionplotwidget.h"
#include <QPainter>
#include <QFont>
#include <QPolygon>

IgnitionPlotWidget::IgnitionPlotWidget(QWidget *parent)
    : QWidget(parent)
    , m_isPlotDrawn(false) // 初始为未绘制状态
{
    setStyleSheet("background-color: white;");
}

// 外部触发绘图的方法（供按钮调用）
void IgnitionPlotWidget::drawIgnitionPlot()
{
    m_isPlotDrawn = true;
    update(); // 触发paintEvent重绘
}

void IgnitionPlotWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();

    // ========== 初始状态：绘制提示文字 ==========
    if (!m_isPlotDrawn) {
        painter.setFont(QFont("微软雅黑", 14, QFont::Bold));
        painter.setPen(Qt::gray);
        // 文字居中显示
        QString tipText = "待生成燃烧器1点火时序图";
        QRect textRect(0, 0, w, h);
        painter.drawText(textRect, Qt::AlignCenter, tipText);
        return; // 初始状态只画提示，直接返回
    }

    // ========== 绘制完成状态：绘制完整时序图 ==========
    int margin = 50; // 边距，给坐标轴和标签留出空间

    // 1. 画背景网格和坐标轴
    painter.setPen(QPen(Qt::lightGray, 1, Qt::DotLine));
    // 水平网格线（-10,0,10,20,30,40）
    QList<int> yTicks = {-10, 0, 10, 20, 30, 40};
    for (int y : yTicks) {
        int py = h - margin - (y + 10) * (h - 2*margin) / 50; // -10~40 共50单位
        painter.drawLine(margin, py, w - margin, py);
    }
    // 垂直网格线（0,2,4...20）
    QList<int> xTicks = {0,2,4,6,8,10,12,14,16,18,20};
    for (int x : xTicks) {
        int px = margin + x * (w - 2*margin) / 20; // 0~20秒
        painter.drawLine(px, margin, px, h - margin);
    }

    // 坐标轴
    painter.setPen(QPen(Qt::black, 2));
    painter.drawLine(margin, h - margin, w - margin, h - margin); // X轴
    painter.drawLine(margin, margin, margin, h - margin);         // Y轴

    // 坐标轴标签
    painter.setPen(Qt::black);
    painter.setFont(QFont("Arial", 10));
    // Y轴标签
    for (int y : yTicks) {
        int py = h - margin - (y + 10) * (h - 2*margin) / 50;
        painter.drawText(margin - 35, py + 5, QString::number(y));
    }
    // X轴标签
    for (int x : xTicks) {
        int px = margin + x * (w - 2*margin) / 20;
        painter.drawText(px - 10, h - margin + 20, QString::number(x, 'f', 1));
    }
    // 竖排中文 + 旋转90度单位
    QString text = "质量流量阀流量";
    int startX = 15;
    int startY = h / 2 - 70; // 整体上下居中
    int lineHeight = 18;

    // 1. 中文逐字竖排
    for (int i = 0; i < text.size(); ++i) {
        painter.drawText(startX, startY + i * lineHeight, text.at(i));
    }

    // 2. (L/min) 旋转90度显示
    painter.save();
    int unitX = 22;
    int unitY = startY + text.size() * lineHeight + 15;
    painter.translate(unitX, unitY);  // 定位到中文下方
    painter.rotate(90);              // 旋转90度
    painter.drawText(0, 0, "(L/min)");
    painter.restore();

    painter.drawText(w/2 - 40, h - 5, "时间 (秒)");

    // 2. 画三条曲线
    auto toX = [&](double t) { return margin + t * (w - 2*margin) / 20; };
    auto toY = [&](double v) { return h - margin - (v + 10) * (h - 2*margin) / 50; };

    // 燃气（黄色）：0→4秒升到4，保持到18秒，降到0
    painter.setPen(QPen(QColor(255,200,0), 2));
    QPolygon gasLine;
    gasLine << QPoint(toX(0), toY(0))
            << QPoint(toX(4), toY(4))
            << QPoint(toX(18), toY(4))
            << QPoint(toX(20), toY(0));
    painter.drawPolyline(gasLine);

    // 点火器（红色）：1~4秒
    painter.setPen(QPen(Qt::red, 2));
    painter.drawLine(toX(1), toY(-2), toX(4), toY(-2));

    // 氧气（青色）：3→12秒升到15，保持到14秒，降到0
    painter.setPen(QPen(Qt::cyan, 2));
    QPolygon oxyLine;
    oxyLine << QPoint(toX(3), toY(0))
            << QPoint(toX(12), toY(15))
            << QPoint(toX(14), toY(15))
            << QPoint(toX(20), toY(0));
    painter.drawPolyline(oxyLine);

    // 3. 图例
    painter.setPen(Qt::black);
    painter.setFont(QFont("Arial", 10));
    int legendX = w - 120;
    int legendY = 30;
    // 燃气
    painter.setPen(QPen(QColor(255,200,0), 2));
    painter.drawLine(legendX, legendY, legendX+20, legendY);
    painter.setPen(Qt::black);
    painter.drawText(legendX+10, legendY+5, "燃气质量流量阀开度");
    // 点火器
    painter.setPen(QPen(Qt::red, 2));
    painter.drawLine(legendX, legendY+25, legendX+20, legendY+25);
    painter.setPen(Qt::black);
    painter.drawText(legendX+10, legendY+30, "点火器打火");
    // 氧气
    painter.setPen(QPen(Qt::cyan, 2));
    painter.drawLine(legendX, legendY+50, legendX+20, legendY+50);
    painter.setPen(Qt::black);
    painter.drawText(legendX+10, legendY+55, "氧气质量流量阀开度");
}
