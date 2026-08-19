#include "videowidget.h"
#include <QImage>
#include <QPixmap>
#include <QDebug>
#include <QInputDialog>
#include <QPainter>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#define CAMERA_DEVICE "/dev/video33"
#define BUFFER_COUNT 4

VideoWidget::VideoWidget(QWidget *parent) : QLabel(parent) {
    fd = -1;
    bufferLength = 0;
    width = 1920;
    height = 1080;
    stride = 0;
    for (int i = 0; i < BUFFER_COUNT; i++) buffers[i] = nullptr;
    setText("Camera Offline");
    setAlignment(Qt::AlignCenter);
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &VideoWidget::captureFrame);
    detecting = false;
}

VideoWidget::~VideoWidget() {
    closeCamera();
}

bool VideoWidget::xioctl(int request, void *arg) {
    int r;
    do { r = ioctl(fd, request, arg); }
    while (r < 0 && errno == EINTR);
    return r >= 0;
}

QPixmap VideoWidget::getCurrentFrame() const {
    if (m_currentFrame.empty()) return QPixmap();
    QImage image(m_currentFrame.data, m_currentFrame.cols, m_currentFrame.rows,
                 m_currentFrame.step, QImage::Format_RGB888);
    return QPixmap::fromImage(image.copy());
}

// 计算ROI区域平均亮度（归一化到0-1）
double VideoWidget::calculateBrightness(const cv::Mat& roi) {
    if (roi.empty()) return 0;
    cv::Mat gray;
    if (roi.channels() == 3) cv::cvtColor(roi, gray, cv::COLOR_RGB2GRAY);
    else gray = roi;
    cv::Scalar meanVal = cv::mean(gray);
    return meanVal[0] / 255.0;
}

// SSIM计算（灰度图）
double VideoWidget::calculateSSIM(const cv::Mat& img1, const cv::Mat& img2) {
    if (img1.empty() || img2.empty()) return 0;
    if (img1.size() != img2.size()) return 0;

    cv::Mat gray1, gray2;
    if (img1.channels() == 3) cv::cvtColor(img1, gray1, cv::COLOR_RGB2GRAY);
    else gray1 = img1;
    if (img2.channels() == 3) cv::cvtColor(img2, gray2, cv::COLOR_RGB2GRAY);
    else gray2 = img2;

    const double C1 = 6.5025, C2 = 58.5225;
    cv::Mat I1, I2;
    gray1.convertTo(I1, CV_64F);
    gray2.convertTo(I2, CV_64F);

    cv::Mat I1_2 = I1.mul(I1);
    cv::Mat I2_2 = I2.mul(I2);
    cv::Mat I1_I2 = I1.mul(I2);

    cv::Mat mu1, mu2;
    cv::GaussianBlur(I1, mu1, cv::Size(11, 11), 1.5);
    cv::GaussianBlur(I2, mu2, cv::Size(11, 11), 1.5);

    cv::Mat mu1_2 = mu1.mul(mu1);
    cv::Mat mu2_2 = mu2.mul(mu2);
    cv::Mat mu1_mu2 = mu1.mul(mu2);

    cv::Mat sigma1_2, sigma2_2, sigma12;
    cv::GaussianBlur(I1_2, sigma1_2, cv::Size(11, 11), 1.5);
    cv::GaussianBlur(I2_2, sigma2_2, cv::Size(11, 11), 1.5);
    cv::GaussianBlur(I1_I2, sigma12, cv::Size(11, 11), 1.5);

    sigma1_2 -= mu1_2;
    sigma2_2 -= mu2_2;
    sigma12 -= mu1_mu2;

    cv::Mat t1, t2, t3;
    t1 = 2 * mu1_mu2 + C1;
    t2 = 2 * sigma12 + C2;
    t3 = t1.mul(t2);

    t1 = mu1_2 + mu2_2 + C1;
    t2 = sigma1_2 + sigma2_2 + C2;
    t1 = t1.mul(t2);

    cv::Mat ssim_map;
    cv::divide(t3, t1, ssim_map);
    cv::Scalar mssim = cv::mean(ssim_map);
    return mssim[0];
}

