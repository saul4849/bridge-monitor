#ifndef KALMANTRACKER_H
#define KALMANTRACKER_H

#include <opencv2/video/tracking.hpp>
#include <QMap>
#include <QString>

class KalmanTracker
{
public:
    KalmanTracker();
    ~KalmanTracker();

    void init(const QString& targetId, const cv::Point2f& initialPos);
    void reset(const QString& targetId);
    cv::Point2f update(const QString& targetId, const cv::Point2f& measurement);
    bool hasTracker(const QString& targetId) const;
    void removeTracker(const QString& targetId);

private:
    struct TrackerState {
        cv::KalmanFilter kf;
        bool initialized;
        cv::Point2f lastMeasurement;
    };

    QMap<QString, TrackerState> m_trackers;
    void setupKalmanFilter(cv::KalmanFilter& kf, const cv::Point2f& initPos);
};

#endif // KALMANTRACKER_H
