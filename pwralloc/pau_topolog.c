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
ID_TYPE get_neighbor_lower_alpha(ID_TYPE nodeid)
{
    if (ASSERT_NODE_ID_ENCIRCLE(nodeid))
    {
        return ID_VAIN;
    }
    return nodeid - NODES_MAX_ENCIRCLE;
}
ID_TYPE get_neighbor_lower_beta(ID_TYPE nodeid)
{
    if (ASSERT_NODE_ID_ENCIRCLE(nodeid))
    {
        return ID_VAIN;
    }
    return nodeid - NODES_MAX_ENCIRCLE / 2;
}

/*
 * @brief 获取统一图（线环 + 半矩阵）上某个节点的全部邻接点
 *
 * 线环节点：左、右、对径、以及其对应的矩阵节点（get_neighbor_upper）
 * 矩阵节点：两个下端点线环节点（对径开关的两端）以及其余全部矩阵节点
 *
 * 邻接点写入 neighbors[0..MAX_NODE_NEIGHBORS-1]，不足的位置填 0(ID_VAIN)，
 * 调用方遍历到 ID_VAIN 为止。
 */
void get_neighbors(ID_TYPE nodeid, ID_TYPE *neighbors)
{
    for (int i = 0; i < MAX_NODE_NEIGHBORS; i++)
    {
        neighbors[i] = ID_VAIN;
    }
    if (!ASSERT_NODE_ID(nodeid))
    {
        return;
    }
    if (nodeid <= NODES_MAX_ENCIRCLE)
    {
        // 线环节点：逆顺对各邻接点 + 对径
        neighbors[0] = get_neighbor_right(nodeid);
        neighbors[1] = get_neighbor_left(nodeid);
        neighbors[2] = get_neighbor_diagonal(nodeid);
        // 该线环节点对应的半矩阵节点（对径开关 + 4XX 接触器）
        if (ASSERT_TOPOTYPE_WHEEL_PLUS_SEMIMATRIX)
        {
            ID_TYPE upper = get_neighbor_upper(nodeid);
            if (ASSERT_NODE_ID(upper))
            {
                neighbors[3] = upper;
            }
        }
        return;
    }
    // 半矩阵节点：两个下端点线环节点 + 其余矩阵节点（矩阵母线全连接）
    int idx = 0;
    ID_TYPE lower_alpha = get_neighbor_lower_alpha(nodeid);
    ID_TYPE lower_beta = get_neighbor_lower_beta(nodeid);
    if (ASSERT_NODE_ID(lower_alpha))
    {
        neighbors[idx++] = lower_alpha;
    }
    if (ASSERT_NODE_ID(lower_beta))
    {
        neighbors[idx++] = lower_beta;
    }
    for (ID_TYPE matrix_node = NODES_MAX_ENCIRCLE + 1; matrix_node <= NODE_MAX; matrix_node++)
    {
        if (matrix_node == nodeid)
        {
            continue;
        }
        if (idx >= MAX_NODE_NEIGHBORS)
        {
            break;
        }
        neighbors[idx++] = matrix_node;
    }
}
bool is_furthernode_pathself(ID_TYPE plugid, ID_TYPE nodeid_alpha, ID_TYPE nodeid_beta)
{
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    hops_refresh(pplug->connectedNode, plugid);
    int hops_alpha = get_hops_occupied(pplug->connectedNode, nodeid_alpha, plugid);
    int hops_beta = get_hops_occupied(pplug->connectedNode, nodeid_beta, plugid);
    hops_refresh(nodeid_alpha, plugid);
    int hops_tween = get_hops_occupied(nodeid_alpha, nodeid_beta, plugid);
    if (hops_alpha < 0 || hops_beta < 0 || hops_tween < 0)
    {
        return false;
    }
    return ((hops_beta - hops_alpha) == hops_tween);
}

void expseudous_eisalethes(struct Alloc_plugObj *pplug)
{
    PAU_VECTOR_FOREACH(nodeid, pplug->allocatedNodes)
    {
        if (!is_node_pseudocycledon(nodeid))
        {
            continue;
        }
        ID_TYPE neighbors[MAX_NODE_NEIGHBORS] = {0};
        get_neighbors(nodeid, neighbors);
        for (int i = 0; i < MAX_NODE_NEIGHBORS; i++)
        {
            if (!ASSERT_NODE_ID(neighbors[i]))
            {
                continue;
            }
            struct Alloc_nodeObj *pneighbornode = refer_Node_Extracted(neighbors[i]);
            if (pplug->id != pneighbornode->plug_id)
            {
                continue;
            }
            if (pneighbornode->pseudocycledon)
            {
                continue;
            }
            recover_node_pseudocycledon(pplug->id, nodeid);
        }
    }
}
/**
 * @brief 更新某个充电桩相关的接触器状态：构建无环生成树
 *
 * 原则：
 *   1. 优先树形分叉，避免长链
 *   2. 分叉点尽量靠近直连根节点（BFS距离最小）
 *   3. 各分叉节点数量尽量平均
 *   4. 环形边优先于对角线边
 *
 * 算法：按 BFS 距离逐层建树 + 均衡分支
 *
 * @param pplug 充电桩对象
 */
/* 判断接触器是否把两个都属于该桩的端点连接起来（含 4XX 编码接触器） */
static bool contactor_belongs_plug(struct Alloc_contactorObj *c, struct Alloc_plugObj *pplug)
{
    if (NULL == c || NULL == pplug)
    {
        return false;
    }
    if (!pau_vector_contains(pplug->allocatedNodes, c->node1))
    {
        return false;
    }
    if (c->node2 > CONTACTOR_SPLICE_MULTIPLE)
    {
        ID_TYPE nodeid_alpha = c->node2 / CONTACTOR_SPLICE_MULTIPLE;
        ID_TYPE nodeid_beta = c->node2 % CONTACTOR_SPLICE_MULTIPLE;
        return pau_vector_contains(pplug->allocatedNodes, nodeid_alpha) ||
               pau_vector_contains(pplug->allocatedNodes, nodeid_beta);
    }
    return pau_vector_contains(pplug->allocatedNodes, c->node2);
}

/*
 * 查找连接 node_alpha 与 node_beta 的接触器。
 * 对“矩阵节点-线环节点”边，返回 4XX 接触器，并通过 appendix 输出
 * 对应的对径分段接触器编号（线环节点一侧到公共交点的接触器）。
 */
