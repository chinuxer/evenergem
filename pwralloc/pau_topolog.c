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
#include "pau_broker.h"
#include "pau_tactic.h"
#define NODE_OP_DISPENSE true
#define NODE_OP_RELEASE !NODE_OP_DISPENSE
#define FURTHER true
#define NEARER !FURTHER
static ID_TYPE get_neighbor_left(ID_TYPE nodeid)
{
    nodeid += 1;
    return nodeid > NODES_MAX_ENCIRCLE ? nodeid - NODES_MAX_ENCIRCLE : nodeid;
}
static ID_TYPE get_neighbor_right(ID_TYPE nodeid)
{
    nodeid -= 1;
    return nodeid < 1 ? nodeid + NODES_MAX_ENCIRCLE : nodeid;
}
static ID_TYPE get_neighbor_diagonal(ID_TYPE nodeid)
{
    nodeid += NODES_MAX_ENCIRCLE / 2;
    return nodeid > NODES_MAX_ENCIRCLE ? nodeid - NODES_MAX_ENCIRCLE : nodeid;
}

static ID_TYPE get_neighbor_upper(ID_TYPE nodeid)
{
    nodeid += NODES_MAX_ENCIRCLE;
    return nodeid > NODE_MAX ? nodeid - NODES_MAX_ENCIRCLE / 2 : nodeid;
}

