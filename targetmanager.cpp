#include "targetmanager.h"
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QImage>
#include <QPainter>
#include <QGridLayout>
#include <QMouseEvent>
#include <QDebug>
#include <QDateTime>

TargetManager::TargetManager(QWidget *parent) : QWidget(parent) {
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setSpacing(10);
    outer->setContentsMargins(12, 12, 12, 12);

    QHBoxLayout* topLayout = new QHBoxLayout;
    topLayout->setSpacing(12);

    QVBoxLayout* leftLayout = new QVBoxLayout;
    leftLayout->setSpacing(8);
    QLabel* lblListTitle = new QLabel("靶标");
    lblListTitle->setStyleSheet("font-size:15px; font-weight:bold; color:#00d2d3;");
    leftLayout->addWidget(lblListTitle);

    m_list = new QListWidget;
    m_list->setMinimumWidth(200);
    m_list->setMaximumWidth(260);
    m_list->setStyleSheet(
        "QListWidget { background:#16213e; border:1px solid #0f3460; border-radius:6px; font-size:13px; color:#ecf0f1; }"
        "QListWidget::item { padding:10px; border-bottom:1px solid #0f3460; }"
        "QListWidget::item:selected { background:#00d2d3; color:#1a1a2e; }"
        "QListWidget::item:hover { background:#1e3799; }"
    );
    leftLayout->addWidget(m_list, 1);
    topLayout->addLayout(leftLayout, 0);

    QVBoxLayout* centerLayout = new QVBoxLayout;
    centerLayout->setSpacing(8);
    QLabel* lblImgTitle = new QLabel("图像显示区域");
    lblImgTitle->setStyleSheet("font-size:15px; font-weight:bold; color:#00d2d3;");
    centerLayout->addWidget(lblImgTitle);

    m_imageView = new QLabel;
    m_imageView->setMinimumSize(480, 360);
    m_imageView->setAlignment(Qt::AlignCenter);
    m_imageView->setStyleSheet(
        "background:#0c0e1b; border:1px solid #0f3460; border-radius:6px; color:#7f8c8d; font-size:14px;"
    );
    m_imageView->setText("暂无参考图像");
    m_imageView->installEventFilter(this);
    centerLayout->addWidget(m_imageView, 1);
    topLayout->addLayout(centerLayout, 1);

    QVBoxLayout* rightLayout = new QVBoxLayout;
    rightLayout->setSpacing(10);
    rightLayout->setAlignment(Qt::AlignTop);

    auto makeBtn = [&](const QString& text, const QString& color) -> QPushButton* {
        QPushButton* btn = new QPushButton(text);
        btn->setMinimumHeight(36);
        btn->setStyleSheet(QString(
            "QPushButton { background:%1; color:#ecf0f1; border:1px solid %2; border-radius:4px; padding:6px; font-size:12px; }"
            "QPushButton:hover { background:%2; color:#1a1a2e; }"
        ).arg(color.split(",").first()).arg(color.split(",").last()));
        return btn;
    };

    QLabel* lblRef = new QLabel("参考图像操作");
    lblRef->setStyleSheet("font-size:14px; font-weight:bold; color:#00d2d3;");
    rightLayout->addWidget(lblRef);

    m_btnLoadRef = makeBtn("加载参考图", "#0f3460,#00d2d3");
    m_btnUpdateRef = makeBtn("更新参考图像", "#0f3460,#00d2d3");
    m_btnManualCheck = makeBtn("人工校对", "#0f3460,#00d2d3");
    m_btnSaveROI = makeBtn("保存ROI区域", "#0f3460,#00d2d3");
    m_btnSaveROI->setEnabled(false);
    for (auto* btn : {m_btnLoadRef, m_btnUpdateRef, m_btnManualCheck, m_btnSaveROI})
        rightLayout->addWidget(btn);

    rightLayout->addSpacing(15);
    QLabel* lblMgmt = new QLabel("靶标管理");
    lblMgmt->setStyleSheet("font-size:14px; font-weight:bold; color:#00d2d3;");
    rightLayout->addWidget(lblMgmt);

    m_btnAdd = makeBtn("新增靶标", "#0f3460,#00d2d3");
    m_btnRemove = makeBtn("删除靶标", "#0f3460,#00d2d3");
    m_btnEdit = makeBtn("修改靶标", "#0f3460,#00d2d3");
    m_btnRefresh = makeBtn("刷新参数", "#0f3460,#00d2d3");
    for (auto* btn : {m_btnAdd, m_btnRemove, m_btnEdit, m_btnRefresh})
        rightLayout->addWidget(btn);

    rightLayout->addSpacing(15);
    QLabel* lblCfg = new QLabel("配置管理");
    lblCfg->setStyleSheet("font-size:14px; font-weight:bold; color:#00d2d3;");
    rightLayout->addWidget(lblCfg);

    m_btnExport = makeBtn("导出配置", "#6c5ce7,#a29bfe");
    m_btnImport = makeBtn("导入配置", "#6c5ce7,#a29bfe");
    rightLayout->addWidget(m_btnExport);
    rightLayout->addWidget(m_btnImport);
    rightLayout->addStretch();
    topLayout->addLayout(rightLayout, 0);

    outer->addLayout(topLayout, 1);

    QHBoxLayout* infoTitleLayout = new QHBoxLayout;
    QLabel* lblInfo = new QLabel("靶标信息");
    lblInfo->setStyleSheet("font-size:15px; font-weight:bold; color:#00d2d3; margin-top:8px;");
    infoTitleLayout->addWidget(lblInfo);
    infoTitleLayout->addStretch();
    outer->addLayout(infoTitleLayout);

    m_infoPanel = new QWidget;
    m_infoPanel->setStyleSheet("background:#16213e; border:1px solid #0f3460; border-radius:8px;");
    QVBoxLayout* infoOuterLayout = new QVBoxLayout(m_infoPanel);
    infoOuterLayout->setSpacing(0);
    infoOuterLayout->setContentsMargins(16, 12, 16, 12);

    m_infoGrid = new QGridLayout;
    m_infoGrid->setSpacing(8);
    m_infoGrid->setColumnStretch(1, 1);
    m_infoGrid->setColumnStretch(3, 1);
    m_infoGrid->setColumnStretch(5, 1);
    m_infoGrid->setColumnStretch(7, 1);
    infoOuterLayout->addLayout(m_infoGrid);
    outer->addWidget(m_infoPanel, 0);

    connect(m_list, &QListWidget::itemClicked, this, &TargetManager::onItemClicked);
    connect(m_btnAdd, &QPushButton::clicked, this, &TargetManager::onAddTarget);
    connect(m_btnRemove, &QPushButton::clicked, this, &TargetManager::onRemoveTarget);
    connect(m_btnSaveROI, &QPushButton::clicked, this, &TargetManager::onSaveROI);
    connect(m_btnUpdateRef, &QPushButton::clicked, this, &TargetManager::onUpdateReference);
    connect(m_btnLoadRef, &QPushButton::clicked, this, &TargetManager::onLoadReference);
    connect(m_btnManualCheck, &QPushButton::clicked, this, &TargetManager::onManualCheck);
    connect(m_btnExport, &QPushButton::clicked, this, &TargetManager::onExportConfig);
    connect(m_btnImport, &QPushButton::clicked, this, &TargetManager::onImportConfig);
    connect(m_btnRefresh, &QPushButton::clicked, this, &TargetManager::onRefreshParams);
    connect(m_btnEdit, &QPushButton::clicked, this, &TargetManager::onSaveTargetEdit);
}