static struct Alloc_contactorObj *find_contactor_bynode(ID_TYPE node_alpha, ID_TYPE node_beta, ID_TYPE *appendix)
{
    if (appendix)
    {
        *appendix = ID_VAIN;
    }
    if (!ASSERT_NODE_ID(node_alpha) || !ASSERT_NODE_ID(node_beta))
    {
        return NULL;
    }
    bool alpha_matrix = (node_alpha > NODES_MAX_ENCIRCLE);
    bool beta_matrix = (node_beta > NODES_MAX_ENCIRCLE);
    if (alpha_matrix != beta_matrix)
    {
        ID_TYPE matrix_node = alpha_matrix ? node_alpha : node_beta;
        ID_TYPE ring_node = alpha_matrix ? node_beta : node_alpha;
        for (ID_TYPE c = 1; c <= CONTACTOR_MAX; c++)
        {
            struct Alloc_contactorObj *pcontactor = refer_Contactor_Extracted(c);
            if (pcontactor->node1 != matrix_node || pcontactor->node2 <= CONTACTOR_SPLICE_MULTIPLE)
            {
                continue;
            }
            ID_TYPE nodeid_alpha = pcontactor->node2 / CONTACTOR_SPLICE_MULTIPLE;
            ID_TYPE nodeid_beta = pcontactor->node2 % CONTACTOR_SPLICE_MULTIPLE;
            if (nodeid_alpha == ring_node || nodeid_beta == ring_node)
            {
                if (appendix)
                {
                    struct Alloc_contactorObj *psegment = refer_Contactor_Extracted(NODES_MAX_ENCIRCLE + ring_node);
                    *appendix = (NULL != psegment) ? psegment->id : ID_VAIN;
                }
                return pcontactor;
            }
        }
        return NULL;
    }
    for (ID_TYPE c = 1; c <= CONTACTOR_MAX; c++)
    {
        struct Alloc_contactorObj *pcontactor = refer_Contactor_Extracted(c);
        if (pcontactor->node2 > CONTACTOR_SPLICE_MULTIPLE)
        {
            continue;
        }
        if ((pcontactor->node1 == node_alpha && pcontactor->node2 == node_beta) ||
            (pcontactor->node1 == node_beta && pcontactor->node2 == node_alpha))
        {
            return pcontactor;
        }
    }
    return NULL;
}

static void close_edge_contactors(ID_TYPE node_alpha, ID_TYPE node_beta)
{
    ID_TYPE appendix = ID_VAIN;
    struct Alloc_contactorObj *pcontactor = find_contactor_bynode(node_alpha, node_beta, &appendix);
    if (NULL != pcontactor)
    {
        pcontactor->isClosed = true;
        /*
         * 仅对“线环-线环”对径边闭合镜像接触器：对径接触器成对出现（直径两端各一个），
         * 走直径必须同时闭合两端。而矩阵-线环边（appendix 非空）只闭合被挑选线环节点
         * 一侧的单个对径分段，绝不闭合其镜像，否则会在功率潮流路径中形成环路。
         */
        if (ID_VAIN == appendix &&
            pcontactor->id > NODES_MAX_ENCIRCLE && pcontactor->id <= 2 * NODES_MAX_ENCIRCLE)
        {
            ID_TYPE mirror = (pcontactor->id <= 3 * NODES_MAX_ENCIRCLE / 2)
                                 ? pcontactor->id + NODES_MAX_ENCIRCLE / 2
                                 : pcontactor->id - NODES_MAX_ENCIRCLE / 2;
            refer_Contactor_Extracted(mirror)->isClosed = true;
        }
    }
    if (ASSERT_CONTACTOR_ID(appendix))
    {
        refer_Contactor_Extracted(appendix)->isClosed = true;
    }
}

/**
 * @brief 更新某个充电桩相关的接触器状态：构建无环生成树
 *
 * 原则：
 *   1. 优先树形分叉，避免长链
 *   2. 分叉点尽量靠近直连根节点（BFS距离最小）
 *   3. 各分叉节点数量尽量平均
 *   4. 环形边优先于对角线/矩阵边
 *
 * 线环节点与半矩阵节点共用同一张图，因此本函数对两类节点统一建树，
 * 不再需要单独的半矩阵接触器维护逻辑。
 *
 * @param pplug 充电桩对象
 */