void VideoWidget::openCamera() {
    if (fd >= 0) return;
    fd = open(CAMERA_DEVICE, O_RDWR);
    if (fd < 0) {
        setText("Camera open failed");
        qDebug() << "open device failed:" << strerror(errno);
        return;
    }
    v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width = width;
    fmt.fmt.pix_mp.height = height;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
    fmt.fmt.pix_mp.num_planes = 1;
    if (!xioctl(VIDIOC_S_FMT, &fmt)) {
        qDebug() << "VIDIOC_S_FMT failed";
        closeCamera();
        return;
    }
    stride = fmt.fmt.pix_mp.plane_fmt[0].bytesperline;

    v4l2_requestbuffers req{};
    req.count = BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    req.memory = V4L2_MEMORY_MMAP;
    if (!xioctl(VIDIOC_REQBUFS, &req)) {
        qDebug() << "REQBUFS failed";
        closeCamera();
        return;
    }

    for (unsigned int i = 0; i < req.count; i++) {
        v4l2_buffer buf{};
        v4l2_plane plane{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        buf.length = 1;
        buf.m.planes = &plane;
        if (!xioctl(VIDIOC_QUERYBUF, &buf)) {
            qDebug() << "QUERYBUF failed";
            closeCamera();
            return;
        }
        bufferLength = buf.m.planes[0].length;
        buffers[i] = mmap(NULL, bufferLength, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.planes[0].m.mem_offset);
        if (buffers[i] == MAP_FAILED) {
            qDebug() << "mmap failed";
            closeCamera();
            return;
        }
        if (!xioctl(VIDIOC_QBUF, &buf)) {
            qDebug() << "QBUF failed";
            closeCamera();
            return;
        }
    }

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (!xioctl(VIDIOC_STREAMON, &type)) {
        qDebug() << "STREAMON failed";
        closeCamera();
        return;
    }
    
    if (!m_calibManager.loadFromFile()) {
            qDebug() << "[VideoWidget] No calibration file, using default pixel scale";
    } else {
        qDebug() << "[VideoWidget] Calibration loaded. Scale:" << m_calibManager.getPixelScale()
                 << "Focal:" << m_calibManager.getCameraMatrix().at<double>(0,0);
    }
    
    timer->start(33);
    setText("Camera Running");
    qDebug() << "camera started";
}

void VideoWidget::closeCamera() {
    timer->stop();
    if (fd >= 0) {
        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        ioctl(fd, VIDIOC_STREAMOFF, &type);
        for (int i = 0; i < BUFFER_COUNT; i++) {
            if (buffers[i]) {
                munmap(buffers[i], bufferLength);
                buffers[i] = nullptr;
            }
        }
        ::close(fd);
        fd = -1;
    }
    stopDetect();
    setText("Camera Offline");
    m_hasFrame = false;
}

void VideoWidget::startDetect() {
    if (fd < 0) {
        qDebug() << "camera not open";
        return;
    }
    detecting = true;
    QString filename = QString("displacement_%1.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    m_csvFile = new QFile(filename);
    if (m_csvFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_csvStream = new QTextStream(m_csvFile);
        *m_csvStream << "timestamp,target_id,target_name,dx_mm,dy_mm,confidence,brightness,ssim,distance,status\n";
    }
    m_startTime = QDateTime::currentDateTime();
    qDebug() << "detect start, csv:" << filename;
}

void VideoWidget::stopDetect() {
    detecting = false;
    if (m_csvStream) { delete m_csvStream; m_csvStream = nullptr; }
    if (m_csvFile) { m_csvFile->close(); delete m_csvFile; m_csvFile = nullptr; }
    qDebug() << "detect stop";
}

void VideoWidget::resetZero() {
    for (auto& t : m_targets) {
        if (t.active) {
            t.refCenter = t.currCenter;
            t.dx = 0;
            t.dy = 0;
            m_kalmanTracker.reset(t.id);
            m_consecutiveFrames[t.id] = 0;
        }
    }
    m_statusAnalyzer.resetHistory();
    qDebug() << "zero reset + kalman reset";
}

void VideoWidget::updateAllReferenceImages() {
    if (m_currentFrame.empty()) return;
    for (auto& t : m_targets) {
        if (!t.active || t.roi.isEmpty()) continue;
        cv::Rect cvRoi(t.roi.x(), t.roi.y(), t.roi.width(), t.roi.height());
        cvRoi &= cv::Rect(0, 0, m_currentFrame.cols, m_currentFrame.rows);
        if (cvRoi.width > 0 && cvRoi.height > 0) {
            t.templateImg = m_currentFrame(cvRoi).clone();
            t.refCenter = cv::Point2f(t.roi.x() + t.roi.width() / 2.0, t.roi.y() + t.roi.height() / 2.0);
        }
    }
    emit targetListChanged(m_targets);
    qDebug() << "reference images updated";
}

QPoint VideoWidget::mapToImage(const QPoint& pos) const {
    if (!m_hasFrame) return pos;
    QSize labelSize = size();
    QSize imgSize(m_currentFrame.cols, m_currentFrame.rows);
    imgSize = imgSize.scaled(labelSize, Qt::KeepAspectRatio);
    int offsetX = (labelSize.width() - imgSize.width()) / 2;
    int offsetY = (labelSize.height() - imgSize.height()) / 2;
    int x = (pos.x() - offsetX) * m_currentFrame.cols / imgSize.width();
    int y = (pos.y() - offsetY) * m_currentFrame.rows / imgSize.height();
    x = qBound(0, x, m_currentFrame.cols - 1);
    y = qBound(0, y, m_currentFrame.rows - 1);
    return QPoint(x, y);
}

void VideoWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && m_hasFrame) {
        m_selecting = true;
        m_roiStart = mapToImage(event->pos());
        m_roiEnd = m_roiStart;
    }
    QLabel::mousePressEvent(event);
}