// ========== 接收当前帧并显示 ==========
void TargetManager::loadCurrentFrame(const QPixmap& frame) {
    if (frame.isNull()) {
        QMessageBox::warning(this, "提示", "相机未开启或暂无图像，请先打开相机");
        return;
    }
    m_loadedImage = frame;
    m_roiSelectStart = QPoint();
    m_roiSelectEnd = QPoint();
    m_hasROI = false;
    m_btnSaveROI->setEnabled(false);
    refreshImageViewWithROI();
    QMessageBox::information(this, "提示",
        "当前监控画面已加载\n请在图像上按住鼠标左键框选ROI区域，\n然后点击'保存ROI区域'按钮保存。");
}

bool TargetManager::eventFilter(QObject* obj, QEvent* event) {
    if (obj != m_imageView) return QWidget::eventFilter(obj, event);
    if (m_loadedImage.isNull()) return QWidget::eventFilter(obj, event);

    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            m_roiSelecting = true;
            m_roiSelectStart = mapToImage(me->pos());
            m_roiSelectEnd = m_roiSelectStart;
            m_hasROI = false;
            m_btnSaveROI->setEnabled(false);
            refreshImageViewWithROI();
            return true;
        }
    }
    else if (event->type() == QEvent::MouseMove) {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        if (m_roiSelecting) {
            m_roiSelectEnd = mapToImage(me->pos());
            refreshImageViewWithROI();
            return true;
        }
    }
    else if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        if (m_roiSelecting && me->button() == Qt::LeftButton) {
            m_roiSelecting = false;
            m_roiSelectEnd = mapToImage(me->pos());
            QRect roi = QRect(m_roiSelectStart, m_roiSelectEnd).normalized();
            if (roi.width() > 10 && roi.height() > 10) {
                m_hasROI = true;
                m_btnSaveROI->setEnabled(true);
                refreshImageViewWithROI();
            } else {
                m_hasROI = false;
                m_btnSaveROI->setEnabled(false);
                m_roiSelectStart = QPoint();
                m_roiSelectEnd = QPoint();
                refreshImageView();
            }
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

QPoint TargetManager::mapToImage(const QPoint& pos) const {
    if (m_loadedImage.isNull()) return pos;
    QSize labelSize = m_imageView->size();
    QSize imgSize = m_loadedImage.size();
    imgSize = imgSize.scaled(labelSize, Qt::KeepAspectRatio);
    int offsetX = (labelSize.width() - imgSize.width()) / 2;
    int offsetY = (labelSize.height() - imgSize.height()) / 2;
    int x = (pos.x() - offsetX) * m_loadedImage.width() / imgSize.width();
    int y = (pos.y() - offsetY) * m_loadedImage.height() / imgSize.height();
    x = qBound(0, x, m_loadedImage.width() - 1);
    y = qBound(0, y, m_loadedImage.height() - 1);
    return QPoint(x, y);
}

void TargetManager::refreshImageViewWithROI() {
    if (m_loadedImage.isNull()) return;
    QSize labelSize = m_imageView->size();
    QPixmap finalPixmap(labelSize);
    finalPixmap.fill(Qt::black);

    QPainter painter(&finalPixmap);
    QPixmap scaled = m_loadedImage.scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    int imgX = (labelSize.width() - scaled.width()) / 2;
    int imgY = (labelSize.height() - scaled.height()) / 2;
    painter.drawPixmap(imgX, imgY, scaled);

    if (m_hasROI || m_roiSelecting) {
        double sx = scaled.width() / (double)m_loadedImage.width();
        double sy = scaled.height() / (double)m_loadedImage.height();
        QRect r = QRect(m_roiSelectStart, m_roiSelectEnd).normalized();
        int rx = imgX + r.x() * sx;
        int ry = imgY + r.y() * sy;
        int rw = r.width() * sx;
        int rh = r.height() * sy;
        painter.setPen(QPen(QColor(0, 255, 255), 2));
        painter.drawRect(rx, ry, rw, rh);
    }
    painter.end();
    m_imageView->setPixmap(finalPixmap);
}

// ==========  QImage -> OpenCV Mat 转换 ==========
void TargetManager::finishROISelection() {
    int idx = currentIndex();
    if (idx < 0 || m_loadedImage.isNull() || !m_hasROI) return;

    QRect roi = QRect(m_roiSelectStart, m_roiSelectEnd).normalized();
    roi = roi.intersected(QRect(0, 0, m_loadedImage.width(), m_loadedImage.height()));
    if (roi.width() <= 0 || roi.height() <= 0) return;

    // 1. 从 QPixmap 裁剪 ROI 区域
    QPixmap roiPixmap = m_loadedImage.copy(roi);
    if (roiPixmap.isNull()) {
        QMessageBox::warning(this, "错误", "ROI裁剪失败");
        return;
    }

    // 2. 转为 QImage 并确保是 RGB888 格式
    QImage roiImage = roiPixmap.toImage().convertToFormat(QImage::Format_RGB888);
    if (roiImage.isNull() || roiImage.bits() == nullptr) {
        QMessageBox::warning(this, "错误", "ROI图像数据无效");
        return;
    }

    // 3. 逐行复制到 OpenCV Mat（最可靠的方式，避免内存对齐问题）
    cv::Mat mat(roiImage.height(), roiImage.width(), CV_8UC3);
    for (int y = 0; y < roiImage.height(); ++y) {
        const uchar* src = roiImage.scanLine(y);
        uchar* dst = mat.ptr(y);
        memcpy(dst, src, roiImage.width() * 3);
    }

    // 4. clone 确保数据独立
    m_targets[idx].templateImg = mat.clone();

    // 5. 更新靶标 ROI 和中心点
    m_targets[idx].roi = roi;
    m_targets[idx].refCenter = cv::Point2f(
        roi.x() + roi.width() / 2.0f,
        roi.y() + roi.height() / 2.0f
    );
    m_targets[idx].updateTime = QDateTime::currentDateTime().toString("yyyy/M/d");

    QMessageBox::information(this, "成功",
        QString("ROI已保存\n靶标: %1\nROI: %2x%3 @(%4,%5)")
        .arg(m_targets[idx].name)
        .arg(roi.width()).arg(roi.height())
        .arg(roi.x()).arg(roi.y()));

    m_hasROI = false;
    m_btnSaveROI->setEnabled(false);
    m_roiSelectStart = QPoint();
    m_roiSelectEnd = QPoint();

    refreshImageView();
    refreshInfoPanel();
    emit targetsChanged(m_targets);
}

void TargetManager::setTargets(const QList<Target>& t) {
    m_targets = t;
    refreshList();
    refreshInfoPanel();
}

void TargetManager::setCurrentFrame(const QPixmap& frame) {
    m_currentFrame = frame;
    refreshImageView();
}

int TargetManager::currentIndex() const {
    int row = m_list->currentRow();
    return (row >= 0 && row < m_targets.size()) ? row : -1;
}

void TargetManager::refreshList() {
    m_list->clear();
    for (int i = 0; i < m_targets.size(); i++) {
        const auto& t = m_targets[i];
        QString status = t.active ? "[active]" : "[inactive]";
        m_list->addItem(QString("传感器%1[%2] %3").arg(i+1).arg(t.id).arg(status));
    }
}

void TargetManager::refreshImageView() {
    int idx = currentIndex();
    if (idx < 0 || idx >= m_targets.size()) {
        m_imageView->setText("暂无参考图像");
        m_loadedImage = QPixmap();
        m_btnSaveROI->setEnabled(false);
        return;
    }
    const auto& t = m_targets[idx];
    if (!t.templateImg.empty()) {
        cv::Mat rgb = t.templateImg.clone();
        int displayW = 480;
        int displayH = 360;
        double scale = qMin(displayW / (double)rgb.cols, displayH / (double)rgb.rows);
        if (scale > 1.0) {
            cv::resize(rgb, rgb, cv::Size(), scale, scale, cv::INTER_NEAREST);
        }
        QImage img(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
        m_imageView->setPixmap(QPixmap::fromImage(img.copy()).scaled(
            m_imageView->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else if (!m_currentFrame.isNull()) {
        m_imageView->setPixmap(m_currentFrame.scaled(
            m_imageView->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        m_imageView->setText("暂无参考图像");
    }
}

static QLabel* makeLabel(const QString& text, bool isValue = false) {
    QLabel* lbl = new QLabel(text);
    if (isValue) {
        lbl->setStyleSheet("color:#ecf0f1; font-size:12px; padding:4px 8px;");
    } else {
        lbl->setStyleSheet("color:#00d2d3; font-size:12px; padding:4px 8px;");
    }
    lbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return lbl;
}

void TargetManager::refreshInfoPanel() {
    QLayoutItem* item;
    while ((item = m_infoGrid->takeAt(0)) != nullptr) {
        if (item->widget()) delete item->widget();
        delete item;
    }

    int idx = currentIndex();
    if (idx < 0 || idx >= m_targets.size()) {
        QLabel* empty = new QLabel("暂无靶标信息，请先在实时监控界面框选靶标");
        empty->setStyleSheet("color:#7f8c8d; font-size:13px; padding:20px;");
        empty->setAlignment(Qt::AlignCenter);
        m_infoGrid->addWidget(empty, 0, 0, 1, 8);
        return;
    }

    const auto& t = m_targets[idx];
    struct Field { QString label; QString value; };
    QList<Field> fields = {
        {"靶标代码:", t.id}, {"靶标名称:", t.name},
        {"靶标类型:", t.targetType}, {"状态:", t.active ? "active" : "inactive"},
        {"初始位置X:", QString::number(t.refCenter.x, 'f', 2) + "像素"},
        {"初始位置Y:", QString::number(t.refCenter.y, 'f', 2) + "像素"},
        {"像素宽度:", QString::number(t.roi.width()) + "像素"},
        {"像素高度:", QString::number(t.roi.height()) + "像素"},
        {"物理宽度:", QString::number(t.roi.width() * t.mmPerPixel, 'f', 1) + "mm"},
        {"物理高度:", QString::number(t.roi.height() * t.mmPerPixel, 'f', 1) + "mm"},
        {"像素比率:", QString::number(t.mmPerPixel, 'f', 6)},
        {"ROI状态:", t.roi.isEmpty() ? "未设置" : "已设置"},
        {"参考中心:", t.isRefCenter ? "是" : "否"},
        {"校准状态:", t.calibStatus},
        {"创建时间:", t.createTime.isEmpty() ? "-" : t.createTime},
        {"更新时间:", t.updateTime.isEmpty() ? "-" : t.updateTime},
    };

    int row = 0, col = 0;
    for (const auto& f : fields) {
        m_infoGrid->addWidget(makeLabel(f.label, false), row, col * 2);
        m_infoGrid->addWidget(makeLabel(f.value, true), row, col * 2 + 1);
        col++;
        if (col >= 4) { col = 0; row++; }
    }
}

void TargetManager::onItemClicked(QListWidgetItem*) {
    m_loadedImage = QPixmap();
    m_roiSelectStart = QPoint();
    m_roiSelectEnd = QPoint();
    m_hasROI = false;
    m_btnSaveROI->setEnabled(false);
    refreshImageView();
    refreshInfoPanel();
}

void TargetManager::onAddTarget() {
    bool ok;
    QString name = QInputDialog::getText(this, "新增靶标", "靶标名称:", QLineEdit::Normal, QString("传感器%1").arg(m_targets.size()+1), &ok);
    if (!ok || name.isEmpty()) return;
    Target t;
    t.id = QString("MK_%1").arg(m_targets.size() + 1, 2, 10, QChar('0'));
    t.name = name;
    t.active = true;
    t.mmPerPixel = 0.05;
    t.createTime = QDateTime::currentDateTime().toString("yyyy/M/d");
    t.updateTime = t.createTime;
    m_targets.append(t);
    refreshList();
    refreshInfoPanel();
    emit targetsChanged(m_targets);
}

void TargetManager::onRemoveTarget() {
    int idx = currentIndex();
    if (idx < 0) { QMessageBox::warning(this, "提示", "请先选中要删除的靶标"); return; }
    if (QMessageBox::question(this, "确认", "确定删除该靶标吗?") != QMessageBox::Yes) return;
    m_targets.removeAt(idx);
    for (int i = 0; i < m_targets.size(); i++)
        m_targets[i].id = QString("MK_%1").arg(i + 1, 2, 10, QChar('0'));
    refreshList();
    refreshInfoPanel();
    refreshImageView();
    emit targetsChanged(m_targets);
}

void TargetManager::onSaveROI() {
    if (m_loadedImage.isNull()) {
        QMessageBox::warning(this, "提示", "请先加载参考图");
        return;
    }
    if (!m_hasROI) {
        QMessageBox::warning(this, "提示", "请先在图片上框选ROI区域");
        return;
    }
    finishROISelection();
}

void TargetManager::onUpdateReference() {
    emit requestUpdateReferenceImage();
    QMessageBox::information(this, "提示", "已发送更新参考图像请求\n请在实时监控界面点击'更新参考'");
}

// ========== 请求当前相机帧 ==========
void TargetManager::onLoadReference() {
    int idx = currentIndex();
    if (idx < 0) { QMessageBox::warning(this, "提示", "请先选中靶标"); return; }
    emit requestCurrentFrame();  // 让mainwindow从videowidget获取当前帧
}

void TargetManager::onManualCheck() {
    int idx = currentIndex();
    if (idx < 0) { QMessageBox::warning(this, "提示", "请先选中靶标"); return; }
    QMessageBox::information(this, "人工校对",
        QString("靶标: %1\n当前置信度: %2%\n状态: %3")
        .arg(m_targets[idx].name)
        .arg(m_targets[idx].confidence, 0, 'f', 1)
        .arg(m_targets[idx].active ? "正常" : "停用"));
}

void TargetManager::onExportConfig() {
    QString path = QFileDialog::getSaveFileName(this, "导出配置", "targets_config.json", "JSON (*.json)");
    if (path.isEmpty()) return;
    QJsonArray arr;
    for (const auto& t : m_targets) {
        QJsonObject obj;
        obj["id"] = t.id; obj["name"] = t.name; obj["active"] = t.active;
        obj["roi_x"] = t.roi.x(); obj["roi_y"] = t.roi.y();
        obj["roi_w"] = t.roi.width(); obj["roi_h"] = t.roi.height();
        obj["mmPerPixel"] = t.mmPerPixel;
        obj["targetType"] = t.targetType;
        obj["createTime"] = t.createTime;
        obj["updateTime"] = t.updateTime;
        arr.append(obj);
    }
    QJsonObject root; root["targets"] = arr;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(root).toJson());
        f.close();
        QMessageBox::information(this, "成功", "配置已导出");
    }
}

void TargetManager::onImportConfig() {
    QString path = QFileDialog::getOpenFileName(this, "导入配置", "", "JSON (*.json)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    f.close();
    QJsonArray arr = root["targets"].toArray();
    m_targets.clear();
    for (const auto& v : arr) {
        QJsonObject obj = v.toObject();
        Target t;
        t.id = obj["id"].toString();
        t.name = obj["name"].toString();
        t.active = obj["active"].toBool();
        t.roi = QRect(obj["roi_x"].toInt(), obj["roi_y"].toInt(),
                      obj["roi_w"].toInt(), obj["roi_h"].toInt());
        t.mmPerPixel = obj["mmPerPixel"].toDouble(0.05);
        t.targetType = obj["targetType"].toString("参考靶标");
        t.createTime = obj["createTime"].toString();
        t.updateTime = obj["updateTime"].toString();
        m_targets.append(t);
    }
    refreshList();
    refreshInfoPanel();
    refreshImageView();
    emit targetsChanged(m_targets);
    QMessageBox::information(this, "成功", "配置已导入");
}

void TargetManager::onRefreshParams() {
    refreshList();
    refreshInfoPanel();
    refreshImageView();
}

void TargetManager::onSaveTargetEdit() {
    int idx = currentIndex();
    if (idx < 0) { QMessageBox::warning(this, "提示", "请先选中靶标"); return; }
    bool ok;
    QString name = QInputDialog::getText(this, "修改靶标", "靶标名称:", QLineEdit::Normal, m_targets[idx].name, &ok);
    if (ok && !name.isEmpty()) {
        m_targets[idx].name = name;
        m_targets[idx].updateTime = QDateTime::currentDateTime().toString("yyyy/M/d");
        refreshList();
        refreshInfoPanel();
        emit targetsChanged(m_targets);
    }
}
