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
 *************************************************************************************************************************************************************************/
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
    int nxt[6 * MAXNODES_MEM_LMT + 1];
    int to[6 * MAXNODES_MEM_LMT + 1];
    int dist[MAXNODES_MEM_LMT + 1];    /* 对当前起点，dist[i] 为到 i 的最短距离 */
    int q[MAXNODES_MEM_LMT];           /* 手写队列，比 STL 快 */
    char locked[MAXNODES_MEM_LMT + 1]; /* >0 表示被锁，节点编号 1..n */

    int qh;
    int qt;
    int nodeCount;
    int plugCount;
    int tot; /* 邻接表，最多 3n 条边 */
    Contactor_Edge candidates[MAXNODES_MEM_LMT * 3];
    int parent[MAXNODES_MEM_LMT + 1];
    size_t rear_canary;
} *pconfig_graph IN_PAU_RAM_SECTION = NULL;

static inline void add_edge(int u, int v)
{
    pconfig_graph->tot += 1;
    pconfig_graph->nxt[pconfig_graph->tot] = pconfig_graph->head[u];
    pconfig_graph->head[u] = pconfig_graph->tot;
    pconfig_graph->to[pconfig_graph->tot] = v;
}

/* 建图：每个节点连 左、右、对径 */
void build_graph(void)
{
    pconfig_graph->tot = 0;
    int n = pconfig_graph->nodeCount;
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
}

/* 从 start 出发跑一次 BFS，只计算到 locked==plugid 的点的距离 */
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
    if (nodeid > pconfig_graph->nodeCount || nodeid < 1)
    {
        return -1;
    }
    return pconfig_graph->dist[nodeid];
}
void set_dist(ID_TYPE nodeid, int value)
{
    if (nodeid > pconfig_graph->nodeCount || nodeid < 1)
    {
        return;
    }
    pconfig_graph->dist[nodeid] = value;
}
void set_locked(ID_TYPE plugid, ID_TYPE nodeid)
{
    if (nodeid > pconfig_graph->nodeCount || nodeid < 1)
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
    if (nodeid > pconfig_graph->nodeCount || nodeid < 1)
    {
        return -1;
    }
    return pconfig_graph->locked[nodeid];
}

void directedConfig_Init(ID_TYPE nodes, ID_TYPE plugs)
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
    // 原因：每个节点产生 6 条边，nxt[] 和 to[] 数组大小为 6*MAXNODES_MEM_LMT+1
    if (nodes > MAXNODES_MEM_LMT)
    {
        pau_printf("ERROR: Requested nodes (%u) exceeds MAXNODES_MEM_LMT (%u)\n",
                   nodes, MAXNODES_MEM_LMT);
        return;
    }

    pconfig_graph = (typeof(pconfig_graph))pau_calloc(sizeof(*pconfig_graph), __func__);
    pau_printf("PAU_DIRECTED_CONFIG_INIT: %x\n", sizeof(*pconfig_graph));
    pconfig_graph->nodeCount = nodes;
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
        for (int i = 1; i <= pconfig_graph->nodeCount; i++)
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
    while (pconfig_graph->parent[root] != root)
    {
        root = pconfig_graph->parent[root];
    }
    // 路径压缩（所有节点直接指向根）
    while (pconfig_graph->parent[x] != root)
    {
        int next = pconfig_graph->parent[x];
        pconfig_graph->parent[x] = root;
        x = next;
    }
    return root;
}

void unite(int x, int y)
{
    pconfig_graph->parent[find(x)] = find(y);
}

void add_candidate_edge(size_t *candidateCnt, ID_TYPE u, ID_TYPE v, bool isDiagonal)
{
    if (*candidateCnt >= (MAXNODES_MEM_LMT * 3)) // 检查是否超过候选边数组的容量
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
    if (node > pconfig_graph->nodeCount || node < 1)
    {
        return;
    }
    if (parentNode > pconfig_graph->nodeCount || parentNode < 1)
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

void dist_print(void)
{
    // 打印pconfig_graph->dist[]

    pau_printf("[%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d]", pconfig_graph->dist[0], pconfig_graph->dist[1], pconfig_graph->dist[2], pconfig_graph->dist[3], pconfig_graph->dist[4], pconfig_graph->dist[5], pconfig_graph->dist[6], pconfig_graph->dist[7], pconfig_graph->dist[8], pconfig_graph->dist[9], pconfig_graph->dist[10], pconfig_graph->dist[11], pconfig_graph->dist[12], pconfig_graph->dist[13], pconfig_graph->dist[14], pconfig_graph->dist[15], pconfig_graph->dist[16], pconfig_graph->dist[17], pconfig_graph->dist[18], pconfig_graph->dist[19], pconfig_graph->dist[20]);
}
void lock_print(void)
{
    // 打印pconfig_graph->locked[]

    pau_printf("[%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d]", pconfig_graph->locked[0], pconfig_graph->locked[1], pconfig_graph->locked[2], pconfig_graph->locked[3], pconfig_graph->locked[4], pconfig_graph->locked[5], pconfig_graph->locked[6], pconfig_graph->locked[7], pconfig_graph->locked[8], pconfig_graph->locked[9], pconfig_graph->locked[10], pconfig_graph->locked[11], pconfig_graph->locked[12], pconfig_graph->locked[13], pconfig_graph->locked[14], pconfig_graph->locked[15], pconfig_graph->locked[16], pconfig_graph->locked[17], pconfig_graph->locked[18], pconfig_graph->locked[19], pconfig_graph->locked[20]);
}