void VideoWidget::mouseMoveEvent(QMouseEvent *event) {
    if (m_selecting) {
        m_roiEnd = mapToImage(event->pos());
    }
    QLabel::mouseMoveEvent(event);
}

void VideoWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (m_selecting && event->button() == Qt::LeftButton) {
        m_selecting = false;
        QRect roi = QRect(m_roiStart, m_roiEnd).normalized();
        if (roi.width() > 20 && roi.height() > 20 && !m_currentFrame.empty()) {
            bool ok;
            QString name = QInputDialog::getText(this, "新建靶标", "靶标名称:", QLineEdit::Normal, QString("传感器%1").arg(m_targets.size()+1), &ok);
            if (ok && !name.isEmpty()) {
                Target t;
                t.id = QString("MK_%1").arg(m_targets.size() + 1, 2, 10, QChar('0'));
                t.name = name;
                t.roi = roi;
                t.active = true;
                cv::Rect cvRoi(roi.x(), roi.y(), roi.width(), roi.height());
                cvRoi &= cv::Rect(0, 0, m_currentFrame.cols, m_currentFrame.rows);
                if (cvRoi.width > 0 && cvRoi.height > 0) {
                    t.templateImg = m_currentFrame(cvRoi).clone();
                    t.refCenter = cv::Point2f(roi.x() + roi.width() / 2.0, roi.y() + roi.height() / 2.0);
                    // 同步标定参数
                    if (m_calibManager.isCalibrated()) {
                        t.mmPerPixel = m_calibManager.getPixelScale();
                        t.calibStatus = "已标定";
                    } else {
                        t.mmPerPixel = 0.05;
                        t.calibStatus = "未标定";
                    }
                    
                    t.createTime = QDateTime::currentDateTime().toString("yyyy/M/d");
                    t.updateTime = t.createTime;
                    m_targets.append(t);
                    emit targetListChanged(m_targets);
                }
            }
        }
    }
    QLabel::mouseReleaseEvent(event);
}

