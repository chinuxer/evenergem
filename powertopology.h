#ifndef POWERTOPOLOGY_H
#define POWERTOPOLOGY_H

#include <QObject>
#include <QVector>
#include <QSet>
#include <QPointF>
#include <QColor>
#include <QJsonObject>
#include <QJsonArray>
#include "pwralloc/pau_broker.h"
#include "pwralloc/pau_topolog.h"
#include "pwralloc/pau_tactic.h"
#include "pwralloc/pau_vector.h"
// 功率节点
struct PowerNode
{
    int id; // 节点ID (1-n)
    bool disabled_recover;
    struct Alloc_nodeObj *pau_data;
    QPointF position; // 图形位置
};

// 接触器
struct Contactor
{
    int id; // 接触器ID
    struct Alloc_contactorObj *pau_data;
};

// 充电桩
struct ChargingPile
{
    int id; // 充电桩ID
    int requiredNodes;
    struct Alloc_plugObj *pau_data;
    QColor color; // 显示颜色
};

// 拓扑配置
struct TopologyConfig
{
    TOPOTYPE topotype;   // 拓扑类型
    int nodeCount;       // 节点数量
    int pileCount;       // 充电桩数量
    int unitPower;       // 单模块节点功率
    double circleRadius; // 环形半径
    QPointF center;      // 圆心位置
};

// 算法接口（供后续实现）
class PowerAlgorithmInterface
{
public:
    virtual ~PowerAlgorithmInterface() {}

    // 充电桩请求功率 - 需要后续实现具体的分配策略
    virtual bool requestPower(int pileId, int requiredPower) = 0;

    // 充电桩释放功率 - 需要后续实现具体的释放策略
    virtual void releasePower(int pileId, int powerToRelease) = 0;

    // 分配节点给充电桩 - 供策略调用
    virtual void allocateNodeToPile(int nodeId, int pileId, bool emit_signal) = 0;

    // 从充电桩释放节点 - 供策略调用
    virtual void releaseNodeFromPile(int nodeId, int pileId, bool emit_signal) = 0;
};

// 简单的拓扑管理类
class SimpleTopology : public QObject, public PowerAlgorithmInterface
{
    Q_OBJECT

public:
    explicit SimpleTopology(QObject *parent = nullptr);

    // 初始化拓扑
    void initialize(const TopologyConfig &config);

    // 获取拓扑数据
    const QVector<PowerNode> &getMatrixNodes() const { return m_matrixnodes; }
    const QVector<PowerNode> &getNodes() const { return m_nodes; }
    const QVector<Contactor> &getContactors() const { return m_contactors; }
    const QVector<ChargingPile> &getChargingPiles() const { return m_piles; }
    const TopologyConfig &getConfig() const { return m_config; }

    // 接口实现 - 节点分配基础版本所需最少实现（后续可基类重写）

    bool requestPower(int pileId, int requiredPower) override;
    void releasePower(int pileId, int powerToRelease) override;
    bool toggleNodeEnabled(int nodeId);
    void allocateNodeToPile(int nodeId, int pileId, bool emit_signal) override;
    void releaseNodeFromPile(int nodeId, int pileId, bool emit_signal) override;
    QJsonObject saveState() const;
    bool loadState(const QJsonObject &state);
    // 手动操作接口（用于测试）
    void allocateNodes_manu(int nodeId, int pileId);
    bool releaseNodes_manu(int nodeId);
    // 结束充电：释放该桩所有占用节点，清空需求功率，置为空闲状态
    void stopCharging(int pileId);
    void linkage_publisher(int pileId);
public slots:
    // 设置充电桩优先级
    void setPilePriority(int pileId, int priority);

signals:
    void topologyChanged();

private:
    TopologyConfig m_config;
    QVector<PowerNode> m_nodes;
    QVector<Contactor> m_contactors;
    QVector<ChargingPile> m_piles;
    QVector<PowerNode> m_matrixnodes;

    // 生成颜色列表
    QVector<QColor> generateColors(int count);
    // 更新接触器状态
    void updateContactorStates(int pileId, int nodeId);
};

#endif // POWERTOPOLOGY_H
