#ifndef STATUSANALYZER_H
#define STATUSANALYZER_H

#include <QString>
#include <QVector>

struct StatusResult {
    QString statusText;
    QString statusColor;
    QString detailReason;
    bool isNormal;
};

class StatusAnalyzer
{
public:
    StatusAnalyzer();

    StatusResult analyze(double confidence, double brightness, double ssim,
                         double dx, double dy, int consecutiveFrames);

    void updateHistory(double confidence, double brightness, double ssim);
    void resetHistory();

    void setThresholds(double confLow, double confWarn,
                       double ssimLow, double ssimWarn,
                       double brightLow, double brightHigh);

private:
    QVector<double> m_confHistory;
    QVector<double> m_brightHistory;
    QVector<double> m_ssimHistory;
    int m_maxHistory;

    double m_confLow, m_confWarn;
    double m_ssimLow, m_ssimWarn;
    double m_brightLow, m_brightHigh;
};

#endif // STATUSANALYZER_H
