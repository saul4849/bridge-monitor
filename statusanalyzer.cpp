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
    m_confLow = confLow; m_confWarn = confWarn;
    m_ssimLow = ssimLow; m_ssimWarn = ssimWarn;
    m_brightLow = brightLow; m_brightHigh = brightHigh;
}

StatusResult StatusAnalyzer::analyze(double confidence, double brightness, double ssim,
                                     double dx, double dy, int consecutiveFrames)
{
    StatusResult result;
    result.isNormal = true;
    result.detailReason = "监测正常";

    if (confidence < m_confLow * 100.0) {
        result.statusText = "异常";
        result.statusColor = "#e74c3c";
        result.detailReason = QString("置信度过低(%1%)").arg(confidence, 0, 'f', 1);
        result.isNormal = false;
    } else if (confidence < m_confWarn * 100.0) {
        result.statusText = "警告";
        result.statusColor = "#f1c40f";
        result.detailReason = QString("置信度偏低(%1%)").arg(confidence, 0, 'f', 1);
        result.isNormal = false;
    }

    if (ssim < m_ssimLow) {
        result.statusText = "异常";
        result.statusColor = "#e74c3c";
        result.detailReason = QString("模板漂移严重(SSIM:%1)").arg(ssim, 0, 'f', 3);
        result.isNormal = false;
    } else if (ssim < m_ssimWarn && result.isNormal) {
        result.statusText = "警告";
        result.statusColor = "#f1c40f";
        result.detailReason = QString("模板可能漂移(SSIM:%1)").arg(ssim, 0, 'f', 3);
        result.isNormal = false;
    }

    if (brightness < m_brightLow) {
        result.statusText = "异常";
        result.statusColor = "#e74c3c";
        result.detailReason = QString("光照不足(%1)").arg(brightness, 0, 'f', 3);
        result.isNormal = false;
    } else if (brightness > m_brightHigh && result.isNormal) {
        result.statusText = "警告";
        result.statusColor = "#f1c40f";
        result.detailReason = QString("光照过强(%1)").arg(brightness, 0, 'f', 3);
        result.isNormal = false;
    }

    if (consecutiveFrames > 5 && result.isNormal) {
        double maxDisp = qMax(qAbs(dx), qAbs(dy));
        if (maxDisp > 5.0) {
            result.statusText = "警告";
            result.statusColor = "#f1c40f";
            result.detailReason = QString("位移突变(%1mm)").arg(maxDisp, 0, 'f', 2);
            result.isNormal = false;
        }
    }

    if (result.isNormal) {
        result.statusText = "正常";
        result.statusColor = "#2ecc71";
    }

    return result;
}

void StatusAnalyzer::updateHistory(double confidence, double brightness, double ssim)
{
    m_confHistory.append(confidence);
    m_brightHistory.append(brightness);
    m_ssimHistory.append(ssim);

    if (m_confHistory.size() > m_maxHistory) {
        m_confHistory.removeFirst();
        m_brightHistory.removeFirst();
        m_ssimHistory.removeFirst();
    }
}

void StatusAnalyzer::resetHistory()
{
    m_confHistory.clear();
    m_brightHistory.clear();
    m_ssimHistory.clear();
}
