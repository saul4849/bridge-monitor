#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>

#include "videowidget.h"
#include "plotwidget.h"
#include "targetmanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onMenuClicked();
    void onTargetsChanged(const QList<Target>& targets);
    void onTargetUpdated(int index, double dx, double dy, double confidence,
                         double brightness, double ssim, double distance, QString status);
    void onUpdateReferenceImage();

private:
    Ui::MainWindow *ui;
    QWidget* createMenu();
    QWidget* createMonitorPage();
    QWidget* createControlPanel();
    QWidget* createDataTablePanel();
    QWidget* createTargetManagerPage();
    void updateDataTable(int index, const Target& t);

    QPushButton* openCamera;
    QPushButton* startMonitor;
    QPushButton* closeCamera;
    QPushButton* stopMonitor;
    QPushButton* zeroBtn;
    QPushButton* m_btnUpdateRef;

    QStackedWidget *stack;
    VideoWidget* video;
    PlotWidget* m_plot;
    TargetManager* m_targetManager;
    QLabel* m_statusLabel;
    QTableWidget* m_dataTable;

    // 控制面板占位标签
    QLabel* m_lblResolution;
    QLabel* m_lblSampleRate;
    QLabel* m_lblCaptureFPS;
    QLabel* m_lblProcessFPS;
    QLabel* m_lblTotalFrames;
    QLabel* m_lblDetectFrames;
    QLabel* m_lblLightStatus;
};

#endif // MAINWINDOW_H
