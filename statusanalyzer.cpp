#include "statusanalyzer.h"
#include <QtMath>

StatusAnalyzer::StatusAnalyzer()
    : m_maxHistory(30)
    , m_confLow(0.55), m_confWarn(0.70)
    , m_ssimLow(0.65), m_ssimWarn(0.75)
    , m_brightLow(0.25), m_brightHigh(0.95)
{
}

void StatusAnalyzer::setThresholds(double confLow, double confWarn,
                                   double ssimLow, double ssimWarn,
                                   double brightLow, double brightHigh)
{
    m_confLow = confLow;
    m_confWarn = confWarn;
    m_ssimLow = ssimLow;
    m_ssimWarn = ssimWarn;
    m_brightLow = brightLow;
    m_brightHigh = brightHigh;
}

// 单帧瞬时判定
InstantStatus StatusAnalyzer::classifyInstant(double confidence, double brightness, double ssim,
                                              double dx, double dy, int consecutiveFrames,
                                              StatusResult& outResult) const
{
    outResult.isNormal = true;
    outResult.detailReason = "监测正常";
    InstantStatus instant = InstantStatus::Normal;

    // 置信度
    if (confidence < m_confLow * 100.0) {
        outResult.statusText = "异常";
        outResult.statusColor = "#e74c3c";
        outResult.detailReason = QString("置信度过低(%1%)").arg(confidence, 0, 'f', 1);
        outResult.isNormal = false;
        instant = InstantStatus::Error;
    } else if (confidence < m_confWarn * 100.0) {
        outResult.statusText = "警告";
        outResult.statusColor = "#f1c40f";
        outResult.detailReason = QString("置信度偏低(%1%)").arg(confidence, 0, 'f', 1);
        outResult.isNormal = false;
        instant = InstantStatus::Warning;
    }

    // SSIM
    if (ssim < m_ssimLow) {
        outResult.statusText = "异常";
        outResult.statusColor = "#e74c3c";
        outResult.detailReason = QString("模板漂移严重(SSIM:%1)").arg(ssim, 0, 'f', 3);
        outResult.isNormal = false;
        instant = InstantStatus::Error;
    } else if (ssim < m_ssimWarn && outResult.isNormal) {
        outResult.statusText = "警告";
        outResult.statusColor = "#f1c40f";
        outResult.detailReason = QString("模板可能漂移(SSIM:%1)").arg(ssim, 0, 'f', 3);
        outResult.isNormal = false;
        instant = InstantStatus::Warning;
    }

    // 亮度
    if (brightness < m_brightLow) {
        outResult.statusText = "异常";
        outResult.statusColor = "#e74c3c";
        outResult.detailReason = QString("光照不足(%1)").arg(brightness, 0, 'f', 3);
        outResult.isNormal = false;
        instant = InstantStatus::Error;
    } else if (brightness > m_brightHigh && outResult.isNormal) {
        outResult.statusText = "警告";
        outResult.statusColor = "#f1c40f";
        outResult.detailReason = QString("光照过强(%1)").arg(brightness, 0, 'f', 3);
        outResult.isNormal = false;
        instant = InstantStatus::Warning;
    }

    // 位移突变
    if (consecutiveFrames > 5 && outResult.isNormal) {
        double maxDisp = qMax(qAbs(dx), qAbs(dy));
        if (maxDisp > 5.0) {
            outResult.statusText = "警告";
            outResult.statusColor = "#f1c40f";
            outResult.detailReason = QString("位移突变(%1mm)").arg(maxDisp, 0, 'f', 2);
            outResult.isNormal = false;
            instant = InstantStatus::Warning;
        }
    }

    if (outResult.isNormal) {
        outResult.statusText = "正常";
        outResult.statusColor = "#2ecc71";
        instant = InstantStatus::Normal;
    }

    return instant;
}

// 检查尾部连续 n 帧是否全为指定状态
bool StatusAnalyzer::checkConsecutive(const QVector<InstantStatus>& history, int n, InstantStatus s) const
{
    if (history.size() < n)
        return false;

    for (int i = history.size() - n; i < history.size(); ++i) {
        if (history[i] != s)
            return false;
    }
    return true;
}

// 更新历史队列（维护 30 帧滑动窗口）
void StatusAnalyzer::updateTargetRecord(const QString& targetId, double confidence,
                                        double brightness, double ssim, InstantStatus instant)
{
    TargetRecord& rec = m_records[targetId];
    if (!rec.initialized) {
        rec.initialized = true;
        rec.confirmedStatus = InstantStatus::Initializing;
        rec.confirmedResult.statusText = "检测中";
        rec.confirmedResult.statusColor = "#f39c12";
        rec.confirmedResult.detailReason = "监测初始化中，正在积累历史数据...";
        rec.confirmedResult.isNormal = false;
    }

    rec.confHistory.append(confidence);
    rec.brightHistory.append(brightness);
    rec.ssimHistory.append(ssim);
    rec.instantHistory.append(instant);

    while (rec.confHistory.size() > m_maxHistory) {
        rec.confHistory.removeFirst();
        rec.brightHistory.removeFirst();
        rec.ssimHistory.removeFirst();
        rec.instantHistory.removeFirst();
    }
}

