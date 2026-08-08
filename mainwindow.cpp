#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QSlider>
#include <QSplitter>
#include <QDateTime>
#include <QDebug>
#include <QLineEdit>
#include <QComboBox>
#include <QMessageBox>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QGridLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->resize(1600, 960);
    this->setWindowTitle("Bridge Health Monitoring System");
    QWidget *central = new QWidget;
    QHBoxLayout *mainLayout = new QHBoxLayout(central);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    QWidget *menu = createMenu();
    stack = new QStackedWidget;
    stack->addWidget(createMonitorPage());
    stack->addWidget(createTargetManagerPage());
    mainLayout->addWidget(menu);
    mainLayout->addWidget(stack, 1);
    setCentralWidget(central);

    connect(openCamera, &QPushButton::clicked, video, &VideoWidget::openCamera);
    connect(closeCamera, &QPushButton::clicked, video, &VideoWidget::closeCamera);
    connect(startMonitor, &QPushButton::clicked, video, &VideoWidget::startDetect);
    connect(stopMonitor, &QPushButton::clicked, video, &VideoWidget::stopDetect);
    connect(zeroBtn, &QPushButton::clicked, video, &VideoWidget::resetZero);
    connect(m_btnUpdateRef, &QPushButton::clicked, this, &MainWindow::onUpdateReferenceImage);

    connect(video, &VideoWidget::targetUpdated, this, &MainWindow::onTargetUpdated);
    connect(video, &VideoWidget::targetListChanged, this, &MainWindow::onTargetsChanged);
    connect(m_targetManager, &TargetManager::targetsChanged, this, &MainWindow::onTargetsChanged);
    connect(m_targetManager, &TargetManager::requestUpdateReferenceImage, this, &MainWindow::onUpdateReferenceImage);

    connect(m_targetManager, &TargetManager::requestCurrentFrame, [this]() {
        QPixmap frame = video->getCurrentFrame();
        m_targetManager->loadCurrentFrame(frame);
    });
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::onMenuClicked() {
    QPushButton* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    QString page = btn->property("page").toString();
    if (page == "实时监控") {
        stack->setCurrentIndex(0);
        video->setTargets(m_targetManager->targets());
    } else if (page == "靶标管理") {
        stack->setCurrentIndex(1);
        m_targetManager->setTargets(video->targets());
    }
}

void MainWindow::onUpdateReferenceImage() {
    video->updateAllReferenceImages();
    m_targetManager->setTargets(video->targets());
    QMessageBox::information(this, "提示", "参考图像已更新");
}

void MainWindow::onTargetsChanged(const QList<Target>& targets) {
    video->setTargets(targets);
    m_targetManager->setTargets(targets);
    QStringList names;
    for (const auto& t : targets) names << t.name;
    m_plot->setTargetNames(names);
    m_plot->clearData();
    m_statusLabel->setText(QString("Camera ONLINE   FPS:30   Target:%1").arg(targets.size()));

    m_dataTable->setRowCount(targets.size());
    for (int i = 0; i < targets.size(); i++) {
        for (int col = 0; col < m_dataTable->columnCount(); col++) {
            if (!m_dataTable->item(i, col)) {
                QTableWidgetItem* item = new QTableWidgetItem();
                item->setTextAlignment(Qt::AlignCenter);
                item->setBackground(QColor("#0c0e1b"));
                item->setForeground(QColor("#ecf0f1"));
                m_dataTable->setItem(i, col, item);
            }
        }
        updateDataTable(i, targets[i]);
    }
}

void MainWindow::onTargetUpdated(int index, double dx, double dy, double confidence,
                                  double brightness, double ssim, double distance, QString status) {
    double t = QDateTime::currentDateTime().toMSecsSinceEpoch() / 1000.0;
    m_plot->addData(index, t, dx, dy);

    if (index >= 0 && index < m_dataTable->rowCount()) {
        auto& targets = video->targets();
        if (index < targets.size()) {
            updateDataTable(index, targets[index]);
        }
    }
}

