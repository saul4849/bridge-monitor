#ifndef STATUSANALYZER_H
#define STATUSANALYZER_H

#include <QString>
#include <QVector>
#include <QMap>

struct StatusResult {
    QString statusText;
    QString statusColor;
    QString detailReason;
    bool isNormal = false;
};

enum class InstantStatus {
    Initializing,   // 检测中
    Normal,
    Warning,
    Error
};

class StatusAnalyzer
{
public:
    StatusAnalyzer();


    StatusResult analyze(const QString& targetId,
                         double confidence, double brightness, double ssim,
                         double dx, double dy, int consecutiveFrames);


    void resetHistory(const QString& targetId);

    void resetHistory();

    void removeTarget(const QString& targetId);

    void setThresholds(double confLow, double confWarn,
                       double ssimLow, double ssimWarn,
                       double brightLow, double brightHigh);

private:
    struct TargetRecord {
        QVector<double> confHistory;           // 30 帧数值历史
        QVector<double> brightHistory;
        QVector<double> ssimHistory;
        QVector<InstantStatus> instantHistory; // 30 帧瞬时状态历史
        StatusResult confirmedResult;          // 当前已确认的输出状态
        InstantStatus confirmedStatus = InstantStatus::Initializing;
        bool initialized = false;
    };

    // 单帧瞬时判定
    InstantStatus classifyInstant(double confidence, double brightness, double ssim,
                                  double dx, double dy, int consecutiveFrames,
                                  StatusResult& outResult) const;

    // 检查尾部连续 n 帧是否全为指定状态
    bool checkConsecutive(const QVector<InstantStatus>& history, int n, InstantStatus s) const;

    // 更新历史队列
    void updateTargetRecord(const QString& targetId, double confidence, double brightness,
                            double ssim, InstantStatus instant);

    QMap<QString, TargetRecord> m_records;
    int m_maxHistory;

    double m_confLow, m_confWarn;
    double m_ssimLow, m_ssimWarn;
    double m_brightLow, m_brightHigh;
};

#endif // STATUSANALYZER_H
