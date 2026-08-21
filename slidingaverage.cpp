#include "slidingaverage.h"

SlidingAverage::SlidingAverage(int windowSize) : m_windowSize(windowSize) {}

SlidingAverage::~SlidingAverage() {}

void SlidingAverage::init(const QString& targetId, const cv::Point2f& initialPos)
{
    TrackerState ts;
    ts.initialized = true;
    ts.lastMeasurement = initialPos;
    ts.history.append(initialPos);
    m_trackers[targetId] = ts;
}

void SlidingAverage::reset(const QString& targetId)
{
    if (m_trackers.contains(targetId)) {
        m_trackers.remove(targetId);
    }
}

cv::Point2f SlidingAverage::update(const QString& targetId, const cv::Point2f& measurement)
{
    if (!m_trackers.contains(targetId)) {
        init(targetId, measurement);
        return measurement;
    }

    TrackerState& ts = m_trackers[targetId];
    ts.lastMeasurement = measurement;
    ts.history.append(measurement);

    // 保持窗口长度
    while (ts.history.size() > m_windowSize) {
        ts.history.removeFirst();
    }

    // 求滑动平均
    cv::Point2f avg(0.0f, 0.0f);
    for (const auto& pt : ts.history) {
        avg.x += pt.x;
        avg.y += pt.y;
    }
    avg.x /= static_cast<float>(ts.history.size());
    avg.y /= static_cast<float>(ts.history.size());

    return avg;
}

bool SlidingAverage::hasTracker(const QString& targetId) const
{
    return m_trackers.contains(targetId);
}

void SlidingAverage::removeTracker(const QString& targetId)
{
    m_trackers.remove(targetId);
}