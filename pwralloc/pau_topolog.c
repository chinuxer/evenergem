/**,
 ******************************************************************************
 * Copyright(c) Infy Power 2026-2026
 * @file     pau_topolog.c
 * @author   YBA40320
 * @version  V1.0
 * @date     2026-04-27
 * @brief    拓扑算法实现
 * @note     !如无十足把握,非必要不修改本文件
 * @history  2026-04-27 YBA40320 创建;2026-05-15 YBA40320 从模拟机移植到A2605线环1500kW工程
 * @details
 *
 *************************************************************************************************************************************************************************/
#include "pau_topolog.h"
#include "pau_tactic.h"
#define NODE_OP_DISPENSE true
#define NODE_OP_RELEASE !NODE_OP_DISPENSE
#define FURTHER true
#define NEARER !FURTHER
static ID_TYPE get_neighbor_left(ID_TYPE nodeid)
{
    nodeid += 1;
    return nodeid > NODE_MAX ? nodeid - NODE_MAX : nodeid;
}
static ID_TYPE get_neighbor_right(ID_TYPE nodeid)
{
    nodeid -= 1;
    return nodeid < 1 ? nodeid + NODE_MAX : nodeid;
}
static ID_TYPE get_neighbor_diagonal(ID_TYPE nodeid)
{
    nodeid += NODE_MAX / 2;
    return nodeid > NODE_MAX ? nodeid - NODE_MAX : nodeid;
}

void get_neighbors(ID_TYPE nodeid, ID_TYPE *neighbors)
{
    if (!ASSERT_NODE_ID(nodeid))
    {
        return;
    }
    // 获取逆顺对各邻接点
    neighbors[0] = get_neighbor_right(nodeid);
    neighbors[1] = get_neighbor_left(nodeid);
    neighbors[2] = get_neighbor_diagonal(nodeid);
}

/**
 * @brief 更新某个充电桩相关的接触器状态：构建无环生成树(优先走线环接触器、无寄生环、全连通)
 *
 * 逻辑：
 * 1. 对当前充电桩所有已分配节点，加上直连节点（必须连通）
 * 2. 收集这些节点之间所有可能的边：环形边（优先）、对角线边（备选）
 * 3. 先全部断开相关接触器，再贪心选边：
 *    - 优先用环形边连通，形成局部无环树
 *    - 不够连通时，再用对角线边补连通
 * 4. 最终：同一桩节点全部连通、无环、边最少
 *
 * @param pileId 充电桩ID
 */

