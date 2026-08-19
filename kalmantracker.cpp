#include "kalmantracker.h"
#include <QDebug>

KalmanTracker::KalmanTracker() {}

KalmanTracker::~KalmanTracker() {}

void KalmanTracker::setupKalmanFilter(cv::KalmanFilter& kf, const cv::Point2f& initPos)
{
    // 状态向量：[x, y, vx, vy]^T
    // 测量向量：[x, y]^T
    kf.init(4, 2, 0);

    kf.transitionMatrix = (cv::Mat_<float>(4, 4) <<
        1, 0, 1, 0,
        0, 1, 0, 1,
        0, 0, 1, 0,
        0, 0, 0, 1);

    kf.measurementMatrix = (cv::Mat_<float>(2, 4) <<
        1, 0, 0, 0,
        0, 1, 0, 0);

    cv::setIdentity(kf.processNoiseCov, cv::Scalar::all(1e-4));
    cv::setIdentity(kf.measurementNoiseCov, cv::Scalar::all(1e-1));
    cv::setIdentity(kf.errorCovPost, cv::Scalar::all(1));

    kf.statePost.at<float>(0) = initPos.x;
    kf.statePost.at<float>(1) = initPos.y;
    kf.statePost.at<float>(2) = 0;
    kf.statePost.at<float>(3) = 0;
}

void KalmanTracker::init(const QString& targetId, const cv::Point2f& initialPos)
{
    TrackerState ts;
    ts.initialized = true;
    ts.lastMeasurement = initialPos;
    setupKalmanFilter(ts.kf, initialPos);
    m_trackers[targetId] = ts;
    qDebug() << "[KalmanTracker] Initialized for target:" << targetId;
}

void KalmanTracker::reset(const QString& targetId)
{
    if (m_trackers.contains(targetId)) {
        m_trackers.remove(targetId);
    }
}

cv::Point2f KalmanTracker::update(const QString& targetId, const cv::Point2f& measurement)
{
    if (!m_trackers.contains(targetId)) {
        init(targetId, measurement);
        return measurement;
    }

    TrackerState& ts = m_trackers[targetId];
    ts.lastMeasurement = measurement;

    cv::Mat prediction = ts.kf.predict();
    cv::Mat measurementMat = (cv::Mat_<float>(2, 1) << measurement.x, measurement.y);
    cv::Mat estimated = ts.kf.correct(measurementMat);

    return cv::Point2f(estimated.at<float>(0), estimated.at<float>(1));
}

bool KalmanTracker::hasTracker(const QString& targetId) const
{
    return m_trackers.contains(targetId);
}

void KalmanTracker::removeTracker(const QString& targetId)
{
    m_trackers.remove(targetId);
}
