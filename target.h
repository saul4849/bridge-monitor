#ifndef TARGET_H
#define TARGET_H

#include <QString>
#include <QRect>
#include <opencv2/opencv.hpp>

struct Target {
    QString id;
    QString name;
    QRect roi;              // 原始图像坐标系 ROI
    cv::Mat templateImg;    // RGB 模板图像
    cv::Point2f refCenter;  // 零点中心（图像坐标）
    cv::Point2f currCenter; // 当前中心（图像坐标）
    double dx = 0.0;        // X方向位移 mm
    double dy = 0.0;        // Y方向位移 mm
    double confidence = 0.0;
    bool active = true;
    double mmPerPixel = 0.05; // 保留默认值，运行时会由 CalibrationManager 覆盖
    QString targetType = "参考靶标";
    QString createTime;
    QString updateTime;
    bool isRefCenter = false;
    QString calibStatus = "未校准";

    // 实时监测数据
    double brightness = 1.0;    // 亮度比
    double ssim = 0.0;          // 结构相似度
    double distance = 0.0;      // 靶标到相机距离 (mm)
    QString status = "正常";    // 状态文字
    QString statusColor = "#2ecc71"; // 状态颜色

    Target() {}
    Target(const QString& _id, const QString& _name) : id(_id), name(_name) {}
};

#endif // TARGET_H