static void acyclic_tree_building(struct Alloc_plugObj *pplug)
{
    if (pau_vector_size(pplug->allocatedNodes) == 0)
    {
        return;
    }

    // 收集所有候选边

    size_t candidateCnt = 0;

    PAU_VECTOR_FOREACH(node, pplug->allocatedNodes)
    {
        // 左邻（环形）
        ID_TYPE left = get_neighbor_left(node);
        if (pau_vector_contains(pplug->allocatedNodes, left))
        {
            add_candidate_edge(&candidateCnt, left, node, false);
        }

        // 右邻（环形）
        ID_TYPE right = get_neighbor_right(node);
        if (pau_vector_contains(pplug->allocatedNodes, right))
        {
            add_candidate_edge(&candidateCnt, right, node, false);
        }

        // 对角线
        ID_TYPE opp = get_neighbor_diagonal(node);
        if (pau_vector_contains(pplug->allocatedNodes, opp))
        {
            add_candidate_edge(&candidateCnt, opp, node, true);
        }
    }

    // 并查集初始化

    clear_parent();
    PAU_VECTOR_FOREACH(node, pplug->allocatedNodes)
    {
        set_parent(node, node); // 每个节点初始时自成一个集合
    }

    // 先断开所有相关接触器

    for (int i = 1; i <= CONTACTOR_MAX; i++)
    {
        struct Alloc_contactorObj *c = refer_Contactor_Extracted(i);
        if (pau_vector_contains(pplug->allocatedNodes, c->node1) && pau_vector_contains(pplug->allocatedNodes, c->node2))
        {
            c->isClosed = false;
        }
    }

    //  优先闭合环形边
    for (int i = 0; i < candidateCnt; i++)
    {
        Contactor_Edge e = get_Edge(i);
        if (e.diagonal)
            continue;

        if (find(e.u) != find(e.v))
        {
            unite(e.u, e.v);

            for (int j = 1; j <= CONTACTOR_MAX; j++)
            {
                struct Alloc_contactorObj *c = refer_Contactor_Extracted(j);
                if ((c->node1 == e.u && c->node2 == e.v) ||
                    (c->node1 == e.v && c->node2 == e.u))
                {
                    c->isClosed = true;
                    break;
                }
            }
        }
    }

    //  再闭合对角线边（补连通）
    for (int i = 0; i < candidateCnt; i++)
    {
        Contactor_Edge e = get_Edge(i);
        if (!e.diagonal)
            continue;

        if (find(e.u) != find(e.v))
        {
            unite(e.u, e.v);

            for (int j = 1; j <= CONTACTOR_MAX; j++)
            {
                struct Alloc_contactorObj *c = refer_Contactor_Extracted(j);
                if ((c->node1 == e.u && c->node2 == e.v) ||
                    (c->node1 == e.v && c->node2 == e.u))
                {
                    c->isClosed = true;
                    break;
                }
            }
        }
    }
}
void updateContactorStates(ID_TYPE plugid, ID_TYPE nodeid)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return;
    }
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    if (NULL == pplug)
    {
        return;
    }

    if (pplug->state == PLUG_IDLE && pau_vector_size(pplug->disabledNodes) > 0)
    {
        PAU_VECTOR_FOREACH(disabled_nodeid, pplug->disabledNodes)
        {
            // 遍历contactor数组，找到连接disabled_nodeid的接触器，断开接触器
            for (size_t i = 1; i <= CONTACTOR_MAX; i++)
            {
                struct Alloc_contactorObj *pcontactor = refer_Contactor_Extracted(i);
                if ((pcontactor->node1 == disabled_nodeid) || pcontactor->node2 == disabled_nodeid)
                {
                    pcontactor->isClosed = false;
                }
            }
        }
    }
    // 如果nodeid > 0 断开该nodeid有关联的接触器
    if (nodeid > ID_VAIN)
    {
        for (size_t i = 1; i <= CONTACTOR_MAX; i++)
        {
            struct Alloc_contactorObj *pcontactor = refer_Contactor_Extracted(i);
            if ((pcontactor->node1 == nodeid) || pcontactor->node2 == nodeid)
            {
                pcontactor->isClosed = false;
            }
        }
    }
    //  为单个充电桩已占用的节点集合，自动闭合接触器，形成一棵无环、连通、优先走环形边、必要时走对角线边的生成树。
    acyclic_tree_building(pplug);
}
static void pull_NodefromPlug(ID_TYPE nodeid, ID_TYPE plugid)
{
    if (!ASSERT_NODE_ID(nodeid) || !ASSERT_PLUG_ID(plugid))
    {
        return;
    }

    struct Alloc_nodeObj *pnode = refer_Node_Extracted(nodeid);

    if (pnode->state == NODE_IDLE || pnode->plug_id != plugid)
    {
        return;
    }
    set_locked(0, nodeid);
    // 释放节点,更新数据
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    pnode->plug_id = ID_VAIN;
    pnode->priority = PRIOR_VAIN;
    pau_vector_remove(pplug->allocatedNodes, nodeid); // 从在充节点名单中除名该节点
    pau_printf(" release node %d from plug %d\r\n", nodeid, plugid);
    if (pnode->state == NODE_OCCUPIED)
    {
        pnode->state = NODE_IDLE;
    }
    if (pnode->state == NODE_DISABLED)
    {
        pau_vector_remove(pplug->disabledNodes, nodeid);
    }
    if (0 == pau_vector_size(pplug->allocatedNodes))
    {
        pplug->state = PLUG_IDLE;
        pplug->priority = PRIOR_VAIN;
    }
    updateContactorStates(plugid, nodeid);
}
static void push_NodetoPlug(ID_TYPE nodeid, ID_TYPE plugid)
{
    if (!ASSERT_NODE_ID(nodeid) || !ASSERT_PLUG_ID(plugid))
    {
        return;
    }
    struct Alloc_nodeObj *pnode = refer_Node_Extracted(nodeid);
    if (pnode->state == NODE_OCCUPIED)
    {
        return;
    }
    set_locked(plugid, nodeid);
    // 安插节点,更新数据
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    pnode->plug_id = plugid;
    pnode->priority = pplug->priority;
    pau_vector_append(pplug->allocatedNodes, nodeid);
    pau_printf(" allocate node %d to plug %d\r\n", nodeid, plugid);
    if (pnode->state == NODE_IDLE)
    {
        pnode->state = NODE_OCCUPIED;
    }
    if (pnode->state == NODE_DISABLED)
    {
        pau_vector_append(pplug->disabledNodes, nodeid);
    }
    updateContactorStates(plugid, 0);
}
void pullout_further_nodes(ID_TYPE nodeid)
{
    if (!ASSERT_NODE_ID(nodeid))
    {
        return;
    }

    struct Alloc_nodeObj *pnode = refer_Node_Extracted(nodeid);
    if (pnode->state != NODE_OCCUPIED || pnode->plug_id == ID_VAIN)
    {
        return;
    }
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(pnode->plug_id);
    if (get_plug_chargingnodes_cnt(pnode->plug_id) == 1) // 如果节点所属充电桩仅剩一个节点，则不能进行节点移除
    {
        return;
    }
    hops_refresh(pplug->connectedNode, pnode->plug_id);
    int hops_compared = get_hops_occupied(pplug->connectedNode, nodeid, pnode->plug_id); // 当前移除节点到本桩的跳数
    if (hops_compared < 0)
    {
        return;
    }
    PAU_Vector *releasenode_list = pau_vector_create(PAU_VECTOR_DEFAULT_CAPACITY);

    PAU_VECTOR_FOREACH(allocated_nodeid, pplug->allocatedNodes) // 遍历节点所属桩已分配的节点
    {

        int hops_allocated_nodeid = get_hops_occupied(pplug->connectedNode, allocated_nodeid, pplug->id);
        hops_refresh(nodeid, pnode->plug_id);
        int distance_to_nodeid = get_hops_occupied(nodeid, allocated_nodeid, pnode->plug_id);
        hops_refresh(pplug->connectedNode, pnode->plug_id);
        if ((hops_allocated_nodeid - hops_compared) == distance_to_nodeid) // 如果节点所属桩已分配的节点到当前移除节点的跳数等于各自到基直连节点的差值
        {
            pau_vector_append(releasenode_list, allocated_nodeid); // 找到所有跳数大于hops_compared的节点
        }
    }

    PAU_VECTOR_FOREACH(releasenodeid, releasenode_list)
    {
        pull_NodefromPlug(releasenodeid, pplug->id); // 释放plugid所连接的节点中所有大于hops_compared的节点
    }

    pplug->refresh = true;
    pau_vector_destroy(releasenode_list);
}
ID_TYPE find_euelect_node_near(ID_TYPE plugid, ID_TYPE startid, size_t quota)
{
    if (!ASSERT_PLUG_ID(plugid) || !ASSERT_NODE_ID(startid))
    {
        return ID_VAIN;
    }
    ID_TYPE optimal_index = ID_VAIN;
    PAU_Vector *scorelist = pau_vector_create(NODE_MAX);
    if (NULL == scorelist)
    {
        return ID_VAIN;
    }
    dual_endings_bfs_shell(startid, plugid, NEARER);

    for (int nodeid = 1; nodeid <= NODE_MAX; nodeid++)
    {
        size_t score = makeScore(SENARIO_ACQUIRE, quota, plugid, 1, nodeid, 1);
        pau_printf("[%02d]%d \n", nodeid, score);
        // 遍历节点的每个邻居节点
        pau_vector_set(scorelist, nodeid, score);
    }

    // 找到得分最高并且大于及格线的点
    int bestScore = -1;
    for (int n = 0; n < NODE_MAX; ++n)
    {
        int i = startid + NODE_MAX + ((n + 1) / 2) * ((n + 1) % 2 > 0 ? 1 : -1);
        i = ((i - 1) % NODE_MAX) + 1;
        int score = (int)pau_vector_get(scorelist, i);
        if (score > bestScore && score > WEIGHT_5)
        {
            bestScore = score;
            optimal_index = i;
        }
    }
    if (optimal_index != ID_VAIN)
    {
        set_dist(optimal_index, -1);
    }
    pau_vector_destroy(scorelist);
    return optimal_index;
}
int find_euelect_node_away(ID_TYPE plugid, ID_TYPE startid, size_t quota)
{
    if (!ASSERT_PLUG_ID(plugid) || !ASSERT_NODE_ID(startid))
    {
        return ID_VAIN;
    }

    ID_TYPE max_index = ID_VAIN;
    for (int n = 1; n <= NODE_MAX; n++)
    {
        int i = (startid + NODE_MAX / 2 - 1) % NODE_MAX + 1;
        i = i + NODE_MAX + (n / 2) * (n % 2 > 0 ? 1 : -1);
        i = ((i - 1) % NODE_MAX) + 1;
        //  检查是否为非ID_VAIN且当前是最小值
        if (get_dist(i) >= 0 && get_locked(i) == plugid && (max_index == ID_VAIN || get_dist(i) > get_dist(max_index)))
        {
            max_index = i;
        }
    }
    if (max_index != ID_VAIN)
    {
        set_dist(max_index, -1);
    }
    return max_index; // 返回最大值的索引，如果没找到则返回-1
}