static void
acyclic_tree_building(struct Alloc_plugObj *pplug)
{
    if (pau_vector_size(pplug->allocatedNodes) == 0)
    {
        return;
    }

    /* ── 0. 先断开所有相关接触器（含矩阵接触器） ── */
    for (int i = 1; i <= (int)CONTACTOR_MAX; i++)
    {
        struct Alloc_contactorObj *c = refer_Contactor_Extracted(i);
        if (contactor_belongs_plug(c, pplug))
        {
            c->isClosed = false;
        }
    }

    /* ── 1. 以直连节点为根做 BFS，得到距离层级 ── */
    ID_TYPE root = pplug->connectedNode;
    if (!ASSERT_NODE_ID_ENCIRCLE(root))
    {
        return;
    }

    /* 距离表：仅对已分配节点有效 */
    int dist[MAXNODES_MEM_LMT + 1];
    memset(dist, -1, sizeof(dist));

    /* 手写队列 */
    ID_TYPE queue[MAXNODES_MEM_LMT];
    int qh = 0, qt = 0;

    dist[root] = 0;
    queue[qt++] = root;

    while (qh < qt)
    {
        ID_TYPE u = queue[qh++];
        ID_TYPE neighbors[MAX_NODE_NEIGHBORS] = {0};
        get_neighbors(u, neighbors);
        for (int i = 0; i < MAX_NODE_NEIGHBORS; i++)
        {
            ID_TYPE v = neighbors[i];
            if (!ASSERT_NODE_ID(v))
            {
                continue;
            }
            if (!pau_vector_contains(pplug->allocatedNodes, v))
            {
                continue;
            }
            if (refer_Node_Extracted(v)->pseudocycledon)
            {
                continue;
            }
            if (dist[v] == -1)
            {
                dist[v] = dist[u] + 1;
                queue[qt++] = v;
            }
        }
    }

    /* ── 2. 并查集初始化（仅对已分配的非伪环路节点） ── */
    clear_parent();
    PAU_VECTOR_FOREACH(node, pplug->allocatedNodes)
    {
        if (refer_Node_Extracted(node)->pseudocycledon)
        {
            continue;
        }
        set_parent(node, node);
    }

    /* ── 3. 记录每个节点的子节点数，用于均衡分支 ── */
    int childCount[MAXNODES_MEM_LMT + 1];
    memset(childCount, 0, sizeof(childCount));

    /* ── 4. 收集候选边，并按 (距离和, 是否环形, 父节点子节点数) 排序 ── */
    typedef struct
    {
        ID_TYPE u;     /* 较远节点 */
        ID_TYPE v;     /* 较近节点（候选父） */
        bool diagonal; /* 是否对角线/矩阵边 */
        int distSum;   /* dist[u] + dist[v]，越小越靠近根 */
        int childLoad; /* childCount[v]，越小分支越均衡 */
    } CandidateEdge;

    CandidateEdge candidates[MAX_GRAPH_UNDIRECTED_EDGES];
    int candidateCnt = 0;

    PAU_VECTOR_FOREACH(node, pplug->allocatedNodes)
    {
        if (refer_Node_Extracted(node)->pseudocycledon)
        {
            continue;
        }
        if (dist[node] < 0)
        {
            continue; /* 不在 BFS 可达范围内 */
        }

        ID_TYPE neighbors[MAX_NODE_NEIGHBORS] = {0};
        get_neighbors(node, neighbors);
        for (int i = 0; i < MAX_NODE_NEIGHBORS; i++)
        {
            ID_TYPE nbr = neighbors[i];
            if (!ASSERT_NODE_ID(nbr))
            {
                continue;
            }
            if (!pau_vector_contains(pplug->allocatedNodes, nbr))
            {
                continue;
            }
            if (refer_Node_Extracted(nbr)->pseudocycledon)
            {
                continue;
            }
            if (dist[nbr] < 0)
            {
                continue;
            }
            /* 避免重复添加同一条边（node < nbr 时添加） */
            if (node >= nbr)
            {
                continue;
            }

            CandidateEdge e;
            /* 让 u 为较远节点，v 为较近节点（候选父） */
            if (dist[node] >= dist[nbr])
            {
                e.u = node;
                e.v = nbr;
            }
            else
            {
                e.u = nbr;
                e.v = node;
            }
            /* get_neighbors 前两个是线环/下端相邻边，其余为对径或矩阵边 */
            e.diagonal = (i >= 2);
            e.distSum = dist[e.u] + dist[e.v];
            e.childLoad = 0; /* 排序后再根据实时 childCount 决定，这里先填0 */
            candidates[candidateCnt++] = e;
        }
    }

    /* ── 5. 按 distSum 升序排序（越靠近根越优先），同距离环形优先 ── */
    for (int i = 0; i < candidateCnt - 1; i++)
    {
        int minIdx = i;
        for (int j = i + 1; j < candidateCnt; j++)
        {
            bool swap = false;
            if (candidates[j].distSum < candidates[minIdx].distSum)
            {
                swap = true;
            }
            else if (candidates[j].distSum == candidates[minIdx].distSum)
            {
                /* 环形优先于对角线/矩阵边 */
                if (!candidates[j].diagonal && candidates[minIdx].diagonal)
                {
                    swap = true;
                }
            }
            if (swap)
            {
                minIdx = j;
            }
        }
        if (minIdx != i)
        {
            CandidateEdge tmp = candidates[i];
            candidates[i] = candidates[minIdx];
            candidates[minIdx] = tmp;
        }
    }

    /* ── 6. 逐边闭合，使用并查集避免环 ── */
    for (int i = 0; i < candidateCnt; i++)
    {
        CandidateEdge e = candidates[i];

        /* 已连通则跳过（避免成环） */
        if (find(e.u) == find(e.v))
        {
            continue;
        }

        /* 闭合接触器（矩阵-线环边会同时闭合 4XX 与其对径分段） */
        close_edge_contactors(e.u, e.v);

        unite(e.u, e.v);
        childCount[e.v]++; /* 记录父节点子节点数，供后续均衡使用 */
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
    //
    expseudous_eisalethes(pplug);
    // 半矩阵接触器为多桩共享，先统一断开，再按各桩生成树重新闭合
    if (ASSERT_TOPOTYPE_WHEEL_PLUS_SEMIMATRIX)
    {
        for (size_t i = 2 * NODES_MAX_ENCIRCLE + 1; i <= CONTACTOR_MAX; i++)
        {
            refer_Contactor_Extracted(i)->isClosed = false;
        }
    }
    //  为单个充电桩已占用的节点集合，自动闭合接触器，形成一棵无环、连通、优先走环形边、必要时走对角线/矩阵边的生成树。
    acyclic_tree_building(pplug);
    // 其余在充充电桩的接触器生成树也需重算，保证共享矩阵母线上的路径一致
    for (ID_TYPE i = 1; i <= PLUG_MAX; i++)
    {
        struct Alloc_plugObj *pother = refer_Plug_Extracted(i);
        if (PLUG_IDLE == pother->state || pother->id == plugid)
        {
            continue;
        }
        acyclic_tree_building(pother);
    }
}

static void pull_NodefromPlug(ID_TYPE nodeid, ID_TYPE plugid)
{
    if (!ASSERT_NODE_ID(nodeid) || !ASSERT_PLUG_ID(plugid))
    {
        return;
    }

    struct Alloc_nodeObj *pnode = refer_Node_Extracted(nodeid);

    if (pnode->plug_id != plugid)
    {
        return;
    }
    set_locked(0, nodeid);
    // 释放节点,更新数据
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    pnode->plug_id = ID_VAIN;
    pnode->priority = PRIOR_VAIN;
    pnode->pseudocycledon = false;
    pau_vector_remove(pplug->allocatedNodes, nodeid); // 从在充节点名单中除名该节点
    OUTPUTPWR -= pnode->power_available;
    OUTPUTPWR = OUTPUTPWR < 0 ? 0 : OUTPUTPWR;
    pau_printf(" release node %d from plug %d\r\n", nodeid, plugid);
    if (pnode->state == NODE_OCCUPIED)
    {
        pnode->state = NODE_IDLEFREE;
    }
    if (pnode->state == NODE_OUTORDER)
    {
        pnode->state = NODE_DISABLED;
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
    if (pnode->plug_id != ID_VAIN)
    {
        return;
    }
    set_locked(plugid, nodeid);
    // 安插节点,更新数据
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    pnode->plug_id = plugid;
    pnode->priority = pplug->priority;
    pau_vector_append(pplug->allocatedNodes, nodeid);
    OUTPUTPWR += pnode->power_available;
    pau_printf(" allocate node %d to plug %d\r\n", nodeid, plugid);
    if (pnode->state == NODE_IDLEFREE)
    {
        pnode->state = NODE_OCCUPIED;
    }
    if (pnode->state == NODE_DISABLED)
    {
        pnode->state = NODE_OUTORDER;
    }

    updateContactorStates(plugid, 0);
}
void push_NodetoPlug_pseudocyclose(ID_TYPE nodeid, ID_TYPE plugid)
{
    if (!ASSERT_NODE_ID(nodeid) || !ASSERT_PLUG_ID(plugid))
    {
        return;
    }
    struct Alloc_nodeObj *pnode = refer_Node_Extracted(nodeid);
    if (pnode->plug_id != ID_VAIN)
    {
        return;
    }

    // 安插节点,更新数据
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    pnode->plug_id = plugid;
    pnode->priority = pplug->priority;
    pnode->pseudocycledon = true;
    pau_vector_append(pplug->allocatedNodes, nodeid);
    pau_printf(" allocate pseudocyclose_node %d to plug %d\r\n", nodeid, plugid);
    if (pnode->state == NODE_IDLEFREE)
    {
        pnode->state = NODE_OCCUPIED;
    }
    if (pnode->state == NODE_DISABLED)
    {
        pnode->state = NODE_OUTORDER;
    }
}

void pull_NodefromPlug_pseudocyclose(ID_TYPE nodeid, ID_TYPE plugid)
{
    if (!ASSERT_NODE_ID(nodeid) || !ASSERT_PLUG_ID(plugid))
    {
        return;
    }

    struct Alloc_nodeObj *pnode = refer_Node_Extracted(nodeid);

    if (pnode->plug_id != plugid)
    {
        return;
    }

    // 释放节点,更新数据
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    pnode->plug_id = ID_VAIN;
    pnode->priority = PRIOR_VAIN;
    pnode->pseudocycledon = false;
    pau_vector_remove(pplug->allocatedNodes, nodeid); // 从在充节点名单中除名该节点
    pau_printf(" release pseudocyclose node %d from plug %d\r\n", nodeid, plugid);
    if (pnode->state == NODE_OCCUPIED)
    {
        pnode->state = NODE_IDLEFREE;
    }
    if (pnode->state == NODE_OUTORDER)
    {
        pnode->state = NODE_DISABLED;
    }

    if (0 == pau_vector_size(pplug->allocatedNodes))
    {
        pplug->state = PLUG_IDLE;
        pplug->priority = PRIOR_VAIN;
    }
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
    PAU_Vector *victim_allocatednodes_copy = pau_vector_clone(victim_pplug->allocatedNodes);
    PAU_Vector *nodes_to_release = pau_vector_create(MAXNODES_MEM_LMT);

    ID_TYPE node_survive = ID_VAIN;
    // 遍历所有victim_pplug占据的matrix节点,如果该节点在线环中的node1,node2连接点不再属于victim_pplug的allocatedNodes,则移除该节点
    PAU_VECTOR_FOREACH(nodeid, victim_allocatednodes_copy)
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
        if (pcontactor->node2 < CONTACTOR_SPLICE_MULTIPLE)
        {
            continue;
        }
        ID_TYPE node_appha = pcontactor->node2 / CONTACTOR_SPLICE_MULTIPLE;
        ID_TYPE node_beta = pcontactor->node2 % CONTACTOR_SPLICE_MULTIPLE;

        if (victim_plugid != refer_Node_Extracted(node_appha)->plug_id && victim_plugid != refer_Node_Extracted(node_beta)->plug_id)
        {
            pau_vector_append(nodes_to_release, nodeid);
        }
        else if (is_node_pseudocycledon(node_appha))
        {
            pau_vector_append(nodes_to_release, nodeid);
            pau_vector_append(nodes_to_release, node_appha);
        }
        else if (is_node_pseudocycledon(node_beta))
        {
            pau_vector_append(nodes_to_release, nodeid);
            pau_vector_append(nodes_to_release, node_beta);
        }
        else
        {
            node_survive = nodeid;
        }
    }
    if (ID_VAIN == node_survive)
    {
        victim_pplug->refresh = true;

        PAU_VECTOR_FOREACH(relased_node, nodes_to_release)
        {
            pull_NodefromPlug(relased_node, victim_plugid);
        }
    }
    pau_vector_destroy(nodes_to_release);
    pau_vector_destroy(victim_allocatednodes_copy);
}

static void pullout_pseudocycloma_node(ID_TYPE nodeid)
{
    if (!ASSERT_TOPOTYPE_WHEEL_PLUS_SEMIMATRIX)
    {
        return;
    }
    if (!ASSERT_NODE_ID_ENCIRCLE(nodeid))
    {
        return;
    }
    if (!is_node_pseudocycledon(nodeid))
    {
        return;
    }
    struct Alloc_nodeObj *pnode = refer_Node_Extracted(nodeid);
    if (pnode->plug_id == ID_VAIN)
    {
        return;
    }
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(pnode->plug_id);
    if (get_plug_chargingnodes_cnt(pnode->plug_id) == 1) // 如果节点所属充电桩仅剩一个节点，则不能进行节点移除
    {
        return;
    }
    pull_NodefromPlug_pseudocyclose(nodeid, pnode->plug_id);
    pplug->refresh = true;
}
static void pullout_further_nodes(ID_TYPE nodeid)
{
    if (!ASSERT_NODE_ID_ENCIRCLE(nodeid))
    {
        return;
    }

    struct Alloc_nodeObj *pnode = refer_Node_Extracted(nodeid);
    if (pnode->plug_id == ID_VAIN)
    {
        return;
    }
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(pnode->plug_id);
    if (get_plug_chargingnodes_cnt(pnode->plug_id) == 1) // 如果节点所属充电桩仅剩一个节点，则不能进行节点移除
    {
        return;
    }

    PAU_Vector *releasenode_list = pau_vector_create(PAU_VECTOR_DEFAULT_CAPACITY);
    if (NULL == releasenode_list)
    {
        return;
    }

    PAU_VECTOR_FOREACH(allocated_nodeid, pplug->allocatedNodes) // 遍历节点所属桩已分配的节点
    {

        if (is_furthernode_pathself(pnode->plug_id, nodeid, allocated_nodeid)) // 如果节点所属桩已分配的节点到当前移除节点的跳数等于各自到基直连节点的差值
        {
            pau_vector_append(releasenode_list, allocated_nodeid); // 找到所有跳数大于hops_compared的节点
        }
    }

    PAU_VECTOR_FOREACH(releasenodeid, releasenode_list)
    {
        pull_NodefromPlug(releasenodeid, pplug->id); // 释放plugid所连接的节点中所有大于hops_compared的节点
    }

    pplug->refresh = true;
    pplug->priority += PRIOR_ADHOC; // 被切断的节点所属充电桩优先级提高以便在之后的分配中恢复
    pau_vector_destroy(releasenode_list);
}
static ID_TYPE find_euelect_node_near(ID_TYPE plugid, ID_TYPE startid, size_t quota)
{
    if (!ASSERT_PLUG_ID(plugid) || !ASSERT_NODE_ID_ENCIRCLE(startid))
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

    for (int nodeid = 1; nodeid <= (int)NODE_MAX; nodeid++)
    {
        size_t score = makeScore(SENARIO_ACQUIRE, quota, plugid, 1, nodeid, 1);
        // pau_printf("[%02d]%d \r\n", nodeid, score);
        //  遍历节点的每个邻居节点
        pau_vector_set(scorelist, nodeid, score);
    }

    // 找到得分最高并且大于及格线的点（线环 + 半矩阵节点统一比较）
    int bestScore = -1;
    for (int nodeid = 0; nodeid < (int)NODE_MAX; ++nodeid)
    {
        // 等积分优先级:顺序>逆序>对角>环外
        int index_reordered = startid + NODES_MAX_ENCIRCLE + ((nodeid + 1) / 2) * ((nodeid + 1) % 2 > 0 ? -1 : 1);
        index_reordered = (index_reordered - 1) % NODES_MAX_ENCIRCLE + 1;
        int score = (int)pau_vector_at(scorelist, index_reordered);
        if (score > bestScore && score > WEIGHT_5)
        {
            bestScore = score;
            optimal_index = index_reordered;
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
    (void)quota;
    if (!ASSERT_PLUG_ID(plugid) || !ASSERT_NODE_ID_ENCIRCLE(startid))
    {
        return ID_VAIN;
    }

    ID_TYPE max_index = ID_VAIN;
    for (ID_TYPE nodeid = 1; nodeid <= NODE_MAX; nodeid++)
    {
        //  检查是否为非ID_VAIN且当前是最大值（线环 + 半矩阵节点统一比较）
        if (get_dist(nodeid) >= 0 && get_locked(nodeid) == plugid &&
            (max_index == ID_VAIN || get_dist(nodeid) > get_dist(max_index)))
        {
            max_index = nodeid;
        }
    }
    if (max_index != ID_VAIN)
    {
        set_dist(max_index, -1);
    }
    return max_index; // 返回最大值的索引，如果没找到则返回-1
}

static bool reactive_tuning_encircle(ID_TYPE plugid)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return false;
    }
    // 1.先收集所有已占节点邻接到的其他充电桩plugid（线环 + 半矩阵统一处理）
    PAU_Vector *plugs_shovedover = pau_vector_create(PAU_VECTOR_DEFAULT_CAPACITY);
    PAU_Vector *nodes_shovedover = pau_vector_create(PAU_VECTOR_DEFAULT_CAPACITY);
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    PAU_VECTOR_FOREACH(nodeid, pplug->allocatedNodes)
    {
        ID_TYPE node_neighbors[MAX_NODE_NEIGHBORS] = {ID_VAIN};
        get_neighbors(nodeid, node_neighbors);
        for (int i = 0; i < MAX_NODE_NEIGHBORS; i++)
        {
            ID_TYPE neighborid = node_neighbors[i];
            if (!ASSERT_NODE_ID(neighborid))
            {
                break;
            }
            struct Alloc_nodeObj *pneighbor = refer_Node_Extracted(neighborid);
            if (ID_VAIN < pneighbor->plug_id && !pau_vector_contains(plugs_shovedover, pneighbor->plug_id))
            {
                if (pneighbor->plug_id == plugid)
                {
                    continue;
                }
                if (neighborid == get_plug_connectednode(pneighbor->plug_id))
                {
                    continue;
                }
                if (1 >= get_plug_chargingnodes_cnt(pneighbor->plug_id))
                {
                    continue;
                }
                pau_vector_append(nodes_shovedover, neighborid);
                pau_vector_append(plugs_shovedover, pneighbor->plug_id);
            }
        }
    }

    ID_TYPE plugid_shovedover = ID_VAIN;
    ID_TYPE nodeid_replacement = ID_VAIN;
    // 2.遍历所有空闲节点,如果这个空闲点有邻接点在被收集到的充电桩充电
    for (ID_TYPE nodeid = 1; nodeid <= NODE_MAX; nodeid++)
    {
        struct Alloc_nodeObj *pnode = refer_Node_Extracted(nodeid);
        if (ID_VAIN < pnode->plug_id) // NODE_OUTORDER和NODE_IDLEFREE状态都可以
        {
            continue;
        }
        ID_TYPE node_neighbors[MAX_NODE_NEIGHBORS] = {ID_VAIN};
        get_neighbors(nodeid, node_neighbors);
        for (int i = 0; i < MAX_NODE_NEIGHBORS; i++)
        {
            if (!ASSERT_NODE_ID(node_neighbors[i]))
            {
                break;
            }
            struct Alloc_nodeObj *pneighbor = refer_Node_Extracted(node_neighbors[i]);
            if (ID_VAIN < pneighbor->plug_id && pau_vector_contains(plugs_shovedover, pneighbor->plug_id))
            {
                if (pneighbor->pseudocycledon)
                {
                    continue;
                }

                plugid_shovedover = pneighbor->plug_id;
                nodeid_replacement = nodeid;
                break;
            }
        }
        if (plugid_shovedover != ID_VAIN && nodeid_replacement != ID_VAIN)
        {
            break;
        }
    }
    // 3.在可抢占的nodeid_replacement集合中找到被占plugid是plugid_shovedover的节点
    if (ID_VAIN == plugid_shovedover || ID_VAIN == nodeid_replacement)
    {
        pau_vector_destroy(plugs_shovedover);
        pau_vector_destroy(nodes_shovedover);
        return false;
    }
    bool stop = false;
    PAU_VECTOR_FOREACH_BREAK(nodeid, nodes_shovedover, stop)
    {
        struct Alloc_nodeObj *pnode = refer_Node_Extracted(nodeid);
        if (pnode->plug_id == plugid_shovedover)
        {
            ID_TYPE recover_locked = get_locked(nodeid);
            set_locked(plugid, nodeid);
            set_locked(plugid_shovedover, nodeid_replacement);
            ID_TYPE connectedNode_shovedover = refer_Plug_Extracted(plugid_shovedover)->connectedNode;
            hops_refresh(connectedNode_shovedover, plugid_shovedover);
            int hops = get_hops_occupied(connectedNode_shovedover, nodeid_replacement, plugid_shovedover);
            set_locked(recover_locked, nodeid);
            set_locked(0, nodeid_replacement);

            if (0 >= hops)
            {
                pau_printf("[PAU] reactive_tuning_encircle: plugid %d replace node%d of plug%d with free node %d rehearsal failed!!!\r\n", plugid, nodeid, plugid_shovedover, nodeid_replacement);
                continue;
            }
            push_NodetoPlug(nodeid_replacement, plugid_shovedover);
            pull_NodefromPlug(nodeid, plugid_shovedover);
            update_plug_shortage_power(plugid_shovedover);
            refer_Plug_Extracted(plugid_shovedover)->refresh = true;
            push_NodetoPlug(nodeid, plugid);
            update_plug_shortage_power(plugid);
            refer_Plug_Extracted(plugid)->refresh = true;
            plugid_shovedover = ID_VAIN;
            stop = true;
            break;
        }
    }
    pau_vector_destroy(plugs_shovedover);
    pau_vector_destroy(nodes_shovedover);
    return (ID_VAIN == plugid_shovedover);
}
bool occupiednodes_preempt(ID_TYPE plugid)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return false;
    }
    // 统一图下，线环与半矩阵节点的抢占均由 reactive_tuning_encircle 处理
    return reactive_tuning_encircle(plugid);
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

        ID_TYPE critia = (opType == NODE_OP_DISPENSE) ? ID_VAIN : pplug->id;
        void (*func)(ID_TYPE, ID_TYPE) = (opType == NODE_OP_DISPENSE) ? push_NodetoPlug : pull_NodefromPlug;

        if (poptimal_node->plug_id != critia)
        {
            continue;
        }
        // 分配与释放统一先执行节点操作，再结算 quota，
        // 避免“quota 恰好扣到 0 时节点未被真正释放”的缺陷
        func(optimal_node, plugid);
        pau_printf("%s nodeid:%d plugid:%d\r\n", __FUNCTION__, optimal_node, plugid);
        quota -= poptimal_node->power_available;
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
static int get_neighbors_occupied(ID_TYPE nodeid)
{
    if (!ASSERT_NODE_ID(nodeid))
    {
        return 4;
    }
    ID_TYPE neighbor[MAX_NODE_NEIGHBORS] = {0};
    get_neighbors(nodeid, neighbor);
    int cnt = 0;
    for (int i = 0; i < MAX_NODE_NEIGHBORS; i++)
    {
        if (neighbor[i] != ID_VAIN && refer_Node_Extracted(neighbor[i])->plug_id != ID_VAIN)
        {
            cnt++;
        }
    }
    return cnt;
}
static bool idlenodes_semimatrix_donatio(ID_TYPE plugid)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return false;
    }
    if (!ASSERT_TOPOTYPE_WHEEL_PLUS_SEMIMATRIX)
    {
        return false;
    }
    bool ret = false;
    for (ID_TYPE nodeid = NODES_MAX_ENCIRCLE + 1; nodeid <= NODE_MAX; nodeid++)
    {
        struct Alloc_nodeObj *pnode = refer_Node_Extracted(nodeid);
        if (pnode->plug_id > ID_VAIN)
        {
            continue;
        }
        ID_TYPE lower_alpha = get_neighbor_lower_alpha(nodeid);
        ID_TYPE lower_beta = get_neighbor_lower_beta(nodeid);
        ID_TYPE plugid_alpha = refer_Node_Extracted(lower_alpha)->plug_id;
        int shortage_alpha = ID_VAIN != plugid_alpha ? get_plug_shortage(plugid_alpha) : 0;
        ID_TYPE plugid_beta = refer_Node_Extracted(lower_beta)->plug_id;
        int shortage_beta = ID_VAIN != plugid_beta ? get_plug_shortage(plugid_beta) : 0;
        ID_TYPE successor = (int)(shortage_alpha > 0 | shortage_beta > 0) * ((shortage_alpha > shortage_beta) ? plugid_alpha : plugid_beta);
        if (ID_VAIN == successor || plugid == successor)
        {
            continue;
        }

        push_NodetoPlug(nodeid, successor);
        refer_Plug_Extracted(successor)->refresh = true;
        update_plug_shortage_power(successor);
        ret = true;
    }
    int optimal_score = 0;
    ID_TYPE optimal_plug = ID_VAIN;
    ID_TYPE idle_node = ID_VAIN;
    for (ID_TYPE nodeid = NODES_MAX_ENCIRCLE + 1; nodeid <= NODE_MAX; nodeid++)
    {

        struct Alloc_nodeObj *pnode = refer_Node_Extracted(nodeid);
        if (pnode->plug_id > ID_VAIN)
        {
            size_t score = makeScore(SENARIO_MATRICE, 0, pnode->plug_id, 1, 1, 1);

            if (score >= WEIGHT_3 && score > optimal_score)
            {
                optimal_score = score;
                optimal_plug = pnode->plug_id;
            }
        }
        else
        {
            idle_node = nodeid;
        }
    }
    if (optimal_plug > ID_VAIN && idle_node > ID_VAIN)
    {
        push_NodetoPlug(idle_node, optimal_plug);
        refer_Plug_Extracted(optimal_plug)->refresh = true;
        update_plug_shortage_power(optimal_plug);
        ret = true;
    }
    return ret;
}
static bool idlenodes_encircle_donatio(ID_TYPE plugid)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return false;
    }
    PAU_Vector *idlenode_list = pau_vector_create(NODES_MAX_ENCIRCLE);
    if (NULL == idlenode_list)
    {
        return false;
    }
    // 优先将非connectednnode的节点放到列表中
    for (ID_TYPE nodeid = 1; nodeid <= NODES_MAX_ENCIRCLE; nodeid++)
    {
        struct Alloc_nodeObj *pnode = refer_Node_Extracted(nodeid);
        if (pnode->plug_id == ID_VAIN && pnode->state == NODE_IDLEFREE && !isConnectedNode(nodeid))
        {
            pau_vector_append(idlenode_list, nodeid);
        }
    }
    for (ID_TYPE nodeid = 1; nodeid <= NODES_MAX_ENCIRCLE; nodeid++)
    {
        struct Alloc_nodeObj *pnode = refer_Node_Extracted(nodeid);
        if (pnode->plug_id == ID_VAIN && !pau_vector_contains(idlenode_list, nodeid) && get_neighbors_occupied(nodeid) < 2)
        {
            pau_vector_append(idlenode_list, nodeid);
        }
    }
    for (ID_TYPE nodeid = 1; nodeid <= NODES_MAX_ENCIRCLE; nodeid++)
    {
        struct Alloc_nodeObj *pnode = refer_Node_Extracted(nodeid);
        if (pnode->plug_id == ID_VAIN && !pau_vector_contains(idlenode_list, nodeid))
        {
            pau_vector_append(idlenode_list, nodeid);
        }
    }
    // 按照每个节点的三个邻居节点中被occupied的个数来排序

    bool ret = false;
    PAU_VECTOR_FOREACH(idlenode, idlenode_list)
    {
        ID_TYPE neighbor[MAX_NODE_NEIGHBORS] = {0};
        struct
        {
            size_t score;
            ID_TYPE plugid;
        } plug_score[MAX_NODE_NEIGHBORS] = {{0, ID_VAIN}}, optimal = {0, ID_VAIN};
        get_neighbors(idlenode, neighbor);
        for (int i = 0; i < MAX_NODE_NEIGHBORS; i++)
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
                plug_score[i].score = makeScore(SENARIO_INHERIT, 0, plugid, pneighbor->plug_id, idlenode, neighborid);
            }
        }
        for (int i = 0; i < MAX_NODE_NEIGHBORS; i++)
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
static bool transferPower(ID_TYPE plugid, bool (*func)(ID_TYPE))
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return false;
    }
    if (ASSERT_FLOW_EMBARGO)
    {
        return false;
    }
    int loop_guard = 0;
    while (loop_guard < NODES_MAX_ENCIRCLE)
    {
        bool res = func(plugid);
        if (!res)
        {
            break;
        }
        loop_guard++;
    }
    return (loop_guard > 0);
}

