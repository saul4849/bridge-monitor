#include "calibrationmanager.h"
#include <QFile>
#include <QDebug>
#include <QDir>
#include <opencv2/imgproc.hpp>

CalibrationManager::CalibrationManager()
    : m_calibrated(false)
    , m_pixelScale(0.05)
    , m_focalLength(0.0)
{
    m_cameraMatrix = cv::Mat::eye(3, 3, CV_64F);
    m_distCoeffs = cv::Mat::zeros(1, 5, CV_64F);
}

CalibrationManager::~CalibrationManager() {}

bool CalibrationManager::loadFromFile(const QString& filepath)
{
    cv::FileStorage fs(filepath.toStdString(), cv::FileStorage::READ);
    if (!fs.isOpened()) {
        qDebug() << "[CalibrationManager] Failed to open:" << filepath;
        return false;
    }

    fs["camera_matrix"] >> m_cameraMatrix;
    fs["distortion_coefficients"] >> m_distCoeffs;
    fs["image_width"] >> m_imageSize.width;
    fs["image_height"] >> m_imageSize.height;
    fs["pixel_scale"] >> m_pixelScale;
    fs["focal_length"] >> m_focalLength;

    int calibFlag = 0;
    fs["calibrated"] >> calibFlag;
    m_calibrated = (calibFlag != 0);

    if (m_calibrated && m_imageSize.width > 0 && m_imageSize.height > 0) {
        cv::initUndistortRectifyMap(
            m_cameraMatrix, m_distCoeffs, cv::Mat(),
            cv::getOptimalNewCameraMatrix(m_cameraMatrix, m_distCoeffs, m_imageSize, 1, m_imageSize, nullptr),
            m_imageSize, CV_16SC2, m_map1, m_map2);
    }

    qDebug() << "[CalibrationManager] Loaded. Calibrated:" << m_calibrated
             << "PixelScale:" << m_pixelScale;
    return true;
}

bool CalibrationManager::saveToFile(const QString& filepath) const
{
    // 只有路径包含子目录时才创建目录，同级目录不操作
    QDir dir;
    QString path = QFileInfo(filepath).absolutePath();
    if (!path.isEmpty() && path != "." && path != QDir::currentPath()) {
        dir.mkpath(path);
    }

    cv::FileStorage fs(filepath.toStdString(), cv::FileStorage::WRITE);
    if (!fs.isOpened()) return false;

    fs << "camera_matrix" << m_cameraMatrix;
    fs << "distortion_coefficients" << m_distCoeffs;
    fs << "image_width" << m_imageSize.width;
    fs << "image_height" << m_imageSize.height;
    fs << "pixel_scale" << m_pixelScale;
    fs << "focal_length" << m_focalLength;
    fs << "calibrated" << (m_calibrated ? 1 : 0);

    return true;
}

bool CalibrationManager::calibrate(const std::vector<cv::Mat>& images, cv::Size boardSize, float squareSize)
{
    std::vector<std::vector<cv::Point3f>> objectPoints;
    std::vector<std::vector<cv::Point2f>> imagePoints;

    std::vector<cv::Point3f> objCorners;
    for (int i = 0; i < boardSize.height; ++i)
        for (int j = 0; j < boardSize.width; ++j)
            objCorners.emplace_back(j * squareSize, i * squareSize, 0.0f);

    for (const auto& img : images) {
        cv::Mat gray;
        if (img.channels() == 3) cv::cvtColor(img, gray, cv::COLOR_RGB2GRAY);
        else gray = img;

        std::vector<cv::Point2f> corners;
        bool found = cv::findChessboardCorners(gray, boardSize, corners,
            cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE);

        if (found) {
            cv::cornerSubPix(gray, corners, cv::Size(11, 11), cv::Size(-1, -1),
                cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.1));
            imagePoints.push_back(corners);
            objectPoints.push_back(objCorners);
        }
    }

    if (imagePoints.size() < 3) {
        qDebug() << "[CalibrationManager] Need at least 3 valid images";
        return false;
    }

    m_imageSize = images[0].size();
    std::vector<cv::Mat> rvecs, tvecs;
    double rms = cv::calibrateCamera(objectPoints, imagePoints, m_imageSize,
        m_cameraMatrix, m_distCoeffs, rvecs, tvecs, cv::CALIB_FIX_K4 | cv::CALIB_FIX_K5);

    if (!tvecs.empty()) {
        double distance = cv::norm(tvecs[0]);
        double fx = m_cameraMatrix.at<double>(0, 0);
        m_focalLength = fx;
        m_pixelScale = (distance / fx);
    }

    m_calibrated = true;

    cv::initUndistortRectifyMap(
        m_cameraMatrix, m_distCoeffs, cv::Mat(),
        cv::getOptimalNewCameraMatrix(m_cameraMatrix, m_distCoeffs, m_imageSize, 1, m_imageSize, nullptr),
        m_imageSize, CV_16SC2, m_map1, m_map2);

    qDebug() << "[CalibrationManager] Calibration done. RMS:" << rms
             << "PixelScale:" << m_pixelScale;
    return true;
}

cv::Mat CalibrationManager::undistort(const cv::Mat& src) const
{
    if (!m_calibrated || m_map1.empty()) return src.clone();
    cv::Mat dst;
    cv::remap(src, dst, m_map1, m_map2, cv::INTER_LINEAR);
    return dst;
}

double CalibrationManager::pixelToMM(double pixelDistance, double targetDistanceMM) const
{
    if (!m_calibrated || m_focalLength <= 0) {
        return pixelDistance * m_pixelScale;
    }
    return pixelDistance * targetDistanceMM / m_focalLength;
}

void CalibrationManager::updatePixelScale(double physicalSizeMM, double pixelSize)
{
    if (pixelSize > 0) {
        m_pixelScale = physicalSizeMM / pixelSize;
        m_calibrated = true;
    }
}