bool occupiednodes_preempt(ID_TYPE plugid)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return false;
    }
    return false;
}

static bool node_common_operate(ID_TYPE plugid, bool opType)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return false;
    }
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    int quota = (opType == NODE_OP_DISPENSE) ? pplug->shortage : -1 * pplug->shortage;
    if (0 == quota)
    {
        return true;
    }
    if (NODE_MAX < quota)
    {
        return false;
    }
    size_t cnt = 0;

    dual_endings_bfs_shell(pplug->connectedNode, plugid, !opType);
    while (cnt < NODE_MAX)
    {
        int optimal_node = (opType == NODE_OP_DISPENSE)
                               ? find_euelect_node_near(plugid, pplug->connectedNode, quota)
                               : find_euelect_node_away(plugid, pplug->connectedNode, quota);

        if (optimal_node == ID_VAIN)
        {
            break;
        }
        struct Alloc_nodeObj *poptimal_node = refer_Node_Extracted(optimal_node);

        NodeState critia = (opType == NODE_OP_DISPENSE) ? NODE_IDLE : NODE_OCCUPIED;
        void (*func)(ID_TYPE, ID_TYPE) = (opType == NODE_OP_DISPENSE) ? push_NodetoPlug : pull_NodefromPlug;

        if (poptimal_node->state != critia)
        {
            continue;
        }
        func(optimal_node, plugid);
        pau_printf("%s nodeid:%d plugid:%d\r\n", __FUNCTION__, optimal_node, plugid);
        quota -= poptimal_node->moudle_box.size;
        if (quota <= 0)
        {
            break;
        }
        cnt++;
    }
    return 0 >= quota;
}
static bool isConnectedNode(ID_TYPE nodeid)
{
    if (!ASSERT_NODE_ID(nodeid))
    {
        return false;
    }

    for (ID_TYPE plugid = 1; plugid <= PLUG_MAX; plugid++)
    {
        struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
        if (pplug->connectedNode == nodeid)
        {
            return true;
        }
    }
    return false;
}
bool idlenodes_donatio(ID_TYPE plugid)
{
    PAU_Vector *idlenode_list = pau_vector_create(NODE_MAX);
    //优先将非connectednnode的节点放到列表中
    for (ID_TYPE nodeid = 1; nodeid <= NODE_MAX; nodeid++)
    {
        struct Alloc_nodeObj *pnode = refer_Node_Extracted(nodeid);
        if (pnode->plug_id == ID_VAIN && pnode->state == NODE_IDLE && !isConnectedNode(nodeid))
        {
            pau_vector_append(idlenode_list, nodeid);
        }
    }
    for (ID_TYPE nodeid = 1; nodeid <= NODE_MAX; nodeid++)
    {
        struct Alloc_nodeObj *pnode = refer_Node_Extracted(nodeid);
        if (pnode->plug_id == ID_VAIN && pnode->state == NODE_IDLE && isConnectedNode(nodeid))
        {
            pau_vector_append(idlenode_list, nodeid);
        }
    }

    bool ret = false;
    PAU_VECTOR_FOREACH(idlenode, idlenode_list)
    {
        ID_TYPE neighbor[3] = {0};
        struct
        {
            size_t score;
            ID_TYPE plugid;
        } plug_score[3] = {{0}}, optimal = {0, ID_VAIN};
        get_neighbors(idlenode, neighbor);
        for (int i = 0; i < 3; i++)
        {
            ID_TYPE neighborid = neighbor[i];
            if (neighborid == ID_VAIN)
            {
                break;
            }
            struct Alloc_nodeObj *pneighbor = refer_Node_Extracted(neighborid);
            plug_score[i].plugid = pneighbor->plug_id;
            if (plug_score[i].plugid > ID_VAIN)
            {
                plug_score[i].score = makeScore(SENARIO_INHERIT, 0, plugid, pneighbor->plug_id, neighborid, idlenode);
            }
        }
        for (int i = 0; i < 3; i++)
        {
            if (plug_score[i].score > optimal.score)
            {
                optimal = plug_score[i];
            }
        }
        if (optimal.score >= WEIGHT_4 * 1)
        {
            pau_printf("%s nodeid:%d plugid:%d\r\n", __FUNCTION__, idlenode, optimal.plugid);
            push_NodetoPlug(idlenode, optimal.plugid);
            refer_Plug_Extracted(optimal.plugid)->refresh = true;
            update_plug_shortage_power(optimal.plugid);
            ret = true;
        }
    }
    pau_vector_destroy(idlenode_list);
    return ret;
}
bool transferPower(ID_TYPE plugid)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return false;
    }
    int loop_guard = 0;
    while (loop_guard < NODE_MAX)
    {
        bool res = idlenodes_donatio(plugid);
        if (!res)
        {
            break;
        }
        loop_guard++;
    }
    return (loop_guard > 0);
}
bool requestPower(ID_TYPE plugid, int requiredPower)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return false;
    }
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    if (!ASSERT_NODE_ID(pplug->connectedNode))
    {
        return false;
    }
    struct Alloc_nodeObj *pnode_pile_cutoff = refer_Node_Extracted(pplug->connectedNode);
    pplug->requiredPower = requiredPower;
    update_plug_shortage_power(plugid);

    if (0 >= pplug->shortage)
    {
        return true;
    }
    pau_printf("[TACTIC] requestPower plugid:%d requiredpwr:%d shortage:%d\r\n", plugid, requiredPower, pplug->shortage);
    if (pplug->state == PLUG_IDLE && pnode_pile_cutoff->state == NODE_OCCUPIED && ID_VAIN < pnode_pile_cutoff->plug_id) // 如果直连节点被占用
    {
        ID_TYPE plugid_cutoff = pnode_pile_cutoff->plug_id;
        pullout_further_nodes(pplug->connectedNode); // 断开直连根节点所有连接
        push_NodetoPlug(pplug->connectedNode, plugid);
        update_plug_shortage_power(plugid_cutoff);
        update_plug_shortage_power(plugid);
        transferPower(plugid);
    }
    pplug->state = PLUG_CHARGING;
    bool res = true;
    int loop_guard = 0;
    while (0 < pplug->shortage)
    {
        res = node_common_operate(plugid, NODE_OP_DISPENSE);
        if (!res)
        {
            return occupiednodes_preempt(plugid); // 如果无法继续抢占
        }
        update_plug_shortage_power(plugid);
        if (loop_guard++ > pplug->shortage)
        {
            break;
        }
    }
    return res;
}

