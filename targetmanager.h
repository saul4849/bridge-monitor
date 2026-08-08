#ifndef TARGETMANAGER_H
#define TARGETMANAGER_H

#include <QWidget>
#include <QList>
#include <QPixmap>
#include <QPoint>
#include "target.h"

class QListWidget;
class QListWidgetItem;
class QLabel;
class QPushButton;
class QGridLayout;

class TargetManager : public QWidget {
    Q_OBJECT
public:
    explicit TargetManager(QWidget *parent = nullptr);
    void setTargets(const QList<Target>& t);
    QList<Target> targets() const { return m_targets; }
    void setCurrentFrame(const QPixmap& frame);

signals:
    void targetsChanged(const QList<Target>& targets);
    void requestUpdateReferenceImage();
    void requestCurrentFrame();  // 请求mainwindow提供当前相机帧

public slots:
    void loadCurrentFrame(const QPixmap& frame);  // 接收当前帧并显示

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onItemClicked(QListWidgetItem* item);
    void onAddTarget();
    void onRemoveTarget();
    void onSaveROI();
    void onUpdateReference();
    void onLoadReference();
    void onManualCheck();
    void onExportConfig();
    void onImportConfig();
    void onRefreshParams();
    void onSaveTargetEdit();

private:
    void refreshList();
    void refreshInfoPanel();
    void refreshImageView();
    void refreshImageViewWithROI();
    int currentIndex() const;
    QPoint mapToImage(const QPoint& pos) const;
    void finishROISelection();

    QListWidget* m_list;
    QLabel* m_imageView;
    QWidget* m_infoPanel;
    QGridLayout* m_infoGrid;

    QPushButton* m_btnLoadRef;
    QPushButton* m_btnUpdateRef;
    QPushButton* m_btnManualCheck;
    QPushButton* m_btnSaveROI;
    QPushButton* m_btnAdd;
    QPushButton* m_btnRemove;
    QPushButton* m_btnEdit;
    QPushButton* m_btnRefresh;
    QPushButton* m_btnExport;
    QPushButton* m_btnImport;

    QList<Target> m_targets;
    QPixmap m_currentFrame;
    QPixmap m_loadedImage;
    int m_selectedIndex = -1;

    bool m_roiSelecting = false;
    QPoint m_roiSelectStart;
    QPoint m_roiSelectEnd;
    bool m_hasROI = false;
};

#endif // TARGETMANAGER_H