void VideoWidget::captureFrame() {
    if (fd < 0) return;
    v4l2_buffer buf{};
    v4l2_plane plane{};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.length = 1;
    buf.m.planes = &plane;
    if (!xioctl(VIDIOC_DQBUF, &buf)) return;

    uchar *data = static_cast<uchar*>(buffers[buf.index]);
    cv::Mat yuv(height * 3 / 2, stride, CV_8UC1, data);
    cv::Mat rgb;
    cv::cvtColor(yuv, rgb, cv::COLOR_YUV2RGB_NV12);
    m_currentFrame = rgb.clone();
    m_hasFrame = true;

    cv::Mat display = rgb.clone();

    if (detecting && !m_targets.isEmpty()) {
        cv::Mat gray;
        cv::cvtColor(rgb, gray, cv::COLOR_RGB2GRAY);
        for (int i = 0; i < m_targets.size(); i++) {
            auto& t = m_targets[i];
            if (!t.active || t.templateImg.empty()) continue;

            int sx = qMax(0, t.roi.x() - 80);
            int sy = qMax(0, t.roi.y() - 80);
            int ex = qMin(rgb.cols, t.roi.x() + t.roi.width() + 80);
            int ey = qMin(rgb.rows, t.roi.y() + t.roi.height() + 80);
            if (ex <= sx || ey <= sy) continue;

            cv::Rect searchRect(sx, sy, ex - sx, ey - sy);
            cv::Mat searchImg = gray(searchRect);
            cv::Mat tmpl;
            cv::cvtColor(t.templateImg, tmpl, cv::COLOR_RGB2GRAY);
            if (searchImg.cols < tmpl.cols || searchImg.rows < tmpl.rows) continue;

            cv::Mat result;
            cv::matchTemplate(searchImg, tmpl, result, cv::TM_CCOEFF_NORMED);
            double minVal, maxVal;
            cv::Point minLoc, maxLoc;
            cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);

            if (maxVal > 0.55) {
                t.confidence = maxVal * 100.0;
                // ========== 1. 亚像素插值==========
                double subPixelX = 0.0, subPixelY = 0.0;
                int mx = maxLoc.x, my = maxLoc.y;
                if (mx > 0 && mx < result.cols - 1) {
                    double left  = result.at<float>(my, mx - 1);
                    double center = result.at<float>(my, mx);
                    double right = result.at<float>(my, mx + 1);
                    double denom = 2.0 * (2.0 * center - left - right);
                    if (std::abs(denom) > 1e-6) {
                        subPixelX = (right - left) / denom;
                    }
                }
                if (my > 0 && my < result.rows - 1) {
                    double up = result.at<float>(my - 1, mx);
                    double center = result.at<float>(my, mx);
                    double down  = result.at<float>(my + 1, mx);
                    double denom = 2.0 * (2.0 * center - up - down);
                    if (std::abs(denom) > 1e-6) {
                        subPixelY = (down - up) / denom;
                    }
                }

                // ========== 2. 原始测量中心 ==========
                cv::Point2f measuredCenter(
                            searchRect.x + maxLoc.x + subPixelX + tmpl.cols / 2.0f,
                            searchRect.y + maxLoc.y + subPixelY + tmpl.rows / 2.0f
                            );

                // ========== 3. Kalman 滤波平滑 ==========
                cv::Point2f filteredCenter = m_kalmanTracker.update(t.id, measuredCenter);
                t.currCenter = filteredCenter;

                // ========== 4. 使用标定参数计算物理位移 ==========
                double dxPixel = t.currCenter.x - t.refCenter.x;
                double dyPixel = t.currCenter.y - t.refCenter.y;

                if (m_calibManager.isCalibrated() && m_calibManager.getCameraMatrix().at<double>(0,0) > 0) {
                    t.dx = m_calibManager.pixelToMM(dxPixel, m_targetDistance);
                    t.dy = m_calibManager.pixelToMM(dyPixel, m_targetDistance);
                    t.calibStatus = "已标定";
                } else {
                    t.dx = dxPixel * t.mmPerPixel;
                    t.dy = dyPixel * t.mmPerPixel;
                    t.calibStatus = "未标定";
                }

                t.distance = m_targetDistance;

                // ========== 5. 更新 ROI 位置（用于下一帧搜索）==========
                t.roi = QRect(
                            int(t.currCenter.x - t.roi.width() / 2.0),
                            int(t.currCenter.y - t.roi.height() / 2.0),
                            t.roi.width(),
                            t.roi.height()
                            );
                t.updateTime = QDateTime::currentDateTime().toString("yyyy/M/d");

                // ========== 6. 计算亮度与 SSIM ==========
                cv::Rect currRoi(t.roi.x(), t.roi.y(), t.roi.width(), t.roi.height());
                currRoi &= cv::Rect(0, 0, rgb.cols, rgb.rows);
                if (currRoi.width > 0 && currRoi.height > 0) {
                    cv::Mat currROI = rgb(currRoi);
                    t.brightness = calculateBrightness(currROI);
                    t.ssim = calculateSSIM(t.templateImg, currROI);
                }

                // ========== 7. 状态分析==========
                m_statusAnalyzer.updateHistory(t.confidence, t.brightness, t.ssim);
                int frames = m_consecutiveFrames.value(t.id, 0);
                m_consecutiveFrames[t.id] = frames + 1;

                StatusResult sr = m_statusAnalyzer.analyze(t.confidence, t.brightness, t.ssim, t.dx, t.dy, frames);
                t.status = sr.statusText;
                t.statusColor = sr.statusColor;
                emit targetUpdated(i, t.dx, t.dy, t.confidence,t.brightness, t.ssim, t.distance, t.status);
            } else {
                // 匹配失败
                t.confidence = 0;
                t.status = "异常";
                t.statusColor = "#e74c3c";
                m_consecutiveFrames[t.id] = 0;
                emit targetUpdated(i, 0, 0, 0, 0, 0, 0, "异常");
            }
        }
    }

    for (int i = 0; i < m_targets.size(); i++) {
        auto& t = m_targets[i];
        if (!t.active) continue;
        cv::Scalar color = (i == 0) ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 255, 255);
        cv::rectangle(display,
            cv::Point(t.roi.x(), t.roi.y()),
            cv::Point(t.roi.x() + t.roi.width(), t.roi.y() + t.roi.height()),
            color, 2);
        if (t.confidence > 0) {
            cv::circle(display, cv::Point(t.currCenter.x, t.currCenter.y), 5, cv::Scalar(0, 0, 255), -1);
        }
    }

    if (m_selecting) {
        QRect r = QRect(m_roiStart, m_roiEnd).normalized();
        cv::rectangle(display,
            cv::Point(r.x(), r.y()),
            cv::Point(r.x() + r.width(), r.y() + r.height()),
            cv::Scalar(255, 0, 0), 2);
    }

    QSize labelSize = size();
    QPixmap finalPixmap(labelSize);
    finalPixmap.fill(Qt::black);

    QPainter painter(&finalPixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QImage image(display.data, display.cols, display.rows, display.step, QImage::Format_RGB888);
    QPixmap imgPixmap = QPixmap::fromImage(image).scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    int imgX = (labelSize.width() - imgPixmap.width()) / 2;
    int imgY = (labelSize.height() - imgPixmap.height()) / 2;
    painter.drawPixmap(imgX, imgY, imgPixmap);

    double scaleX = imgPixmap.width() / (double)display.cols;
    double scaleY = imgPixmap.height() / (double)display.rows;

    for (int i = 0; i < m_targets.size(); i++) {
        auto& t = m_targets[i];
        if (!t.active || t.confidence <= 0) continue;

        int roiScreenX = imgX + t.roi.x() * scaleX;
        int roiScreenY = imgY + t.roi.y() * scaleY;
        int roiScreenW = t.roi.width() * scaleX;
        int roiScreenH = t.roi.height() * scaleY;

        QString txt = QString("%1: %2,%3mm").arg(t.name).arg(t.dx, 0, 'f', 2).arg(t.dy, 0, 'f', 2);
        QFont font = painter.font();
        font.setPointSize(10);
        font.setBold(true);
        painter.setFont(font);
        QFontMetrics fm(font);
        QRect textRect = fm.boundingRect(txt);
        textRect.adjust(-4, -2, 4, 2);

        int textX = roiScreenX + (roiScreenW - textRect.width()) / 2;
        int textY = roiScreenY - textRect.height() - 1;

        if (textY < 2) textY = roiScreenY + 4;
        if (textX < 2) textX = 2;
        if (textX + textRect.width() > labelSize.width() - 2)
            textX = labelSize.width() - textRect.width() - 2;

        textRect.moveTo(textX, textY);

        painter.fillRect(textRect, QColor(0, 0, 0, 200));
        painter.setPen(QColor(0, 255, 255));
        painter.drawText(textRect, Qt::AlignCenter, txt);
    }

    painter.end();
    setPixmap(finalPixmap);

    xioctl(VIDIOC_QBUF, &buf);

    if (m_csvStream && detecting) {
        double elapsed = m_startTime.msecsTo(QDateTime::currentDateTime()) / 1000.0;
        for (int i = 0; i < m_targets.size(); i++) {
            auto& t = m_targets[i];
            if (!t.active) continue;
            *m_csvStream << QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10\n")
                .arg(elapsed, 0, 'f', 3).arg(t.id).arg(t.name)
                .arg(t.dx, 0, 'f', 4).arg(t.dy, 0, 'f', 4)
                .arg(t.confidence, 0, 'f', 2)
                .arg(t.brightness, 0, 'f', 3)
                .arg(t.ssim, 0, 'f', 3)
                .arg(t.distance, 0, 'f', 1)
                .arg(t.status);
        }
        m_csvStream->flush();
    }
}

double VideoWidget::pixelsToMM(double pixelDelta, int targetIndex) const
{
    if (targetIndex < 0 || targetIndex >= m_targets.size()) return pixelDelta * 0.05;
    const Target& t = m_targets[targetIndex];

    if (m_calibManager.isCalibrated() && m_calibManager.getCameraMatrix().at<double>(0,0) > 0) {
        return m_calibManager.pixelToMM(pixelDelta, m_targetDistance);
    }
    return pixelDelta * t.mmPerPixel;
}