void MainWindow::updateDataTable(int index, const Target& t) {
    if (index < 0 || index >= m_dataTable->rowCount()) return;

    auto getOrCreateItem = [&](int row, int col) -> QTableWidgetItem* {
        QTableWidgetItem* item = m_dataTable->item(row, col);
        if (!item) {
            item = new QTableWidgetItem();
            item->setTextAlignment(Qt::AlignCenter);
            m_dataTable->setItem(row, col, item);
        }
        return item;
    };

    QColor bg("#0c0e1b");

    QTableWidgetItem* itemId = getOrCreateItem(index, 0);
    itemId->setText(t.id);
    itemId->setBackground(QColor(t.statusColor));
    itemId->setForeground(Qt::white);
    itemId->setFont(QFont("", 10, QFont::Bold));

    getOrCreateItem(index, 1)->setText(QString::number(t.dx, 'f', 3));
    getOrCreateItem(index, 2)->setText(QString::number(t.dy, 'f', 3));
    getOrCreateItem(index, 3)->setText(QString::number(t.brightness, 'f', 3));
    getOrCreateItem(index, 4)->setText(QString::number(t.ssim, 'f', 3));
    getOrCreateItem(index, 5)->setText(QString::number(t.distance, 'f', 1));

    QTableWidgetItem* itemStatus = getOrCreateItem(index, 6);
    itemStatus->setText(t.status);
    itemStatus->setForeground(QColor(t.statusColor));
    itemStatus->setFont(QFont("", 10, QFont::Bold));

    for (int col = 1; col <= 6; col++) {
        m_dataTable->item(index, col)->setBackground(bg);
    }
}

QWidget* MainWindow::createMenu() {
    QWidget *widget = new QWidget;
    widget->setFixedWidth(180);
    widget->setStyleSheet("background:#0c0e1b;");
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setSpacing(6);
    layout->setContentsMargins(8, 16, 8, 16);

    QLabel *title = new QLabel("桥梁监测");
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size:17px; font-weight:bold; color:#00d2d3; margin-bottom:12px;");
    layout->addWidget(title);

    QStringList menus = {"实时监控", "靶标管理", "数据分析", "系统设置"};
    for (QString text : menus) {
        QPushButton *btn = new QPushButton(text);
        btn->setMinimumHeight(48);
        btn->setProperty("page", text);
        btn->setStyleSheet(
            "QPushButton { background:transparent; color:#a4b0be; border:none; border-radius:6px; padding:10px; font-size:14px; text-align:left; padding-left:20px; }"
            "QPushButton:hover { background:#1e3799; color:#ecf0f1; }"
            "QPushButton:pressed { background:#00d2d3; color:#1a1a2e; }"
        );
        connect(btn, &QPushButton::clicked, this, &MainWindow::onMenuClicked);
        layout->addWidget(btn);
    }
    layout->addStretch();

    QLabel* admin = new QLabel("管理员\n退出");
    admin->setStyleSheet("color:#7f8c8d; font-size:12px; padding:10px;");
    layout->addWidget(admin);
    return widget;
}

QWidget* MainWindow::createMonitorPage() {
    QWidget *page = new QWidget;
    page->setStyleSheet("background:#1a1a2e;");
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    QHBoxLayout* topBar = new QHBoxLayout;
    m_statusLabel = new QLabel("Camera ONLINE   FPS:30   Target:0");
    m_statusLabel->setStyleSheet("color:#2ecc71; font-weight:bold; font-size:13px;");
    topBar->addWidget(m_statusLabel);
    topBar->addStretch();
    QLabel* sysInfo = new QLabel("CPU: --  内存: --  已连接");
    sysInfo->setStyleSheet("color:#7f8c8d; font-size:12px;");
    topBar->addWidget(sysInfo);
    mainLayout->addLayout(topBar);

    QHBoxLayout *body = new QHBoxLayout;
    body->setSpacing(12);

    QWidget *center = new QWidget;
    QVBoxLayout *centerLayout = new QVBoxLayout(center);
    centerLayout->setSpacing(8);
    centerLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *cameraTitle = new QLabel("实时监控");
    cameraTitle->setStyleSheet("font-size:16px; font-weight:bold; color:#ecf0f1;");
    centerLayout->addWidget(cameraTitle);

    video = new VideoWidget;
    video->setMinimumSize(500, 320);
    video->setStyleSheet("background:#0c0e1b; border:1px solid #0f3460; border-radius:6px;");
    centerLayout->addWidget(video, 1);

    QLabel *curveTitle = new QLabel("实时数据曲线");
    curveTitle->setStyleSheet("font-size:16px; font-weight:bold; color:#ecf0f1; margin-top:6px;");
    centerLayout->addWidget(curveTitle);

    m_plot = new PlotWidget;
    m_plot->setMinimumHeight(180);
    centerLayout->addWidget(m_plot, 0);

    body->addWidget(center, 4);

    QVBoxLayout* rightLayout = new QVBoxLayout;
    rightLayout->setSpacing(10);
    rightLayout->addWidget(createControlPanel(), 0);
    rightLayout->addWidget(createDataTablePanel(), 1);
    body->addLayout(rightLayout, 0);

    mainLayout->addLayout(body, 1);
    return page;
}

