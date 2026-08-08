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
#include "target.h"

class VideoWidget : public QLabel {
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget();

    QList<Target>& targets() { return m_targets; }
    void setTargets(const QList<Target>& t) { m_targets = t; }
    void updateAllReferenceImages();
    QPixmap getCurrentFrame() const;

public slots:
    void openCamera();
    void closeCamera();
    void startDetect();
    void stopDetect();
    void resetZero();

signals:
    // 扩展信号：传递完整监测数据
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
};

#endif // VIDEOWIDGET_H
