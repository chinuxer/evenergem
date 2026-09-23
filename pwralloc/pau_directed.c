/**,>>,,,
 ******************************************************************************
 * Copyright(c) Infy Power 2026-2026
 * @file    pau_directed.c
 * @author  YBA40320
 * @version V1.0
 * @date    2026-04-27
 * @brief   线环节点抽象成有向图,实现基于广度优先搜索的资源分配算法
 * @note    !如无十足把握,非必要不修改本文件
 * @history 2026-04-27 YBA40320 创建;2026-05-19 YBA40320 从模拟机移植到A2605线环1500kW工程
 * @details
 *
 *****************************************************************************/
#include "pau_vector.h"
#include "pau_broker.h"
#include "pau_topolog.h"
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct
{
    size_t front_canary;
    int head[MAXNODES_MEM_LMT + 1];
    int nxt[MAX_GRAPH_DIRECTED_EDGES];
    int to[MAX_GRAPH_DIRECTED_EDGES];
    int dist[MAXNODES_MEM_LMT + 1];    /* 对当前起点，dist[i] 为到 i 的最短距离 */
    int q[MAXNODES_MEM_LMT];           /* 手写队列，比 STL 快 */
    char locked[MAXNODES_MEM_LMT + 1]; /* >0 表示被锁，节点编号 1..n */

    int qh;
    int qt;
    int nodeCount;   /* 线环节点数 R */
    int matrixCount; /* 半矩阵扩展节点数 H=R/2, 非半矩阵为 0 */
    int totalCount;  /* 节点总数 R+H */
    int plugCount;
    int tot; /* 邻接表已用边数（前向星，有向边） */
    Contactor_Edge candidates[MAX_GRAPH_UNDIRECTED_EDGES];
    int parent[MAXNODES_MEM_LMT + 1];
    size_t rear_canary;
} *pconfig_graph IN_PAU_RAM_SECTION = NULL;

static inline void add_edge(int u, int v)
{
    if (pconfig_graph->tot >= MAX_GRAPH_DIRECTED_EDGES)
    {
        pau_printf("ERROR: graph edge table overflow (%d >= %d)\r\n",
                   pconfig_graph->tot, MAX_GRAPH_DIRECTED_EDGES);
        return;
    }
    pconfig_graph->tot += 1;
    pconfig_graph->nxt[pconfig_graph->tot] = pconfig_graph->head[u];
    pconfig_graph->head[u] = pconfig_graph->tot;
    pconfig_graph->to[pconfig_graph->tot] = v;
}

/* 建图：先对线环建图（左、右、对径），再对半矩阵节点扩容建图
 *
 * 半矩阵扩展规则（R 线环节点 + H=R/2 矩阵节点）：
 *   矩阵节点 R+k 与线环节点 k 和 k+H 相连（对径开关 + 4XX 接触器）
 *   所有矩阵节点两两相连（3XX 接触器构成的全连接矩阵母线）
 */
void build_graph(void)
{
    pconfig_graph->tot = 0;
    int n = pconfig_graph->nodeCount;
    int h = pconfig_graph->matrixCount;
    int t = pconfig_graph->totalCount;
    memset(pconfig_graph->head, 0, sizeof(pconfig_graph->head));
    for (int u = 1; u <= n; ++u)
    {
        int v1 = (u == 1 ? n : u - 1);                /* 左邻居 */
        int v2 = (u == n ? 1 : u + 1);                /* 右邻居 */
        int v3 = (u > n / 2 ? u - n / 2 : u + n / 2); /* 对径 */
        add_edge(u, v1);
        add_edge(v1, u);
        add_edge(u, v2);
        add_edge(v2, u);
        add_edge(u, v3);
        add_edge(v3, u);
    }
    /* 扩容建图：半矩阵节点 */
    for (int k = 1; k <= h; ++k)
    {
        int m = n + k;         /* 矩阵节点编号 R+k */
        int alpha = k;         /* 对径开关下端点 k */
        int beta = k + h;      /* 对径开关下端点 k+H */
        add_edge(m, alpha);
        add_edge(alpha, m);
        add_edge(m, beta);
        add_edge(beta, m);
    }
    for (int a = n + 1; a <= t; ++a)
    {
        for (int b = a + 1; b <= t; ++b)
        {
            add_edge(a, b);
            add_edge(b, a);
        }
    }
}