static ID_TYPE ana_katabatic_flow(ID_TYPE plugid)
{
    if (!ASSERT_PLUG_ID(plugid))
    {
        return false;
    }

    for (int i = 0; i < NODES_MAX_ENCIRCLE / 2; i++)
    {
        struct Alloc_contactorObj *c = refer_Contactor_Extracted(CONTACTOR_MAX - i);
        if (c->node1 == ID_VAIN)
        {
            continue;
        }
        if (plugid != refer_Node_Extracted(c->node1)->plug_id)
        {
            continue;
        }
        ID_TYPE nodeid_alpha = c->node2 / CONTACTOR_SPLICE_MULTIPLE;
        if (ID_VAIN < nodeid_alpha && ID_VAIN == refer_Node_Extracted(nodeid_alpha)->plug_id)
        {
            return nodeid_alpha;
        }
        ID_TYPE nodeid_beta = c->node2 % CONTACTOR_SPLICE_MULTIPLE;
        if (ID_VAIN < nodeid_beta && ID_VAIN == refer_Node_Extracted(nodeid_beta)->plug_id)
        {
            return nodeid_beta;
        }
    }
    return ID_VAIN;
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
    // 找到socrelist中得分最高的编号
    int bestScore = -1;
    ID_TYPE bestNode = ID_VAIN;
    bestNode = ana_katabatic_flow(plugid);
    if (bestNode > ID_VAIN)
    {
        push_NodetoPlug_pseudocyclose(bestNode, plugid);
        updateContactorStates(plugid, bestNode);
        refer_Plug_Extracted(plugid)
            ->refresh = true;
        return true;
    }
    PAU_Vector *scorelist = pau_vector_create(NODE_MAX - NODES_MAX_ENCIRCLE);
    if (NULL == scorelist)
    {
        return false;
    }
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

    for (int n = 1; n <= (NODE_MAX - NODES_MAX_ENCIRCLE); ++n)
    {
        int score = pau_vector_at(scorelist, n);
        if (score > bestScore)
        {
            bestScore = score;
            bestNode = n + NODES_MAX_ENCIRCLE;
        }
    }
    if (bestScore > WEIGHT_6)
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

        found = false;
    }
    pau_vector_destroy(scorelist);
    return found;
}
/* 断开被占用的直连节点，让当前充电桩接管 */
static void cutoff_root_node(struct Alloc_plugObj *pplug,
                             struct Alloc_nodeObj *pnode)
{
    if (pplug->state != PLUG_IDLE || pnode->plug_id <= ID_VAIN)
    {
        return;
    }
    ID_TYPE plug_intruder = pplug->id;
    ID_TYPE plug_victim = pnode->plug_id;
    pullout_pseudocycloma_node(pplug->connectedNode);
    pullout_further_nodes(pplug->connectedNode);
    pullout_matrices_related(plug_victim);
    update_plug_shortage_power(plug_victim);
    push_NodetoPlug(pplug->connectedNode, plug_intruder);
    update_plug_shortage_power(plug_intruder);
    transferPower(plug_intruder, idlenodes_encircle_donatio);
    transferPower(plug_intruder, idlenodes_semimatrix_donatio);
    if (PRIOR_ADHOC < refer_Plug_Extracted(plug_victim)->priority)
    {
        refer_Plug_Extracted(plug_victim)->priority -= PRIOR_ADHOC;
    }
}
bool have_plug_occupied_matrixnode(ID_TYPE plugid)
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
        struct Alloc_nodeObj *pnode = refer_Node_Extracted(nodeid);
        if (pnode->pseudocycledon)
        {
            pull_NodefromPlug(nodeid, plugid);
            update_plug_shortage_power(plugid);
            return true;
        }
    }
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
    struct Alloc_nodeObj *pconnectedNode = refer_Node_Extracted(pplug->connectedNode);
    pplug->requiredPower = requiredPower;
    update_plug_shortage_power(plugid);

    if (pplug->shortage <= 0)
    {
        return true;
    }

    /* ── cutoff connected node ── */
    pau_printf("[TACTIC] requestPower plugid:%d requiredpwr:%d shortage:%d\r\n",
               plugid, requiredPower, pplug->shortage);
    cutoff_root_node(pplug, pconnectedNode);

    /* ── shortage meeting ── */
    pplug->state = PLUG_CHARGING;

    for (int guard = 0; pplug->shortage > 0; guard++)
    {
        bool res = node_common_operate(plugid, NODE_OP_DISPENSE);
        if (!res)
        {
            res = node_extra_operate(plugid, NODE_OP_DISPENSE);
        }
        update_plug_shortage_power(plugid);
        if (guard > NODE_MAX)
        {
            return res;
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
        PAU_Vector *allocatedNodes_copy = pau_vector_clone(pplug->allocatedNodes);
        if (NULL == allocatedNodes_copy)
        {
            return false;
        }
        PAU_VECTOR_FOREACH(allocated_nodeid, allocatedNodes_copy)
        {
            pull_NodefromPlug(allocated_nodeid, plugid);
        }

        pau_vector_clear(pplug->allocatedNodes);
        pau_vector_destroy(allocatedNodes_copy);
        transferPower(plugid, idlenodes_encircle_donatio);
        transferPower(plugid, idlenodes_semimatrix_donatio);
        return true;
    }
    pplug->requiredPower = requiredPower;
    update_plug_shortage_power(plugid);
    if (0 < pplug->shortage)
    {
        return true;
    }
    pau_printf("[TACTIC] releasePower plugid:%d requiredpwr:%d shortage:%d\r\n", plugid, requiredPower, pplug->shortage);
    bool res = true;
    int loop_guard = 0;
    int reserve = get_plug_allocated_cnt(plugid);
    while (0 > pplug->shortage)
    {
        // 按“距离直连节点最远优先”释放节点（线环 + 半矩阵统一比较），
        // 距离判据失效时才回退到矩阵节点专用释放逻辑
        res = node_common_operate(plugid, NODE_OP_RELEASE);
        if (!res)
        {
            res = node_extra_operate(plugid, NODE_OP_RELEASE);
        }

        if (!res)
        {
            return false;
        }
        if (loop_guard++ > reserve)
        {
            break;
        }
        update_plug_shortage_power(plugid);
    }
    transferPower(plugid, idlenodes_encircle_donatio);
    transferPower(plugid, idlenodes_semimatrix_donatio);
    return res;
}