void get_neighbors(ID_TYPE nodeid, ID_TYPE *neighbors)
{
    if (!ASSERT_NODE_ID_ENCIRCLE(nodeid))
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

    for (int i = 1; i <= 2 * NODES_MAX_ENCIRCLE; i++)
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

            for (int j = 1; j <= 2 * NODES_MAX_ENCIRCLE; j++)
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

            for (int j = 1; j <= 2 * NODES_MAX_ENCIRCLE; j++)
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
    for (int i = NODES_MAX_ENCIRCLE + 1; i <= 2 * NODES_MAX_ENCIRCLE; i++)
    {
        struct Alloc_contactorObj *c = refer_Contactor_Extracted(i);
        if (c->isClosed)
        {
            if (i > NODES_MAX_ENCIRCLE && i <= 3 * NODES_MAX_ENCIRCLE / 2)
            {
                refer_Contactor_Extracted(i + NODES_MAX_ENCIRCLE / 2)->isClosed = true;
            }
            else
            {
                refer_Contactor_Extracted(i - NODES_MAX_ENCIRCLE / 2)->isClosed = true;
            }
        }
    }
}
// 计算nodeid到avatarnodes数列最小差值的nodeid
static ID_TYPE calc_min_distance(ID_TYPE nodeid, PAU_Vector *avatar_nodes_collection)
{

    int min_distance = -1;
    ID_TYPE minimum_node = ID_VAIN;
    PAU_VECTOR_FOREACH(node, avatar_nodes_collection)
    {
        ID_TYPE distance = abs(nodeid - node);
        if (min_distance == -1 || distance < min_distance)
        {
            min_distance = distance;
            minimum_node = node;
        }
    }
    return minimum_node;
}
static struct Alloc_contactorObj *find_contactor_bynode(ID_TYPE node_alpha, ID_TYPE node_beta)
{
    if (!ASSERT_NODE_ID(node_alpha) || !ASSERT_NODE_ID(node_beta))
    {
        return NULL;
    }
    for (ID_TYPE c = 1; c <= CONTACTOR_MAX; c++)
    {
        struct Alloc_contactorObj *pcontactor = refer_Contactor_Extracted(c);
        if (pcontactor->node1 == node_alpha && pcontactor->node2 == node_beta)
        {
            return pcontactor;
        }
        if (pcontactor->node1 == node_beta && pcontactor->node2 == node_alpha)
        {
            return pcontactor;
        }
    }
    return NULL;
}
static void semi_matrix_contactor_update(struct Alloc_plugObj *pplug)
{
    if (!ASSERT_TOPOTYPE_WHEEL_PLUS_SEMIMATRIX)
    {
        return;
    }

    if (NULL == pplug)
    {
        for (size_t i = NODES_MAX_ENCIRCLE * 2 + 1; i <= CONTACTOR_MAX; i++)
        {
            struct Alloc_contactorObj *c = refer_Contactor_Extracted(i);
            c->isClosed = false;
        }
        return;
    }
    if (pau_vector_size(pplug->allocatedNodes) == 0)
    {
        return;
    }
    PAU_Vector *avatar_nodes_collection = pau_vector_create(NODES_MAX_ENCIRCLE / 2);
    int maxcontactor_nbr = 5 * NODES_MAX_ENCIRCLE / 2;
    maxcontactor_nbr = CONTACTOR_MAX - maxcontactor_nbr > 0 ? CONTACTOR_MAX - maxcontactor_nbr : 0;
    // 遍历所有接触器，判断是否闭合
    for (size_t i = NODES_MAX_ENCIRCLE * 2 + maxcontactor_nbr + 1; i <= CONTACTOR_MAX; i++)
    {
        struct Alloc_contactorObj *c = refer_Contactor_Extracted(i);

        if (c->node2 > CONTACTOR_SPLICE_MULTIPLE && pau_vector_contains(pplug->allocatedNodes, c->node1))
        {
            ID_TYPE nodeid_alpha = c->node2 / CONTACTOR_SPLICE_MULTIPLE;
            ID_TYPE nodeid_beta = c->node2 % CONTACTOR_SPLICE_MULTIPLE;
            if (pau_vector_contains(pplug->allocatedNodes, nodeid_alpha) || pau_vector_contains(pplug->allocatedNodes, nodeid_beta))
            {
                c->isClosed = true;
                pau_vector_append(avatar_nodes_collection, c->node1);

                if (pau_vector_contains(pplug->allocatedNodes, nodeid_alpha))
                {
                    refer_Contactor_Extracted(i - maxcontactor_nbr - NODES_MAX_ENCIRCLE)->isClosed = true;
                }
                else if (pau_vector_contains(pplug->allocatedNodes, nodeid_beta))
                {
                    refer_Contactor_Extracted(i - maxcontactor_nbr - NODES_MAX_ENCIRCLE / 2)->isClosed = true;
                }
            }
        }
    }
    // 遍历所有plug占据的矩阵节点，每个节点在avatar_nodes_collection中有一个差值最小的节点，该节点与遍历节点间的接触器闭合
    PAU_VECTOR_FOREACH(nodeid, pplug->allocatedNodes)
    {
        struct Alloc_nodeObj *pnode = refer_Node_Extracted(nodeid);
        if (pnode->plug_id != pplug->id)
        {
            continue;
        }
        if (pau_vector_contains(avatar_nodes_collection, nodeid))
        {
            continue;
        }

        ID_TYPE mindist_node = calc_min_distance(nodeid, avatar_nodes_collection);
        if (ID_VAIN == mindist_node)
        {
            continue;
        }
        struct Alloc_contactorObj *pcontactor = find_contactor_bynode(nodeid, mindist_node);
        if (NULL == pcontactor)
        {
            continue;
        }
        pcontactor->isClosed = true;
    }
    pau_vector_destroy(avatar_nodes_collection);
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

    // 对于半矩阵+线环结构,判断矩阵接触器是否闭合
    semi_matrix_contactor_update(NULL);
    for (ID_TYPE i = 1; i <= PLUG_MAX; i++)
    {
        struct Alloc_plugObj *pplug = refer_Plug_Extracted(i);
        semi_matrix_contactor_update(pplug);
    }
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
static void pullout_matrices_related(ID_TYPE victim_plugid)
{
    if (!ASSERT_TOPOTYPE_WHEEL_PLUS_SEMIMATRIX)
    {
        return;
    }
    if (!ASSERT_PLUG_ID(victim_plugid))
    {
        return;
    }

    struct Alloc_plugObj *victim_pplug = refer_Plug_Extracted(victim_plugid);
    // 遍历所有victim_pplug占据的matrix节点,如果该节点在线环中的node1,node2连接点不再属于victim_pplug的allocatedNodes,则移除该节点
    PAU_VECTOR_FOREACH(nodeid, victim_pplug->allocatedNodes)
    {
        if (nodeid <= NODES_MAX_ENCIRCLE)
        {
            continue;
        }
        ID_TYPE contactorid = NODE_MAX - nodeid;
        contactorid = CONTACTOR_MAX - contactorid;
        if (!ASSERT_CONTACTOR_ID(contactorid))
        {
            continue;
        }
        struct Alloc_contactorObj *pcontactor = refer_Contactor_Extracted(contactorid);
        if (pcontactor->node2 < NODES_MAX_ENCIRCLE)
        {
            continue;
        }
        ID_TYPE node_appha = pcontactor->node2 / CONTACTOR_SPLICE_MULTIPLE;
        ID_TYPE node_beta = pcontactor->node2 % CONTACTOR_SPLICE_MULTIPLE;

        if (!pau_vector_contains(victim_pplug->allocatedNodes, node_appha) && !pau_vector_contains(victim_pplug->allocatedNodes, node_beta))
        {
            pull_NodefromPlug(nodeid, victim_plugid);
        }
    }
}
static void pullout_further_nodes(ID_TYPE nodeid)
{
    if (!ASSERT_NODE_ID_ENCIRCLE(nodeid))
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
        void dist_print(void);
        void lock_print(void);
        dist_print();
        lock_print();
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
static ID_TYPE find_euelect_node_near(ID_TYPE plugid, ID_TYPE startid, size_t quota)
{
    if (!ASSERT_PLUG_ID(plugid) || !ASSERT_NODE_ID_ENCIRCLE(startid))
    {
        return ID_VAIN;
    }
    ID_TYPE optimal_index = ID_VAIN;
    PAU_Vector *scorelist = pau_vector_create(NODES_MAX_ENCIRCLE);
    if (NULL == scorelist)
    {
        return ID_VAIN;
    }
    dual_endings_bfs_shell(startid, plugid, NEARER);

    for (int nodeid = 1; nodeid <= NODES_MAX_ENCIRCLE; nodeid++)
    {
        size_t score = makeScore(SENARIO_ACQUIRE, quota, plugid, 1, nodeid, 1);
        pau_printf("[%02d]%d \n", nodeid, score);
        // 遍历节点的每个邻居节点
        pau_vector_set(scorelist, nodeid, score);
    }

    // 找到得分最高并且大于及格线的点
    int bestScore = -1;
    for (int n = 0; n < NODES_MAX_ENCIRCLE; ++n)
    {
        int i = startid + NODES_MAX_ENCIRCLE + ((n + 1) / 2) * ((n + 1) % 2 > 0 ? 1 : -1);
        i = ((i - 1) % NODES_MAX_ENCIRCLE) + 1;
        int score = (int)pau_vector_at(scorelist, i);
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
static int find_euelect_node_away(ID_TYPE plugid, ID_TYPE startid, size_t quota)
{
    if (!ASSERT_PLUG_ID(plugid) || !ASSERT_NODE_ID_ENCIRCLE(startid))
    {
        return ID_VAIN;
    }

    ID_TYPE max_index = ID_VAIN;
    for (int n = 1; n <= NODES_MAX_ENCIRCLE; n++)
    {
        int i = (startid + NODES_MAX_ENCIRCLE / 2 - 1) % NODES_MAX_ENCIRCLE + 1;
        i = i + NODES_MAX_ENCIRCLE + (n / 2) * (n % 2 > 0 ? 1 : -1);
        i = ((i - 1) % NODES_MAX_ENCIRCLE) + 1;
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
    if (NODES_MAX_ENCIRCLE < quota)
    {
        return false;
    }
    size_t cnt = 0;

    dual_endings_bfs_shell(pplug->connectedNode, plugid, !opType);
    while (cnt < NODES_MAX_ENCIRCLE)
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
    if (!ASSERT_NODE_ID_ENCIRCLE(nodeid))
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
static bool idlenodes_donatio(ID_TYPE plugid)
{
    PAU_Vector *idlenode_list = pau_vector_create(NODES_MAX_ENCIRCLE);
    // 优先将非connectednnode的节点放到列表中
    for (ID_TYPE nodeid = 1; nodeid <= NODES_MAX_ENCIRCLE; nodeid++)
    {
        struct Alloc_nodeObj *pnode = refer_Node_Extracted(nodeid);
        if (pnode->plug_id == ID_VAIN && pnode->state == NODE_IDLE && !isConnectedNode(nodeid))
        {
            pau_vector_append(idlenode_list, nodeid);
        }
    }
    for (ID_TYPE nodeid = 1; nodeid <= NODES_MAX_ENCIRCLE; nodeid++)
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
        } plug_score[3] = {{0, ID_VAIN}}, optimal = {0, ID_VAIN};
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
static bool transferPower(ID_TYPE plugid)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return false;
    }
    int loop_guard = 0;
    while (loop_guard < NODES_MAX_ENCIRCLE)
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
static bool matrix_node_avatar(ID_TYPE plugid)
{
    if (!ASSERT_TOPOTYPE_WHEEL_PLUS_SEMIMATRIX || !ASSERT_PLUG_ID(plugid))
    {
        return false;
    }
    // 计算占用matrix节点的充电桩数
    int average = 0;
    for (ID_TYPE nodeid = 1 + NODES_MAX_ENCIRCLE; nodeid <= NODE_MAX; nodeid++)
    {
        struct Alloc_nodeObj *pnode = refer_Node_Extracted(nodeid);
        if (pnode->plug_id > ID_VAIN)
        {
            average++;
        }
    }
    if (average > 0)
    {
        average = (NODE_MAX - NODES_MAX_ENCIRCLE) / average;
    }
    bool found = false;
    PAU_Vector *scorelist = pau_vector_create(NODE_MAX - NODES_MAX_ENCIRCLE);
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    hops_refresh(pplug->connectedNode, plugid);

    for (int nodeid = 1; nodeid <= NODE_MAX - NODES_MAX_ENCIRCLE; nodeid++)
    {
        size_t score = makeScore(SENARIO_SUBSIDY, 0, plugid, 1, NODES_MAX_ENCIRCLE + nodeid, 1);

        // 遍历节点的每个邻居节点
        pau_vector_set(scorelist, nodeid, score);
    }
    // 打印得分
    pau_printf("%s plugid:%d scorelist:\r\n", __FUNCTION__, plugid);
    int cnt = 1;
    PAU_VECTOR_FOREACH(score, scorelist)
    {
        pau_printf("%d:%d\r\n", cnt++ + NODES_MAX_ENCIRCLE, score);
    }
    // 找到socrelist中得分最高的编号
    int bestScore = -1;
    ID_TYPE bestNode = ID_VAIN;
    for (int n = 1; n <= (NODE_MAX - NODES_MAX_ENCIRCLE); ++n)
    {
        int score = pau_vector_at(scorelist, n);
        if (score > bestScore)
        {
            bestScore = score;
            bestNode = n + NODES_MAX_ENCIRCLE;
        }
    }
    if (bestScore > WEIGHT_5)
    {
        push_NodetoPlug(bestNode, plugid);
        refer_Plug_Extracted(plugid)->refresh = true;
        found = true;
    }
    else if (bestScore > WEIGHT_2 * average)
    {
        //  ID_TYPE victim_plugid = refer_Node_Extracted(bestNode)->plug_id;
        //  pull_NodefromPlug(bestNode, victim_plugid);
        //  push_NodetoPlug(bestNode, plugid);
        //  refer_Plug_Extracted(victim_plugid)->refresh = true;
        //  update_plug_shortage_power(victim_plugid);
        //  refer_Plug_Extracted(plugid)->refresh = true;
        //

        found = true;
    }
    pau_vector_destroy(scorelist);
    return found;
}
/* 断开被占用的直连节点，让当前充电桩接管 */
static void cutoff_root_node(struct Alloc_plugObj *pplug,
                             struct Alloc_nodeObj *pnode)
{
    if (pplug->state != PLUG_IDLE || pnode->state != NODE_OCCUPIED || pnode->plug_id <= ID_VAIN)
    {
        return;
    }
    ID_TYPE plug_intruder = pplug->id;
    ID_TYPE plug_victim = pnode->plug_id;
    pullout_further_nodes(pplug->connectedNode);
    pullout_matrices_related(plug_victim);
    update_plug_shortage_power(plug_victim);
    push_NodetoPlug(pplug->connectedNode, plug_intruder);
    update_plug_shortage_power(plug_intruder);
    transferPower(plug_intruder);
}
static bool have_plug_occupied_matrixnode(ID_TYPE plugid)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return false;
    }
    if (!ASSERT_TOPOTYPE_WHEEL_PLUS_SEMIMATRIX)
    {
        return false;
    }
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    PAU_VECTOR_FOREACH(nodeid, pplug->allocatedNodes)
    {
        if (nodeid > NODES_MAX_ENCIRCLE)
        {
            return true;
        }
    }
    return false;
}
static void collect_avatar_nodes(PAU_Vector *collect_vec, ID_TYPE plugid)
{
    int maxcontactor_nbr = 5 * NODES_MAX_ENCIRCLE / 2;
    maxcontactor_nbr = CONTACTOR_MAX - maxcontactor_nbr > 0 ? CONTACTOR_MAX - maxcontactor_nbr : 0;
    // 遍历所有接触器，判断是否闭合
    for (size_t c = NODES_MAX_ENCIRCLE * 2 + maxcontactor_nbr + 1; c <= CONTACTOR_MAX; c++)
    {
        struct Alloc_contactorObj *pcontactor = refer_Contactor_Extracted(c);
        if (!pcontactor->isClosed)
        {
            continue;
        }
        struct Alloc_nodeObj *pnode = refer_Node_Extracted(pcontactor->node1);

        if (pnode->plug_id != plugid)
        {
            continue;
        }
        pau_vector_append(collect_vec, pcontactor->node1);
    }
}
// 释放矩阵中的节点
static bool release_matrix_node(ID_TYPE plugid)
{
    if (!ASSERT_CONTACTOR_ID(plugid))
    {
        return false;
    }
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    PAU_VECTOR_FOREACH(nodeid, pplug->allocatedNodes)
    {
        ID_TYPE contactorid = NODE_MAX - nodeid;
        contactorid = CONTACTOR_MAX - contactorid;
        struct Alloc_contactorObj *pcontactor = refer_Contactor_Extracted(contactorid);
        if (nodeid > NODES_MAX_ENCIRCLE && !pcontactor->isClosed)
        {
            pull_NodefromPlug(nodeid, plugid);
            update_plug_shortage_power(plugid);
            return true;
        }
    }
    PAU_VECTOR_FOREACH(nodeid, pplug->allocatedNodes)
    {
        if (nodeid > NODES_MAX_ENCIRCLE)
        {
            pull_NodefromPlug(nodeid, plugid);
            update_plug_shortage_power(plugid);
            return true;
        }
    }
    return false;
}
/* 正常分配失败后的回退链：如果是SEMIMATRIX先尝试矩阵AVATAR，再尝试抢占已占节点 */
static bool node_extra_operate(ID_TYPE plugid, bool opType)
{
    if (ASSERT_TOPOTYPE_WHEEL_PLUS_SEMIMATRIX && !opType)
    {
        return release_matrix_node(plugid);
    }
    if (ASSERT_TOPOTYPE_WHEEL_PLUS_SEMIMATRIX && matrix_node_avatar(plugid))
    {
        return true;
    }
    return occupiednodes_preempt(plugid);
}

bool requestPower(ID_TYPE plugid, int requiredPower)
{
    /* ── 验证 ── */
    if (!ASSERT_PLUG_ID(plugid))
    {
        return false;
    }
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    if (!ASSERT_NODE_ID_ENCIRCLE(pplug->connectedNode))
    {
        return false;
    }

    /* ── 初始化 ── */
    struct Alloc_nodeObj *pnode = refer_Node_Extracted(pplug->connectedNode);
    pplug->requiredPower = requiredPower;
    update_plug_shortage_power(plugid);

    if (pplug->shortage <= 0)
    {
        return true;
    }

    /* ── cutoff connected node ── */
    pau_printf("[TACTIC] requestPower plugid:%d requiredpwr:%d shortage:%d\r\n",
               plugid, requiredPower, pplug->shortage);
    cutoff_root_node(pplug, pnode);

    /* ── shortage meeting ── */
    pplug->state = PLUG_CHARGING;
    int shortage = pplug->shortage;
    for (int guard = 0; pplug->shortage > 0; guard++)
    {
        if (!node_common_operate(plugid, NODE_OP_DISPENSE))
        {
            bool res = node_extra_operate(plugid, NODE_OP_DISPENSE);
            update_plug_shortage_power(plugid);
            return res;
        }
        update_plug_shortage_power(plugid);
        if (guard > shortage)
        {
            break;
        }
    }
    return true;
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
        res = have_plug_occupied_matrixnode(plugid);
        bool (*func)(ID_TYPE, bool) = res ? node_extra_operate : node_common_operate;
        res = func(plugid, NODE_OP_RELEASE);

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
void excircle_flowDirectioned(ID_TYPE plugid, FlowMap *object)
{
    if (!ASSERT_TOPOTYPE_WHEEL_PLUS_SEMIMATRIX)
    {
        return;
    }
    if (!ASSERT_PLUG_ID(plugid) || NULL == object)
    {
        return;
    }
    PAU_Vector *avatar_collcection = pau_vector_create(NODES_MAX_ENCIRCLE / 2);
    collect_avatar_nodes(avatar_collcection, plugid);
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    for (size_t c = 2 * NODES_MAX_ENCIRCLE + 1; c <= CONTACTOR_MAX; c++)
    {
        struct Alloc_contactorObj *pcontactor = refer_Contactor_Extracted(c);
        if (!pcontactor->isClosed)
        {
            continue;
        }
        pau_printf("[TACTIC] excircle_flowDirectioned contactorid:%d (%d-%d) is closed\r\n", c, pcontactor->node1, pcontactor->node2);
        if (pcontactor->node1 < NODES_MAX_ENCIRCLE || pcontactor->node2 < NODES_MAX_ENCIRCLE)
        {
            continue;
        }
        if (pcontactor->node2 > CONTACTOR_SPLICE_MULTIPLE)
        {
            if (!pau_vector_contains(pplug->allocatedNodes, pcontactor->node1))
            {
                continue;
            }
            ID_TYPE nodeid_alpha = pcontactor->node2 / CONTACTOR_SPLICE_MULTIPLE;
            ID_TYPE nodeid_beta = pcontactor->node2 % CONTACTOR_SPLICE_MULTIPLE;
            if (!pau_vector_contains(pplug->allocatedNodes, nodeid_alpha) && !pau_vector_contains(pplug->allocatedNodes, nodeid_beta))
            {
                continue;
            }
        }
        else if (!pau_vector_contains(pplug->allocatedNodes, pcontactor->node1) || !pau_vector_contains(pplug->allocatedNodes, pcontactor->node2))
        {
            continue;
        }
        object->contactorid = c;
        if (pcontactor->node2 < CONTACTOR_SPLICE_MULTIPLE)
        {
            if (pau_vector_contains(avatar_collcection, pcontactor->node1))
            {
                object->direction = pcontactor->node2;
            }
            if (pau_vector_contains(avatar_collcection, pcontactor->node2))
            {
                object->direction = pcontactor->node1;
            }
        }
        else
        {
            object->direction = pcontactor->node1;
            ID_TYPE nodeid_alpha = pcontactor->node2 / CONTACTOR_SPLICE_MULTIPLE;
            ID_TYPE nodeid_beta = pcontactor->node2 % CONTACTOR_SPLICE_MULTIPLE;
            if (pau_vector_contains(pplug->allocatedNodes, nodeid_alpha) && !pau_vector_contains(pplug->allocatedNodes, nodeid_beta))
            {
                object->appendix = nodeid_alpha;
            }
            else if (!pau_vector_contains(pplug->allocatedNodes, nodeid_alpha) && pau_vector_contains(pplug->allocatedNodes, nodeid_beta))
            {
                object->appendix = nodeid_beta;
            }
            else
            {
                object->appendix = nodeid_alpha;
            }
        }
        object += 1;
    }
    pau_vector_destroy(avatar_collcection);
}

FlowMap *encircle_flowDirectioned(ID_TYPE plugid, FlowMap *object)
{
    if (!ASSERT_PLUG_ID(plugid) || NULL == object)
    {
        return NULL;
    }
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    size_t cnt = pau_vector_size(pplug->allocatedNodes);
    hops_refresh(pplug->connectedNode, plugid);
    for (size_t c = 1; c <= 3 * NODES_MAX_ENCIRCLE / 2; c++)
    {
        struct Alloc_contactorObj *pcontactor = refer_Contactor_Extracted(c);
        if (!pcontactor->isClosed)
        {
            continue;
        }
        if (pcontactor->node1 > NODES_MAX_ENCIRCLE || pcontactor->node2 > NODES_MAX_ENCIRCLE)
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
        if (c > NODES_MAX_ENCIRCLE)
        {
            if (refer_Contactor_Extracted(c + NODES_MAX_ENCIRCLE / 2)->isClosed)
            {
                object->appendix = c + NODES_MAX_ENCIRCLE / 2;
            }
            else
            {
                object->contactorid = 0;
                object->direction = 0;
                object->appendix = 0;
                object -= 1;
            }
        }

        object += 1;
        if (0 == --cnt)
        {
            break;
        }
    }
    return object;
}