QWidget* MainWindow::createControlPanel() {
    QWidget *panel = new QWidget;
    panel->setFixedWidth(420);
    panel->setStyleSheet("background:#16213e; border-radius:8px;");
    QVBoxLayout *mainLayout = new QVBoxLayout(panel);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    QLabel *title = new QLabel("系统控制");
    title->setStyleSheet("font-size:15px; font-weight:bold; color:#00d2d3; margin-bottom:8px;");
    mainLayout->addWidget(title);

    // 左右分栏
    QHBoxLayout *hLayout = new QHBoxLayout;
    hLayout->setSpacing(10);

    // 左列：按钮，均匀分布占满高度
    QVBoxLayout *leftCol = new QVBoxLayout;
    leftCol->setSpacing(0);
    auto makeBtn = [&](const QString& text) -> QPushButton* {
        QPushButton* btn = new QPushButton(text);
        btn->setMinimumHeight(42);
        btn->setStyleSheet(
            "QPushButton { background:#0f3460; color:#ecf0f1; border:1px solid #00d2d3; border-radius:4px; padding:4px; font-size:13px; }"
            "QPushButton:hover { background:#00d2d3; color:#1a1a2e; font-weight:bold; }"
        );
        return btn;
    };
    openCamera = makeBtn("打开相机");
    closeCamera = makeBtn("关闭相机");
    startMonitor = makeBtn("开始监测");
    stopMonitor = makeBtn("停止检测");
    zeroBtn = makeBtn("零点校准");
    m_btnUpdateRef = makeBtn("更新参考图");
    leftCol->addWidget(openCamera, 1);
    leftCol->addSpacing(6);
    leftCol->addWidget(closeCamera, 1);
    leftCol->addSpacing(6);
    leftCol->addWidget(startMonitor, 1);
    leftCol->addSpacing(6);
    leftCol->addWidget(stopMonitor, 1);
    leftCol->addSpacing(6);
    leftCol->addWidget(zeroBtn, 1);
    leftCol->addSpacing(6);
    leftCol->addWidget(m_btnUpdateRef, 1);
    hLayout->addLayout(leftCol, 1);

    // 右列：参数 + 滑条 + 设备信息，均匀分布占满高度
    QVBoxLayout *rightCol = new QVBoxLayout;
    rightCol->setSpacing(0);

    auto makeParamLabel = [&](const QString& text, const QString& value, bool highlight = false) -> QLabel* {
        QLabel* lbl = new QLabel(QString("%1: %2").arg(text).arg(value));
        lbl->setStyleSheet(highlight ?
            "color:#00d2d3; font-size:12px; font-weight:bold;" :
            "color:#a4b0be; font-size:12px;");
        return lbl;
    };
    rightCol->addWidget(makeParamLabel("分辨率", "3840x2160", true), 1);
    rightCol->addSpacing(4);
    rightCol->addWidget(makeParamLabel("采样率", "30 Hz"), 1);

    QLabel* lblExp = new QLabel("曝光");
    lblExp->setStyleSheet("color:#a4b0be; font-size:12px; margin-top:4px;");
    rightCol->addWidget(lblExp, 1);
    QSlider *slider = new QSlider(Qt::Horizontal);
    slider->setRange(0, 100);
    slider->setValue(50);
    slider->setStyleSheet(
        "QSlider::groove:horizontal { height:4px; background:#0f3460; border-radius:2px; }"
        "QSlider::handle:horizontal { width:12px; background:#00d2d3; border-radius:6px; margin:-4px 0; }"
    );
    rightCol->addWidget(slider, 1);

    QLabel* lblGain = new QLabel("增益");
    lblGain->setStyleSheet("color:#a4b0be; font-size:12px;");
    rightCol->addWidget(lblGain, 1);
    QSlider *gainSlider = new QSlider(Qt::Horizontal);
    gainSlider->setRange(0, 100);
    gainSlider->setValue(20);
    gainSlider->setStyleSheet(
        "QSlider::groove:horizontal { height:4px; background:#0f3460; border-radius:2px; }"
        "QSlider::handle:horizontal { width:12px; background:#00d2d3; border-radius:6px; margin:-4px 0; }"
    );
    rightCol->addWidget(gainSlider, 1);

    QLabel* devTitle = new QLabel("设备信息");
    devTitle->setStyleSheet("font-size:13px; font-weight:bold; color:#00d2d3; margin-top:4px;");
    rightCol->addWidget(devTitle, 1);

    auto makeDevLabel = [&](const QString& text, const QString& value) -> QLabel* {
        QLabel* lbl = new QLabel(QString("%1: %2").arg(text).arg(value));
        lbl->setStyleSheet("color:#a4b0be; font-size:12px;");
        return lbl;
    };
    m_lblCaptureFPS = makeDevLabel("采集帧率", "30.0 FPS");
    m_lblProcessFPS = makeDevLabel("处理帧率", "-- FPS");
    m_lblTotalFrames = makeDevLabel("总帧数", "--");
    m_lblDetectFrames = makeDevLabel("检测帧数", "--");
    rightCol->addWidget(m_lblCaptureFPS, 1);
    rightCol->addSpacing(4);
    rightCol->addWidget(m_lblProcessFPS, 1);
    rightCol->addSpacing(4);
    rightCol->addWidget(m_lblTotalFrames, 1);
    rightCol->addSpacing(4);
    rightCol->addWidget(m_lblDetectFrames, 1);

    hLayout->addLayout(rightCol, 1);
    mainLayout->addLayout(hLayout, 1);
    return panel;
}