ID_TYPE restrictPower(size_t limitedPower)
{
    LIMITEDPWR = limitedPower;
    if (OUTPUTPWR < limitedPower)
    {
        return ID_VAIN;
    }
    // 找到充电中的各plug中占有模块数最多的plug
    ID_TYPE plugid_most = ID_VAIN;
    size_t plug_most_cnt = 0;
    for (ID_TYPE plugid = 1; plugid <= PLUG_MAX; plugid++)
    {
        if (PLUG_IDLE == get_plug_state(plugid))
        {
            continue;
        }
        if (1 == get_plug_chargingnodes_cnt(plugid))
        {
            continue;
        }
        size_t module_cnt = get_plug_charging_modules_cnt(plugid);
        if (module_cnt > plug_most_cnt)
        {
            plug_most_cnt = module_cnt;
            plugid_most = plugid;
        }
    }
    if (ID_VAIN == plugid_most || 0 == plug_most_cnt)
    {
        return ID_VAIN;
    }

    int releasepwr = (plug_most_cnt - 1) * UNITPWR_MAX - 1 - SIZING_TOLERANCE;
    pau_printf("[TACTIC] restrictPower plugid:%d limitedpwr:%d current charging modules:%d\r\n", plugid_most, limitedPower, plug_most_cnt);
    releasePower(plugid_most, releasepwr);
    update_plug_shortage_power(plugid_most);
    return plugid_most;
}
static inline int flowmap_cmp(const FlowMap *a, const FlowMap *b)
{
    return (a->hops > b->hops) - (a->hops < b->hops);
}

