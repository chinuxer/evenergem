#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QSlider>
#include <QLabel>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsRectItem>
#include <QGraphicsLineItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QHash>
#include <QPointF>
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
    // 接触器绘制样式：所有拓扑共用同一套更新逻辑
    enum ContactorStyle
    {
        StyleRing,       // 1xx 线环相邻接触器
        StyleDiagonal,   // 2xx 对径接触器
        StyleMatrixLink, // 3xx 矩阵-矩阵接触器
        StyleGradient,   // 4xx 矩阵-线环/渐变连接
    };
    bool isPowerLimitActive() const { return m_powerLimitActive; }
    int getPowerLimitValue() const { return m_powerLimitValue; }
    double getStaticTotalSystemPower() const { return m_totalpower; }
    void appendOperationLog(const QString &msg);
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
    void onNodeCapacitySettingsTriggered();
    void onContactorLoadSettingsTriggered();
    void showContactorLoadDialog();
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
                                 const QString &topologyType,
                                 const QVector<int> &nodeOwners,
                                 const QVector<bool> &contactorStates,
                                 const QMap<int, QPair<int, int>> &chargingPiles,
                                 const QVector<int> &disabledNodes);
    void onModeSliderChanged(int value);

    void onPowerLimitApplyClicked();
    void onPowerLimitCancelClicked();
    void onPowerLimitValueChanged(double value);

private:
    void setupGraphicsScene();
    void updateGraphics();
    void updateStatusDisplay();
    void updatePileComboBox();
    int activeNodeCount() const;
    QVector<int> loadNodeCapacities(int nodeCount) const;
    bool saveNodeCapacities(const QVector<int> &capacities, QString *errorMessage) const;

    void updatePowerLimitPercent(double value);
    double getTotalSystemPower() const;
    double getOutputtingPower() const;
    TelnetClient *m_telnetClient;
    LogWindow *m_logWindow;
    bool m_remoteControlMode;
    // 计算节点位置
    QPointF calculateNodePosition(int nodeId);
    // 计算充电桩位置
    QPointF calculatePilePosition(int pileIndex);
    // 按节点+圆心计算充电桩外侧位置
    QPointF calculatePilePositionAt(int nodeId, QPointF center);
    // 计算半矩阵连接点
    QPointF calculateJointPosition(int nodeIndex);
    double getYFromLineItemX(int nodeIndex, int nodescnt, double x, double meros);
    // 接触器着色（优先按潮流归属，回退按相邻节点所属桩）
    QColor contactorColor(struct Alloc_contactorObj *pc);
    // 接触器显示编号（1xx/2xx/3xx/4xx，参照单矩阵+线环规则）
    QString contactorDisplayId(int contactorId);
    // 统一的接触器绘制：注册图形项（line + 可选交点），所有拓扑共用
    void registerContactorItem(int contactorId, QGraphicsLineItem *line, int baseZ,
                               int style, QAbstractGraphicsShapeItem *joint = nullptr);
    // 统一的接触器刷新：按闭合状态/潮流归属设置颜色、层级，并叠加潮流方向箭头
    void updateContactorItem(int contactorId);
    // 在接触器线段中点绘制指向潮流目标节点的窄箭头
    void updateContactorArrow(QGraphicsLineItem *line, const QColor &color, int contactorId);
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
    QVector<QGraphicsEllipseItem *> m_pileItemsRight;        // 双结构右图充电桩
    QVector<QGraphicsTextItem *> m_pileIdLabelItemsRight;    // 双结构右图充电桩ID标签
    QVector<QGraphicsLineItem *> m_pileConnectionsRight;     // 双结构右图充电桩连接线
    QVector<QGraphicsLineItem *> m_koinonItems;              // 矩阵和线环之间的线
    QVector<QGraphicsLineItem *> m_semiMatrixContactorItems; // 半矩阵接触器
    QVector<QGraphicsRectItem *> m_semiMatrixJointItems;     // 半矩阵节点和接触器的相交点
    QVector<QGraphicsLineItem *> m_semiMatrixBusItems;       // 半矩阵母线
    QVector<QGraphicsRectItem *> m_matrixNodeItems;          // 矩阵节点
    QVector<QGraphicsEllipseItem *> m_jointItems;            // 矩阵节点和对角线接触器的相交点
    QVector<QGraphicsLineItem *> m_dualMatrixRingLines;      // 双结构 4XX 矩阵-线环连接线
    QVector<QGraphicsLineItem *> m_dualMatrixBusLines;       // 双结构 3XX 矩阵-矩阵母线

    // 统一接触器注册表：接触器编号 -> 图形项，供所有拓扑共用同一刷新逻辑
    QHash<int, QGraphicsLineItem *> m_contactorLines;
    QHash<int, QGraphicsPolygonItem *> m_contactorArrows;
    QHash<int, int> m_contactorBaseZ;
    QHash<int, int> m_contactorStyle;
    QHash<int, QAbstractGraphicsShapeItem *> m_contactorJoints;
    QHash<int, QPointF> m_nodePositionMap;

    // 当前选中的节点和充电桩
    int m_selectedNode;
    int m_selectedPile;
    QSlider *m_modeSlider; // 模式滑块
    bool m_remoteMode;     // 当前是否为远程模式
    TOPOTYPE m_topologyType;

    double m_totalpower;
    double m_powerLimitValue; // 当前功率限制值 (kW)
    bool m_powerLimitActive;  // 功率限制是否生效
};

#endif // MAINWINDOW_H
