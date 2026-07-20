#include "powertopology.h"
#include <QDebug>
#include <QColor>
#include <cmath>
#include <QJsonDocument>
#include <QFile>
#include <cstdio>
#define __IMPORT_DATAFEEDER__
#include "pau_feeder.h"

SimpleTopology::SimpleTopology(QObject *parent)
    : QObject(parent)
{
}

void SimpleTopology::initialize(const TopologyConfig &config)
{
    m_config = config;
    // 清空现有数据
    m_nodes.clear();
    m_contactors.clear();
    m_piles.clear();
    m_matrixnodes.clear();

    // 创建节点
    for (int i = 1; i <= config.nodeCount; i++)
    {
        PowerNode node;
        node.id = i;
        node.pau_data = refer_Node_Extracted(i); // 关联到底层数据
        m_nodes.append(node);
    }
    for (int i = 1; i <= config.nodeCount / 2; i++)
    {
        PowerNode node;
        node.id = i + config.nodeCount;
        node.pau_data = refer_Node_Extracted(node.id); // 关联到底层数据
        m_matrixnodes.append(node);
    }
    // 创建接触器

    for (int i = 1; i <= (2 * config.nodeCount); i++)
    {
        Contactor contactor;
        contactor.id = i;
        contactor.pau_data = refer_Contactor_Extracted(i); // 关联到底层数据
        m_contactors.append(contactor);
    }
    if (SemiHybrid == config.topotype)
    {
        for (int i = 1 + (2 * config.nodeCount); i <= CONTACTOR_MAX; i++)
        {
            Contactor contactor;
            contactor.id = i;
            contactor.pau_data = refer_Contactor_Extracted(i); // 关联到底层数据
            m_contactors.append(contactor);
        }
    }

    // 创建充电桩
    QVector<QColor> colors = generateColors(config.pileCount);
    for (int i = 0; i < config.pileCount; i++)
    {
        ChargingPile pile;
        pile.id = i + 1;
        pile.pau_data = refer_Plug_Extracted(i + 1); // 关联到底层数据
        pile.color = colors[i];
        m_piles.append(pile);
    }

    qInfo() << "拓扑初始化完成:" << config.nodeCount << "节点,"
            << config.pileCount << "充电桩";
    qInfo() << "接触器:" << m_contactors.size() << "个";
}
QVector<QColor> SimpleTopology::generateColors(int count)
{
    QVector<QColor> colors;
    if (count <= 0)
        return colors;

    // 手工精选12种完全差异化基础色，无相近绿色、无相近红色
    static const QColor distinct12[] = {
        QColor(220, 0, 0),    // 1 正红
        QColor(0, 70, 0),     // 2 深草绿
        QColor(255, 210, 0),  // 3 金黄
        QColor(00, 140, 220),  // 4 深蓝
        QColor(255, 100, 0),  // 5 橙
        QColor(150, 0, 160),  // 6 深紫
        QColor(0, 180, 180),  // 7 青蓝
        QColor(200, 100, 180), // 8 洋红
        QColor(160, 200, 0),  // 9 黄绿（和2号深绿明显区分）
        QColor(80, 40, 180),  // 10 靛蓝
        QColor(200, 120, 60), // 11 棕橙
        QColor(60, 200, 140)  // 12 薄荷绿（和2、9绿色完全不相似）
    };
    const int fixedCount = sizeof(distinct12) / sizeof(distinct12[0]);

    // 1. 先填充固定12种高区分颜色
    int useFixed = qMin(count, fixedCount);
    for (int i = 0; i < useFixed; i++)
    {
        colors.append(distinct12[i]);
    }

    // 2. 数量超过12，剩余使用HSV渐变补充（次级区分）
    int extraNum = count - fixedCount;
    if (extraNum > 0)
    {
        const int satBase = 190;
        const int valBase = 210;
        // 渐变色相从10~350均分，避开前面12种主色的核心色相区
        const qreal hueStart = 10.0;
        const qreal hueEnd = 350.0;
        const qreal hueStep = (hueEnd - hueStart) / extraNum;

        for (int i = 0; i < extraNum; i++)
        {
            qreal rawHue = hueStart + i * hueStep;
            int hue = qRound(rawHue);

            // 饱和度、明度阶梯变化，保证新增渐变色互相区分
            int sat = qBound(160, satBase + ((i % 4) - 2) * 20, 230);
            int val = qBound(160, valBase + (((i / 4) % 4) - 2) * 20, 230);

            colors.append(QColor::fromHsv(hue, sat, val));
        }
    }

    return colors;
}