// hops排序函数
void sort_flowmap_by_hops(FlowMap *map, size_t n)
{
    if (NULL == map || 0 == n)
    {
        return;
    }
    for (size_t i = 0; i < n - 1; i++)
    {
        size_t min_idx = i;
        for (size_t j = i + 1; j < n; j++)
        {
            if (flowmap_cmp(&map[j], &map[min_idx]) < 0)
            {
                min_idx = j;
            }
        }
        if (min_idx != i)
        {
            FlowMap temp = map[i];
            map[i] = map[min_idx];
            map[min_idx] = temp;
        }
    }
}
/*
 * 对径接触器成对出现（直径两端各一个），返回接触器 contactorid 的镜像接触器编号。
 */
static ID_TYPE diagonal_mirror(ID_TYPE contactorid)
{
    return (contactorid <= 3 * NODES_MAX_ENCIRCLE / 2)
               ? contactorid + NODES_MAX_ENCIRCLE / 2
               : contactorid - NODES_MAX_ENCIRCLE / 2;
}

/*
 * 解析连接 node 与其父节点 parent 的潮流接触器。
 * 返回主接触器编号；若该条潮流路径还涉及第二个接触器（对径分段或对径镜像），
 * 通过 secondary 输出其编号，否则置 ID_VAIN。
 * 仅当主接触器（及必要的副接触器）均已闭合时返回有效值，否则返回 ID_VAIN。
 */