static bool is_edge_available(int u, int v)
{
    // 遍历所有接触器（线环、对径、半矩阵母线、半矩阵-线环）
    for (int c = 1; c <= CONTACTOR_MAX; ++c)
    {
        struct Alloc_contactorObj *pcont = refer_Contactor_Extracted(c);
        if (!pcont || !pcont->isClosed)
        {
            continue;
        }
        if (pcont->node2 <= NODE_MAX)
        {
            if ((pcont->node1 == u && pcont->node2 == v) ||
                (pcont->node1 == v && pcont->node2 == u))
            {
                return true; // 存在闭合接触器
            }
        }
        else if (pcont->node2 > CONTACTOR_SPLICE_MULTIPLE)
        {
            // 4XX 接触器：node1 为矩阵节点，node2 编码两个对径线环节点
            ID_TYPE nodeid_alpha = pcont->node2 / CONTACTOR_SPLICE_MULTIPLE;
            ID_TYPE nodeid_beta = pcont->node2 % CONTACTOR_SPLICE_MULTIPLE;
            ID_TYPE ring = ID_VAIN;
            if (pcont->node1 == u && (nodeid_alpha == v || nodeid_beta == v))
            {
                ring = v;
            }
            else if (pcont->node1 == v && (nodeid_alpha == u || nodeid_beta == u))
            {
                ring = u;
            }
            if (ID_VAIN == ring)
            {
                continue;
            }
            // 还需对应的对径分段接触器闭合，矩阵节点才真正连到该线环节点
            struct Alloc_contactorObj *pseg = refer_Contactor_Extracted(NODES_MAX_ENCIRCLE + ring);
            if (pseg && pseg->isClosed)
            {
                return true;
            }
        }
    }
    return false; // 无可用路径
}
void bfs(ID_TYPE start, ID_TYPE plugid, bool find_type)
{
    memset(pconfig_graph->dist, -1, sizeof(pconfig_graph->dist));
    pconfig_graph->qh = pconfig_graph->qt = 0;
    pconfig_graph->dist[start] = 0;
    pconfig_graph->q[pconfig_graph->qt++] = start;

    while (pconfig_graph->qh < pconfig_graph->qt)
    {
        int u = pconfig_graph->q[pconfig_graph->qh++];
        for (int e = pconfig_graph->head[u]; e; e = pconfig_graph->nxt[e])
        {
            int v = pconfig_graph->to[e];
            if (find_type && !is_edge_available(u, v))
            {
                continue;
            }
            if ((!find_type && pconfig_graph->locked[v] > 0 && pconfig_graph->locked[v] != plugid) || (find_type && pconfig_graph->locked[v] != plugid))
                continue;

            if (pconfig_graph->dist[v] == -1)
            {
                pconfig_graph->dist[v] = pconfig_graph->dist[u] + 1;
                pconfig_graph->q[pconfig_graph->qt++] = v;
            }
        }
    }
}

// 供外部调用的接口 将config.dist和config.locked的访问封装在接口内
int get_hops_occupied(ID_TYPE start, ID_TYPE nodeid, ID_TYPE plugid)
{
    if (!ASSERT_NODE_ID_ENCIRCLE(start) || !ASSERT_PLUG_ID(plugid))
    {
        return -1;
    }
    if (ID_VAIN == nodeid)
    {
        bfs(start, plugid, true);
    }
    return pconfig_graph->dist[nodeid];
}
int get_dist(ID_TYPE nodeid)
{
    if (nodeid > pconfig_graph->totalCount || nodeid < 1)
    {
        return -1;
    }
    return pconfig_graph->dist[nodeid];
}
void set_dist(ID_TYPE nodeid, int value)
{
    if (nodeid > pconfig_graph->totalCount || nodeid < 1)
    {
        return;
    }
    pconfig_graph->dist[nodeid] = value;
}
void set_locked(ID_TYPE plugid, ID_TYPE nodeid)
{
    if (nodeid > pconfig_graph->totalCount || nodeid < 1)
    {
        return;
    }
    if (plugid > pconfig_graph->plugCount)
    {
        return;
    }
    pconfig_graph->locked[nodeid] = plugid; // 标记为已分配（锁定）
}
int get_locked(ID_TYPE nodeid)
{
    if (nodeid > pconfig_graph->totalCount || nodeid < 1)
    {
        return -1;
    }
    return pconfig_graph->locked[nodeid];
}