QWidget* MainWindow::createDataTablePanel() {
    QWidget *panel = new QWidget;
    panel->setFixedWidth(420);
    panel->setStyleSheet("background:#16213e; border-radius:8px;");
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setSpacing(8);
    layout->setContentsMargins(12, 12, 12, 12);

    QLabel *title = new QLabel("实时监测数据");
    title->setStyleSheet("font-size:15px; font-weight:bold; color:#00d2d3; margin-bottom:6px;");
    layout->addWidget(title);

    m_dataTable = new QTableWidget(0, 7);
    m_dataTable->setStyleSheet(
        "QTableWidget { background:#0c0e1b; border:1px solid #0f3460; border-radius:6px; color:#ecf0f1; font-size:11px; gridline-color:#0f3460; }"
        "QTableWidget::item { background:#0c0e1b; color:#ecf0f1; padding:4px; }"
        "QHeaderView::section { background:#0f3460; color:#00d2d3; padding:6px; border:1px solid #1e3799; font-weight:bold; font-size:11px; }"
    );
    m_dataTable->setHorizontalHeaderLabels({
        "靶标", "横向\n(mm)", "竖向\n(mm)", "亮度比", "SSIM", "距离\n(mm)", "状态"
    });
    for (int i = 0; i < m_dataTable->columnCount(); i++) {
        QTableWidgetItem* headerItem = m_dataTable->horizontalHeaderItem(i);
        if (headerItem) headerItem->setTextAlignment(Qt::AlignCenter);
    }
    m_dataTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_dataTable->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    m_dataTable->verticalHeader()->setVisible(false);
    m_dataTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_dataTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_dataTable->setAlternatingRowColors(false);

    
    m_dataTable->setColumnWidth(0, 70);   // 靶标：缩5
    m_dataTable->setColumnWidth(1, 52);   // 横向
    m_dataTable->setColumnWidth(2, 52);   // 竖向
    m_dataTable->setColumnWidth(3, 52);   // 亮度比
    m_dataTable->setColumnWidth(4, 56);   // SSIM：加宽
    m_dataTable->setColumnWidth(5, 54);   // 距离
    m_dataTable->setColumnWidth(6, 56);   // 状态：缩7

    layout->addWidget(m_dataTable, 1);
    return panel;
}

QWidget* MainWindow::createTargetManagerPage() {
    m_targetManager = new TargetManager;
    m_targetManager->setStyleSheet("background:#1a1a2e;");
    return m_targetManager;
}