static ID_TYPE resolve_edge_contactors(ID_TYPE node, ID_TYPE parent, ID_TYPE *secondary)
{
    if (NULL != secondary)
    {
        *secondary = ID_VAIN;
    }
    ID_TYPE appendix = ID_VAIN;
    struct Alloc_contactorObj *pc = find_contactor_bynode(node, parent, &appendix);
    if (NULL == pc || !pc->isClosed)
    {
        return ID_VAIN;
    }

    ID_TYPE sec = ID_VAIN;
    if (ASSERT_CONTACTOR_ID(appendix))
    {
        /* 矩阵-线环边：appendix 即线环节点一侧的对径分段接触器编号 */
        sec = appendix;
    }
    else if (pc->id > NODES_MAX_ENCIRCLE && pc->id <= 2 * NODES_MAX_ENCIRCLE)
    {
        /* 线环-线环对径边：对径接触器成对闭合，副接触器为镜像 */
        sec = diagonal_mirror(pc->id);
    }

    if (ASSERT_CONTACTOR_ID(sec) && !refer_Contactor_Extracted(sec)->isClosed)
    {
        return ID_VAIN;
    }
    if (NULL != secondary)
    {
        *secondary = sec;
    }
    return pc->id;
}

/*
 * @brief 生成单个充电桩的功率潮流方向图（统一图，含线环与半矩阵节点）
 *
 * 以直连节点为根，沿已闭合的接触器在统一图上做 BFS，对每个已分配节点求出：
 *   - direction：节点编号；
 *   - contactorid：该节点潮流指向直连节点所经的主接触器编号；
 *   - appendix：若该路径含第二个接触器（对径分段/对径镜像），为其编号，否则 ID_VAIN；
 *   - hops：该节点到直连节点的距离；
 *   - genogram：功率潮流经该节点流向的下一个节点编号（父节点）。
 *
 * 结果按 hops 从小到大排序（父节点必在子节点之前），伪环路节点以 hops=100 排在末尾。
 *
 * @param plugid 充电桩编号
 * @param pobject 输出缓冲（长度至少 MAXNODES_MEM_LMT）
 * @return 指向输出末尾之后的指针；失败返回 NULL
 */
