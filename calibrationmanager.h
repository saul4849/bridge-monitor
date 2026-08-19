#ifndef CALIBRATIONMANAGER_H
#define CALIBRATIONMANAGER_H

#include <opencv2/opencv.hpp>
#include <QString>

class CalibrationManager
{
public:
    CalibrationManager();
    ~CalibrationManager();

    bool loadFromFile(const QString& filepath = "camera.yaml");
    bool saveToFile(const QString& filepath = "camera.yaml") const;

    // 标定接口：传入棋盘格图像列表，计算内参和畸变系数
    bool calibrate(const std::vector<cv::Mat>& images, cv::Size boardSize, float squareSize);

    // 去畸变
    cv::Mat undistort(const cv::Mat& src) const;

    // 像素到物理距离转换（基于标定得到的像素尺度）
    double pixelToMM(double pixelDistance, double targetDistanceMM = 1000.0) const;

    // 获取标定状态
    bool isCalibrated() const { return m_calibrated; }
    double getPixelScale() const { return m_pixelScale; }
    cv::Mat getCameraMatrix() const { return m_cameraMatrix; }
    cv::Mat getDistCoeffs() const { return m_distCoeffs; }

    // 根据靶标实际物理尺寸和图像像素尺寸计算像素比例
    void updatePixelScale(double physicalSizeMM, double pixelSize);

private:
    bool m_calibrated;
    cv::Mat m_cameraMatrix;      // 3x3 内参矩阵
    cv::Mat m_distCoeffs;        // 畸变系数
    cv::Mat m_map1, m_map2;      // 去畸变映射表（预计算加速）
    cv::Size m_imageSize;
    double m_pixelScale;         // mm/pixel，基于标定平面计算
    double m_focalLength;        // 焦距（像素）
};

#endif // CALIBRATIONMANAGER_H
