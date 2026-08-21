#ifndef SLIDINGAVERAGE_H
#define SLIDINGAVERAGE_H

#include <QMap>
#include <QVector>
#include <QString>
#include <opencv2/core.hpp>

class SlidingAverage
{
public:
    explicit SlidingAverage(int windowSize = 5);
    ~SlidingAverage();

    void init(const QString& targetId, const cv::Point2f& initialPos);
    void reset(const QString& targetId);
    cv::Point2f update(const QString& targetId, const cv::Point2f& measurement);
    bool hasTracker(const QString& targetId) const;
    void removeTracker(const QString& targetId);

private:
    struct TrackerState {
        QVector<cv::Point2f> history;
        bool initialized;
        cv::Point2f lastMeasurement;
    };
    QMap<QString, TrackerState> m_trackers;
    int m_windowSize;
};

#endif // SLIDINGAVERAGE_H