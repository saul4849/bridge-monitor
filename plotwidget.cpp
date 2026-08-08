#include "plotwidget.h"
#include <QPainter>
#include <QtMath>
#include <QFontMetrics>

PlotWidget::PlotWidget(QWidget *parent) : QWidget(parent) {
    setMinimumHeight(250);
    setStyleSheet("background:#101820; border:1px solid #34495e;");
}

void PlotWidget::setTargetNames(const QStringList& names) {
    m_names = names;
    m_data.resize(names.size());
    for (int i = 0; i < names.size(); i++) m_data[i].targetName = names[i];
    update();
}

void PlotWidget::addData(int targetIndex, double t, double dx, double dy) {
    if (targetIndex < 0 || targetIndex >= m_data.size()) return;
    auto& d = m_data[targetIndex];
    d.time.append(t);
    d.dx.append(dx);
    d.dy.append(dy);
    if (d.time.size() > m_maxPoints) {
        d.time.removeFirst();
        d.dx.removeFirst();
        d.dy.removeFirst();
    }
    update();
}

void PlotWidget::clearData() {
    for (auto& d : m_data) {
        d.time.clear(); d.dx.clear(); d.dy.clear();
    }
    update();
}

void PlotWidget::resizeEvent(QResizeEvent *) {
    update();
}

void PlotWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor("#101820"));

    int w = width() - m_leftMargin - m_rightMargin;
    int h = height() - m_topMargin - m_bottomMargin;
    if (w <= 0 || h <= 0) return;

    QRect plotRect(m_leftMargin, m_topMargin, w, h);

    // ========== 计算Y轴范围 ==========
    double maxVal = 0.5;
    for (const auto& d : m_data) {
        for (double v : d.dx) maxVal = qMax(maxVal, qAbs(v));
        for (double v : d.dy) maxVal = qMax(maxVal, qAbs(v));
    }
    m_yRange = qMax(maxVal * 1.2, 0.5);
    double step = 1.0;
    if (m_yRange > 5) step = 2.0;
    if (m_yRange > 10) step = 5.0;
    if (m_yRange > 50) step = 10.0;
    if (m_yRange > 100) step = 20.0;
    m_yRange = ceil(m_yRange / step) * step;
    if (m_yRange < 1.0) m_yRange = 1.0;

    // ========== 绘制网格线 + Y轴刻度 ==========
    p.setPen(QPen(QColor("#2c3e50"), 1));
    QFont tickFont = p.font();
    tickFont.setPointSize(9);
    p.setFont(tickFont);

    int yTicks = 5;
    for (int i = 0; i <= yTicks; i++) {
        double yVal = m_yRange - (i * 2.0 * m_yRange / yTicks);
        int y = plotRect.top() + i * h / yTicks;
        p.drawLine(plotRect.left(), y, plotRect.right(), y);

        p.setPen(QColor("#7f8c8d"));
        QString label = QString::number(yVal, 'f', (m_yRange < 2) ? 2 : 1) + "mm";
        QRect textRect(0, y - 10, m_leftMargin - 8, 20);
        p.drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, label);
        p.setPen(QPen(QColor("#2c3e50"), 1));
    }

    // ========== X轴刻度 ==========
    double t0 = 0, t1 = 10;
    for (const auto& d : m_data) {
        if (!d.time.isEmpty()) {
            t0 = d.time.first();
            t1 = d.time.last();
        }
    }
    if (t1 <= t0) t1 = t0 + 10.0;
    double tRange = t1 - t0;

    int xTicks = 6;
    p.setPen(QColor("#7f8c8d"));
    for (int i = 0; i <= xTicks; i++) {
        double tVal = t0 + i * tRange / xTicks;
        int x = plotRect.left() + i * w / xTicks;
        QString label = QString::number(tVal, 'f', 1) + "s";
        QRect textRect(x - 30, plotRect.bottom() + 4, 60, 20);
        p.drawText(textRect, Qt::AlignCenter, label);
        // 小刻度线
        p.setPen(QPen(QColor("#2c3e50"), 1));
        p.drawLine(x, plotRect.top(), x, plotRect.bottom());
        p.setPen(QColor("#7f8c8d"));
    }

    // 绘制边框
    p.setPen(QPen(QColor("#7f8c8d"), 1));
    p.drawRect(plotRect);

    // ========== 标题和图例 ==========
    QFont titleFont = p.font();
    titleFont.setPointSize(11);
    titleFont.setBold(true);
    p.setFont(titleFont);
    p.setPen(QColor("#ecf0f1"));
    p.drawText(m_leftMargin, 18, "位移-时间曲线");

    // 图例
    QFont legendFont = p.font();
    legendFont.setPointSize(9);
    p.setFont(legendFont);
    int lx = m_leftMargin + 120;
    p.setPen(QPen(QColor(231, 76, 60), 2));
    p.drawLine(lx, 14, lx + 20, 14);
    p.setPen(QColor("#ecf0f1"));
    p.drawText(lx + 25, 18, "X(横向)");

    p.setPen(QPen(QColor(52, 152, 219), 2, Qt::DashLine));
    p.drawLine(lx + 80, 14, lx + 100, 14);
    p.setPen(QColor("#ecf0f1"));
    p.drawText(lx + 105, 18, "Y(竖向)");

    p.setFont(tickFont);

    // ========== 绘制曲线 ==========
    QVector<QColor> colors = {
        QColor(231, 76, 60), QColor(46, 204, 113), QColor(52, 152, 219),
        QColor(155, 89, 182), QColor(241, 196, 15), QColor(26, 188, 156),
        QColor(230, 126, 34), QColor(236, 240, 241)
    };

    auto mapX = [&](double t) { return plotRect.left() + (t - t0) / (t1 - t0) * w; };
    auto mapY = [&](double v) { return plotRect.center().y() - v / m_yRange * (h / 2.0); };

    for (int i = 0; i < m_data.size(); i++) {
        const auto& d = m_data[i];
        if (d.time.size() < 2) continue;
        QColor c = colors[i % colors.size()];

        // X方向 - 实线
        p.setPen(QPen(c, 2, Qt::SolidLine));
        for (int j = 1; j < d.time.size(); j++) {
            p.drawLine(QPointF(mapX(d.time[j-1]), mapY(d.dx[j-1])),
                       QPointF(mapX(d.time[j]), mapY(d.dx[j])));
        }
        // Y方向 - 虚线
        p.setPen(QPen(c, 2, Qt::DashLine));
        for (int j = 1; j < d.time.size(); j++) {
            p.drawLine(QPointF(mapX(d.time[j-1]), mapY(d.dy[j-1])),
                       QPointF(mapX(d.time[j]), mapY(d.dy[j])));
        }
    }
}
