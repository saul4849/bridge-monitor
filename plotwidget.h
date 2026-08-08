#ifndef PLOTWIDGET_H
#define PLOTWIDGET_H

#include <QWidget>
#include <QVector>

struct PlotData {
    QVector<double> time;
    QVector<double> dx;
    QVector<double> dy;
    QString targetName;
};

class PlotWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlotWidget(QWidget *parent = nullptr);
    void addData(int targetIndex, double t, double dx, double dy);
    void clearData();
    void setTargetNames(const QStringList& names);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QVector<PlotData> m_data;
    double m_yRange = 5.0;
    QStringList m_names;
    int m_maxPoints = 300;
    int m_leftMargin = 70;   
    int m_bottomMargin = 35;
    int m_topMargin = 40;
    int m_rightMargin = 10;
};

#endif // PLOTWIDGET_H