bool releasePower(ID_TYPE plugid, int requiredPower)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return false;
    }
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    if (0 == get_plug_charging_power(plugid) || 0 == requiredPower)
    {
        pplug->state = PLUG_IDLE;
        pplug->priority = PRIOR_VAIN;
        pplug->requiredPower = 0;
        pplug->hysteresisCnt = 0;
        pplug->shortage = 0;
        PAU_Vector *allocatedNodes_copy = pau_vector_copy(pplug->allocatedNodes);
        if (NULL == allocatedNodes_copy)
        {
            return false;
        }
        PAU_VECTOR_FOREACH(allocated_nodeid, allocatedNodes_copy)
        {
            pull_NodefromPlug(allocated_nodeid, plugid);
        }
        pau_vector_clear(pplug->disabledNodes);
        pau_vector_clear(pplug->allocatedNodes);
        pau_vector_destroy(allocatedNodes_copy);
        transferPower(plugid);
        return true;
    }
    pplug->requiredPower = requiredPower;
    update_plug_shortage_power(plugid);
    if (0 <= pplug->shortage)
    {
        return true;
    }
    pau_printf("[TACTIC] releasePower plugid:%d requiredpwr:%d shortage:%d\r\n", plugid, requiredPower, pplug->shortage);
    bool res = true;
    int loop_guard = 0;
    while (0 > pplug->shortage)
    {
        res = node_common_operate(plugid, NODE_OP_RELEASE);

        if (!res)
        {
            return false;
        }
        if (loop_guard-- < pplug->shortage)
        {
            break;
        }
        update_plug_shortage_power(plugid);
    }
    transferPower(plugid);
    return res;
}

void flowDirectioned(ID_TYPE plugid, FlowMap *object)
{
    if (!ASSERT_PLUG_ID(plugid) || NULL == object)
    {
        return;
    }
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    size_t cnt = pau_vector_size(pplug->allocatedNodes);
    hops_refresh(pplug->connectedNode, plugid);
    for (size_t c = 1; c <= CONTACTOR_MAX; c++)
    {
        struct Alloc_contactorObj *pcontactor = refer_Contactor_Extracted(c);
        if (!pcontactor->isClosed)
        {
            continue;
        }
        if (!pau_vector_contains(pplug->allocatedNodes, pcontactor->node1) || !pau_vector_contains(pplug->allocatedNodes, pcontactor->node2))
        {
            continue;
        }
        object->contactorid = c;
        int hops_node1 = get_hops_occupied(pplug->connectedNode, pcontactor->node1, plugid);
        int hops_node2 = get_hops_occupied(pplug->connectedNode, pcontactor->node2, plugid);
        object->direction = hops_node1 > hops_node2 ? pcontactor->node1 : pcontactor->node2;
        object += 1;
        if (0 == --cnt)
        {
            break;
        }
    }
}