// 状态机 + 自适应确认帧数
StatusResult StatusAnalyzer::analyze(const QString& targetId,
                                     double confidence, double brightness, double ssim,
                                     double dx, double dy, int consecutiveFrames)
{
    // 1. 单帧瞬时判定
    StatusResult instantResult;
    InstantStatus instant = classifyInstant(confidence, brightness, ssim,
                                            dx, dy, consecutiveFrames, instantResult);

    // 2. 写入该靶标独立历史窗口
    updateTargetRecord(targetId, confidence, brightness, ssim, instant);
    TargetRecord& rec = m_records[targetId];

    // 3. 预计算各种连续条件（避免重复检查）
    bool tail3Normal  = checkConsecutive(rec.instantHistory, 3, InstantStatus::Normal);
    bool tail3Warning = checkConsecutive(rec.instantHistory, 3, InstantStatus::Warning);
    bool tail3Error   = checkConsecutive(rec.instantHistory, 3, InstantStatus::Error);
    bool tail5Normal  = checkConsecutive(rec.instantHistory, 5, InstantStatus::Normal);
    bool tail5Error   = checkConsecutive(rec.instantHistory, 5, InstantStatus::Error);

    // 4. 状态机转换：根据当前确认状态，决定需要什么条件才能跳转
    InstantStatus newStatus = rec.confirmedStatus;

    switch (rec.confirmedStatus) {
        case InstantStatus::Initializing:
            if (tail3Normal)      newStatus = InstantStatus::Normal;
            else if (tail3Warning) newStatus = InstantStatus::Warning;
            else if (tail3Error)   newStatus = InstantStatus::Error;
            break;

        case InstantStatus::Normal:
            // 正常→异常：5帧
            if (tail5Error)       newStatus = InstantStatus::Error;
            // 正常→警告：3帧
            else if (tail3Warning) newStatus = InstantStatus::Warning;
            break;

        case InstantStatus::Warning:
            // 警告→异常：3帧
            if (tail3Error)       newStatus = InstantStatus::Error;
            // 警告→正常：3帧
            else if (tail3Normal)  newStatus = InstantStatus::Normal;
            break;

        case InstantStatus::Error:
            // 异常→正常：5帧
            if (tail5Normal)      newStatus = InstantStatus::Normal;
            // 异常→警告：3帧
            else if (tail3Warning) newStatus = InstantStatus::Warning;
            break;
    }

    // 5. 若状态发生转换，更新确认结果的文字/颜色/原因
    if (newStatus != rec.confirmedStatus) {
        rec.confirmedStatus = newStatus;

        switch (newStatus) {
            case InstantStatus::Normal:
                rec.confirmedResult.statusText = "正常";
                rec.confirmedResult.statusColor = "#2ecc71";
                rec.confirmedResult.isNormal = true;
                rec.confirmedResult.detailReason = "监测正常";
                break;

            case InstantStatus::Warning:
                rec.confirmedResult.statusText = "警告";
                rec.confirmedResult.statusColor = "#f1c40f";
                rec.confirmedResult.isNormal = false;
                // 采用当前帧的瞬时原因（如"置信度偏低"或"光照过强"）
                rec.confirmedResult.detailReason = instantResult.detailReason;
                break;

            case InstantStatus::Error:
                rec.confirmedResult.statusText = "异常";
                rec.confirmedResult.statusColor = "#e74c3c";
                rec.confirmedResult.isNormal = false;
                rec.confirmedResult.detailReason = instantResult.detailReason;
                break;

            default:
                break;
        }
    }

    // 若状态未变，保持原样，防止闪烁
    return rec.confirmedResult;
}

// 清空历史，回到检测中
void StatusAnalyzer::resetHistory(const QString& targetId)
{
    if (!m_records.contains(targetId))
        return;

    TargetRecord& rec = m_records[targetId];
    rec.confHistory.clear();
    rec.brightHistory.clear();
    rec.ssimHistory.clear();
    rec.instantHistory.clear();

    rec.confirmedStatus = InstantStatus::Initializing;
    rec.confirmedResult.statusText = "检测中";
    rec.confirmedResult.statusColor = "#f39c12";
    rec.confirmedResult.detailReason = "历史已重置，正在重新确认...";
    rec.confirmedResult.isNormal = false;
}


void StatusAnalyzer::resetHistory()
{
    for (auto it = m_records.begin(); it != m_records.end(); ++it)
        resetHistory(it.key());
}


void StatusAnalyzer::removeTarget(const QString& targetId)
{
    m_records.remove(targetId);
}