void directedConfig_Init(ID_TYPE nodes, ID_TYPE plugs, ID_TYPE matrix_nodes)
{
    if ((nodes & 1) > 0)
    {
        return;
    }
    if (plugs > nodes)
    {
        return;
    }
    // 防御性检查：节点数不能超过 MAXNODES_MEM_LMT
    if (nodes > MAXNODES_MEM_LMT || nodes + matrix_nodes > MAXNODES_MEM_LMT)
    {
        pau_printf("ERROR: Requested nodes (%u) exceeds MAXNODES_MEM_LMT (%u)\n",
                   nodes, MAXNODES_MEM_LMT);
        return;
    }

    pconfig_graph = (typeof(pconfig_graph))pau_calloc(sizeof(*pconfig_graph), __func__);
    if (NULL == pconfig_graph)
    {
        pau_printf("ERROR: Memory allocation failed for pconfig_graph\n");
        return;
    }
    pau_printf("PAU_DIRECTED_CONFIG_INIT: %x\n", sizeof(*pconfig_graph));
    pconfig_graph->nodeCount = nodes;
    pconfig_graph->matrixCount = matrix_nodes;
    pconfig_graph->totalCount = nodes + matrix_nodes;
    pconfig_graph->plugCount = plugs;
    pconfig_graph->front_canary = FRONT_MAGICWORD;
    pconfig_graph->rear_canary = REAR_MAGICWORD;
    build_graph();
}

/**
 * 远/近双势广度优先搜索查找
 */
void dual_endings_bfs_shell(ID_TYPE start, ID_TYPE plugid, bool find_type)
{
    if (pconfig_graph->locked[start] != 0 && pconfig_graph->locked[start] != plugid)
    {
        for (int i = 1; i <= pconfig_graph->totalCount; i++)
        {
            pconfig_graph->dist[i] = -1;
        }
        return;
    }
    bfs(start, plugid, find_type);
}

// 查找根节点 + 路径压缩
int find(int x)
{
    int root = x;
    // 找根
    int loop_guardian = MAXNODES_MEM_LMT;
    while (pconfig_graph->parent[root] != root)
    {
        root = pconfig_graph->parent[root];
        if (loop_guardian-- <= 0)
        {
            pau_printf("ERROR: Loop detected in union-find structure\r\n");
            break;
        }
    }
    // 路径压缩（所有节点直接指向根）
    loop_guardian = MAXNODES_MEM_LMT;
    while (pconfig_graph->parent[x] != root)
    {
        int next = pconfig_graph->parent[x];
        pconfig_graph->parent[x] = root;
        x = next;
        if (loop_guardian-- <= 0)
        {
            pau_printf("ERROR: Loop detected in union-find structure\r\n");
            break;
        }
    }
    return root;
}

void unite(int x, int y)
{
    pconfig_graph->parent[find(x)] = find(y);
}

void add_candidate_edge(size_t *candidateCnt, ID_TYPE u, ID_TYPE v, bool isDiagonal)
{
    if (*candidateCnt >= MAX_GRAPH_UNDIRECTED_EDGES) // 检查是否超过候选边数组的容量
    {
        // Optional: Handle error or log warning if buffer is full
        return;
    }

    pconfig_graph->candidates[*candidateCnt].u = u;
    pconfig_graph->candidates[*candidateCnt].v = v;
    pconfig_graph->candidates[*candidateCnt].diagonal = isDiagonal;
    (*candidateCnt)++;
}

void clear_parent(void)
{
    memset(pconfig_graph->parent, 0, sizeof(pconfig_graph->parent));
}

void set_parent(ID_TYPE node, ID_TYPE parentNode)
{
    if (node > pconfig_graph->totalCount || node < 1)
    {
        return;
    }
    if (parentNode > pconfig_graph->totalCount || parentNode < 1)
    {
        return;
    }
    pconfig_graph->parent[node] = parentNode;
}

Contactor_Edge get_Edge(int index)
{
    return pconfig_graph->candidates[index];
}
bool graphconfig_Canaries_Twittering(void)
{
    return (pconfig_graph->front_canary == FRONT_MAGICWORD && pconfig_graph->rear_canary == REAR_MAGICWORD);
}