FlowMap *flow_directioned(ID_TYPE plugid, FlowMap *pobject)
{
    if (!ASSERT_PLUG_ID(plugid) || NULL == pobject)
    {
        return NULL;
    }
    struct Alloc_plugObj *pplug = refer_Plug_Extracted(plugid);
    ID_TYPE root = pplug->connectedNode;

    /* 沿已闭合接触器 BFS，得到各节点到直连节点的距离 */
    hops_refresh(root, plugid);

    size_t index = 0;

    /* 根节点（直连节点） */
    pobject[index].direction = root;
    pobject[index].contactorid = 254; /* 虚拟根接触器 */
    pobject[index].appendix = 0;
    pobject[index].hops = 0;
    pobject[index].genogram = root;
    index++;

    /* 已分配的普通节点：找到父节点并解析接触器 */
    PAU_VECTOR_FOREACH(nodeid, pplug->allocatedNodes)
    {
        if (nodeid == root || is_node_pseudocycledon(nodeid))
        {
            continue;
        }
        int hops = get_dist(nodeid);
        if (hops <= 0)
        {
            continue; /* 不可达 */
        }

        ID_TYPE neighbors[MAX_NODE_NEIGHBORS] = {0};
        get_neighbors(nodeid, neighbors);
        for (int i = 0; i < MAX_NODE_NEIGHBORS; i++)
        {
            ID_TYPE nbr = neighbors[i];
            if (!ASSERT_NODE_ID(nbr) || is_node_pseudocycledon(nbr))
            {
                continue;
            }
            if (!pau_vector_contains(pplug->allocatedNodes, nbr))
            {
                continue;
            }
            if (get_dist(nbr) != hops - 1)
            {
                continue;
            }
            ID_TYPE secondary = ID_VAIN;
            ID_TYPE contactorid = resolve_edge_contactors(nodeid, nbr, &secondary);
            if (!ASSERT_CONTACTOR_ID(contactorid))
            {
                continue;
            }
            if (index >= MAXNODES_MEM_LMT)
            {
                break;
            }
            pobject[index].direction = nodeid;
            pobject[index].contactorid = contactorid;
            pobject[index].appendix = secondary;
            pobject[index].hops = (ID_TYPE)hops;
            pobject[index].genogram = nbr;
            index++;
            break;
        }
    }

    /* 伪环路节点：hops=100 排到最后，作为备用接入点 */
    PAU_VECTOR_FOREACH(nodeid, pplug->allocatedNodes)
    {
        if (!is_node_pseudocycledon(nodeid))
        {
            continue;
        }
        if (index >= MAXNODES_MEM_LMT)
        {
            break;
        }
        pobject[index].direction = nodeid;
        pobject[index].contactorid = nodeid + NODES_MAX_ENCIRCLE;
        pobject[index].appendix = CONTACTOR_MAX - NODES_MAX_ENCIRCLE / 2 + (nodeid > NODES_MAX_ENCIRCLE / 2 ? nodeid - NODES_MAX_ENCIRCLE / 2 : nodeid);
        pobject[index].hops = 100;
        struct Alloc_contactorObj *pc = refer_Contactor_Extracted(pobject[index].appendix);
        pobject[index].genogram = pc->node1;
        index++;
    }

    /* 按 hops 从小到大排序 */
    sort_flowmap_by_hops(pobject, index);

    return pobject + index;
}
enum METABOLIN metabole_alethes(unsigned char nodeid, unsigned char relayid, FlowMap *pflow_map)
{
    for (int m = 0; m < MAXNODES_MEM_LMT; m++)
    {
        if (ID_VAIN == pflow_map[m].direction)
        {
            return METABOLIN_VANISH;
        }
        if (pflow_map[m].direction != nodeid)
        {
            continue;
        }
        return pflow_map[m].contactorid == relayid ? METABOLIN_INTACT : METABOLIN_CHANGE;
    }
    return METABOLIN_VANISH;
}
bool set_node_availability(ID_TYPE node_id)
{
    if (!ASSERT_NODE_ID(node_id) && SemiHybrid == TOPOLOGY_TYPE)
    {
        return false;
    }
    if (!ASSERT_NODE_ID_ENCIRCLE(node_id) && CakraWheel == TOPOLOGY_TYPE)
    {
        return false;
    }
    struct Alloc_nodeObj *pnode = refer_Node_Extracted(node_id);
    bool res = false;
    if (NODE_DISABLED == pnode->state)
    {
        pnode->state = NODE_IDLEFREE;
        res = true;
    }
    else if (NODE_IDLEFREE == pnode->state)
    {
        pnode->state = NODE_DISABLED;
        res = false;
    }
    else if (NODE_OUTORDER == pnode->state)
    {
        pnode->state = NODE_OCCUPIED;
        res = true;
    }
    else if (NODE_OCCUPIED == pnode->state)
    {
        pnode->state = NODE_OUTORDER;
        if (ASSERT_PLUG_ID(pnode->plug_id))
        {
            int requiredPower = refer_Plug_Extracted(pnode->plug_id)->requiredPower;
            (void)requestPower(pnode->plug_id, requiredPower);
        }
        res = false;
    }
    return res;
}

void recover_node_pseudocycledon(ID_TYPE plugid, ID_TYPE nodeid)
{
    if (!ASSERT_NODE_ID(nodeid) || !ASSERT_PLUG_ID(plugid))
    {
        return;
    }
    refer_Node_Extracted(nodeid)->pseudocycledon = false;
    set_locked(plugid, nodeid);
}