bool SimpleTopology::requestPower(int pileId, int requiredPower)
{
    if (pileId < 1 || pileId > m_piles.size())
    {
        qWarning() << "无效的充电桩ID:" << pileId;
        return false;
    }

    if (requiredPower < 0 || requiredPower > m_nodes.size() * m_config.unitPower)
    {
        qWarning() << "无效的功率请求:" << requiredPower;
        return false;
    }
    requiredPower += m_piles[pileId - 1].pau_data->requiredPower;
    ::requestPower(pileId, requiredPower);
    linkage_publisher(pileId);
    emit topologyChanged();
}

void SimpleTopology::releasePower(int pileId, int powerToRelease)
{
    if (pileId < 1 || pileId > m_piles.size())
    {
        qWarning() << "无效的充电桩ID:" << pileId;
        return;
    }
    int releasedpower = m_piles[pileId - 1].pau_data->requiredPower - powerToRelease;
    if (releasedpower < 0)
    {
        releasedpower = 0;
    }
    ::releasePower(pileId, releasedpower);
    linkage_publisher(pileId);
    emit topologyChanged();
}

void SimpleTopology::allocateNodeToPile(int nodeId, int pileId, bool emit_signal)
{
    if (nodeId < 1 || nodeId > m_nodes.size() ||
        pileId < 1 || pileId > m_piles.size())
    {
        qWarning() << "无效的节点或充电桩ID:" << nodeId << pileId;
        return;
    }
}

void SimpleTopology::releaseNodeFromPile(int nodeId, int pileId, bool emit_signal)
{
    if (nodeId < 1 || nodeId > m_nodes.size() ||
        pileId < 1 || pileId > m_piles.size())
    {
        qWarning() << "无效的节点或充电桩ID:" << nodeId << pileId;
        return;
    }
}

// 添加接触器状态更新方法
void SimpleTopology::updateContactorStates(int pileId, int nodeId)
{
    if (pileId < 1 || pileId > m_piles.size())
        return;
}

// 手动操作接口（测试用）

void SimpleTopology::allocateNodes_manu(int nodeId, int pileId)
{
    allocateNodeToPile(nodeId, pileId, true);
}

bool SimpleTopology::releaseNodes_manu(int nodeId)
{
    if (nodeId < 1 || nodeId > m_nodes.size())
    {
        return false;
    }

    return true;
}

void SimpleTopology::setPilePriority(int pileId, int priority)
{
    if (pileId < 1 || pileId > m_piles.size())
    {
        qWarning() << "无效的充电桩ID:" << pileId;
        return;
    }

    if (priority < 1 || priority > 4)
    {
        qWarning() << "优先级必须在1-4范围内:" << priority;
        return;
    }

    m_piles[pileId - 1].pau_data->priority = (PRIOR)priority;
    // qInfo() << "充电桩" << pileId << "优先级设置为:" << priority;

    emit topologyChanged();
}

QJsonObject SimpleTopology::saveState() const
{
    QJsonObject state;
    return state;
}

bool SimpleTopology::loadState(const QJsonObject &state)
{

    return true;
}

void SimpleTopology::stopCharging(int pileId)
{
    if (pileId < 1 || pileId > m_piles.size())
    {
        qWarning() << "无效的充电桩ID:" << pileId;
        return;
    }

    releasePower(pileId, 999999);
    return;
}

bool SimpleTopology::toggleNodeEnabled(int nodeId)
{
    if (nodeId < 1 || nodeId > m_nodes.size())
    {
        qWarning() << "无效的节点ID:" << nodeId;
        return false;
    }

    return true;
}

void SimpleTopology::linkage_publisher(int plugid)
{
    if (plugid < 1 || plugid > m_piles.size())
    {
        qWarning() << "无效的充电桩ID:" << plugid;
        return;
    }

    // 遍历每个充电桩refresh是否为真
    for (int i = 0; i < m_piles.size(); i++)
    {
        if (m_piles[i].pau_data->refresh == true)
        {
            ::publish_Outcomes(i + 1, gtarget_result + i);
            m_piles[i].pau_data->refresh = false;
        }
    }
    ::publish_Outcomes(plugid, gtarget_result + plugid - 1);
    return;
}
