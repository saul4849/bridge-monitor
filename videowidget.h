#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QLabel>
#include <QTimer>
#include <QMouseEvent>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <opencv2/opencv.hpp>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <opencv2/imgproc.hpp>
#include <QList>
#include <QMap>
#include "target.h"
#include "calibrationmanager.h"
#include "kalmantracker.h"
#include "statusanalyzer.h"


class VideoWidget : public QLabel {
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget();
    QList<Target>& targets() { return m_targets; }
    void setTargets(const QList<Target>& t) { m_targets = t; }
    void updateAllReferenceImages();
    QPixmap getCurrentFrame() const;

    // 标定管理接口
    CalibrationManager* calibrationManager() { return &m_calibManager; }
    void setTargetDistance(double distanceMM) { m_targetDistance = distanceMM; }

public slots:
    void openCamera();
    void closeCamera();
    void startDetect();
    void stopDetect();
    void resetZero();

signals:
    void targetUpdated(int index, double dx, double dy, double confidence,
                       double brightness, double ssim, double distance, QString status);
    void targetListChanged(const QList<Target>& targets);

private slots:
    void captureFrame();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    bool xioctl(int request, void *arg);
    QPoint mapToImage(const QPoint& pos) const;
    double calculateBrightness(const cv::Mat& roi);
    double calculateSSIM(const cv::Mat& img1, const cv::Mat& img2);

    // 像素转物理距离（优先使用标定参数）
    double pixelsToMM(double pixelDelta, int targetIndex) const;

    QTimer *timer;
    int fd;
    int stride;
    void* buffers[4];
    int bufferLength;
    bool detecting;
    int width;
    int height;
    bool m_selecting = false;
    QPoint m_roiStart;
    QPoint m_roiEnd;
    QList<Target> m_targets;
    cv::Mat m_currentFrame;
    bool m_hasFrame = false;
    QFile* m_csvFile = nullptr;
    QTextStream* m_csvStream = nullptr;
    QDateTime m_startTime;

    
    CalibrationManager m_calibManager;
    KalmanTracker m_kalmanTracker;
    StatusAnalyzer m_statusAnalyzer;
    double m_targetDistance = 1000.0;   // 靶标到相机距离(mm)
    QMap<QString, int> m_consecutiveFrames; // 连续检测帧计数（用targetId做key）
};

#endif // VIDEOWIDGET_H
