#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QSlider>
#include <QLabel>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include "powertopology.h"
#include "telnetclient.h"
#include "logwindow.h"
#include "topologyselectdialog.h"
QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    explicit MainWindow(TOPOTYPE topologyType, QWidget *parent = nullptr);
    enum ItemDataKey
    {
        keyNode1,
        keyNode2,
        vislevel,
    };
private slots:
    void onApplyConfigClicked();
    void onRequestPowerClicked();
    void onReleasePowerClicked();
    void onStopChargeClicked();
    void onPileSelectionChanged(int index);
    void onTopologyChanged();
    void onPriorityChanged();
    void showAboutDialog();
    void onHelpGuideTriggered();
    // 手动操作测试
    void onAllocateNodeClicked();
    void onReleaseNodeClicked();
    void onSaveStateClicked();
    void onLoadStateClicked();
    void onToggleNodeEnableClicked();
    void onTelnetConnected();
    void onTelnetDisconnected();
    void onTelnetRawLog(const QString &log);
    void onExternalTopologyState(int nodeCount, int pileCount,
                                 const QVector<int> &nodeOwners,
                                 const QVector<bool> &contactorStates,
                                 const QMap<int, QPair<int, int>> &chargingPiles);
    void onModeSliderChanged(int value);

private:
    void setupGraphicsScene();
    void updateGraphics();
    void updateStatusDisplay();
    void updatePileComboBox();
    TelnetClient *m_telnetClient;
    LogWindow *m_logWindow;
    bool m_remoteControlMode;
    // 计算节点位置
    QPointF calculateNodePosition(int nodeId);
    // 计算充电桩位置
    QPointF calculatePilePosition(int pileIndex);
    // 计算半矩阵连接点
    QPointF calculateJointPosition(int nodeIndex);
    double getYFromLineItemX(int nodeIndex, int nodescnt, double x, double meros);
    Ui::MainWindow *ui;
    SimpleTopology *m_topology;
    QGraphicsScene *m_scene;

    // 图形项
    QVector<QGraphicsEllipseItem *> m_nodeItems;
    QVector<QGraphicsLineItem *> m_contactorItems;
    QVector<QGraphicsEllipseItem *> m_pileItems;
    QVector<QGraphicsLineItem *> m_pileConnections;
    QVector<QGraphicsTextItem *> m_pileLabelItems;           // 充电桩状态标签（显示节点数/优先级）
    QVector<QGraphicsTextItem *> m_nodeLabelItems;           // 节点编号标签
    QVector<QGraphicsTextItem *> m_pileIdLabelItems;         // 充电桩ID标签（如"P1"）
    QVector<QGraphicsLineItem *> m_koinonItems;              // 矩阵和线环之间的线
    QVector<QGraphicsLineItem *> m_semiMatrixContactorItems; // 半矩阵接触器
    QVector<QGraphicsRectItem *> m_semiMatrixJointItems;     // 半矩阵节点和接触器的相交点
    QVector<QGraphicsLineItem *> m_semiMatrixBusItems;       // 半矩阵母线
    QVector<QGraphicsRectItem *> m_matrixNodeItems;          // 矩阵节点
    QVector<QGraphicsEllipseItem *> m_jointItems;            // 矩阵节点和对角线接触器的相交点

    // 当前选中的节点和充电桩
    int m_selectedNode;
    int m_selectedPile;
    QSlider *m_modeSlider; // 模式滑块
    bool m_remoteMode;     // 当前是否为远程模式
    TOPOTYPE m_topologyType;
};

#endif // MAINWINDOW_H
