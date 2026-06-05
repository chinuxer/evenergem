/**
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
} config IN_PAU_RAM_SECTION = {0};

static inline void add_edge(int u, int v)
{
    config.tot += 1;
    config.nxt[config.tot] = config.head[u];
    config.head[u] = config.tot;
    config.to[config.tot] = v;
}

/* 建图：每个节点连 左、右、对径 */
void build_graph(void)
{
    config.tot = 0;
    int n = config.nodeCount;
    memset(config.head, 0, sizeof(config.head));
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
    memset(config.dist, -1, sizeof(config.dist));
    config.qh = config.qt = 0;
    config.dist[start] = 0;
    config.q[config.qt++] = start;
    while (config.qh < config.qt)
    {
        int u = config.q[config.qh++];
        for (int e = config.head[u]; e; e = config.nxt[e])
        {
            int v = config.to[e];
            if ((!find_type && config.locked[v] > 0 && config.locked[v] != plugid) || (find_type && config.locked[v] != plugid))
                continue;
            if (config.dist[v] == -1)
            {
                config.dist[v] = config.dist[u] + 1;
                config.q[config.qt++] = v;
            }
        }
    }
}

// 供外部调用的接口 将config.dist和config.locked的访问封装在接口内
int get_hops_occupied(ID_TYPE start, ID_TYPE nodeid, ID_TYPE plugid)
{
    if (!ASSERT_NODE_ID(start) || !ASSERT_PLUG_ID(plugid))
    {
        return -1;
    }
    if (ID_VAIN == nodeid)
    {
        bfs(start, plugid, true);
    }
    return config.dist[nodeid];
}
int get_dist(ID_TYPE nodeid)
{
    if (nodeid > config.nodeCount || nodeid < 1)
    {
        return -1;
    }
    return config.dist[nodeid];
}
void set_dist(ID_TYPE nodeid, int value)
{
    if (nodeid > config.nodeCount || nodeid < 1)
    {
        return;
    }
    config.dist[nodeid] = value;
}
void set_locked(ID_TYPE plugid, ID_TYPE nodeid)
{
    if (nodeid > config.nodeCount || nodeid < 1)
    {
        return;
    }
    if (plugid > config.plugCount || plugid < 1)
    {
        return;
    }
    config.locked[nodeid] = plugid; // 标记为已分配（锁定）
}
int get_locked(ID_TYPE nodeid)
{
    if (nodeid > config.nodeCount || nodeid < 1)
    {
        return -1;
    }
    return config.locked[nodeid];
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
    memset(&config, 0, sizeof(config));
    pau_printf("PAU_DIRECTED_CONFIG_INIT: %x\n", sizeof(config));
    config.nodeCount = nodes;
    config.plugCount = plugs;
    config.front_canary = FRONT_MAGICWORD;
    config.rear_canary = REAR_MAGICWORD;
    build_graph();
}

/**
 * 远/近双势广度优先搜索查找
 */
void dual_endings_bfs_shell(ID_TYPE start, ID_TYPE plugid, bool find_type)
{
    if (config.locked[start] != 0 && config.locked[start] != plugid)
    {
        for (int i = 1; i <= config.nodeCount; i++)
        {
            config.dist[i] = -1;
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
    while (config.parent[root] != root)
    {
        root = config.parent[root];
    }
    // 路径压缩（所有节点直接指向根）
    while (config.parent[x] != root)
    {
        int next = config.parent[x];
        config.parent[x] = root;
        x = next;
    }
    return root;
}

void unite(int x, int y)
{
    config.parent[find(x)] = find(y);
}

void add_candidate_edge(size_t *candidateCnt, ID_TYPE u, ID_TYPE v, bool isDiagonal)
{
    if (*candidateCnt >= (MAXNODES_MEM_LMT * 3)) // 检查是否超过候选边数组的容量
    {
        // Optional: Handle error or log warning if buffer is full
        return;
    }

    config.candidates[*candidateCnt].u = u;
    config.candidates[*candidateCnt].v = v;
    config.candidates[*candidateCnt].diagonal = isDiagonal;
    (*candidateCnt)++;
}

void clear_parent(void)
{
    memset(config.parent, 0, sizeof(config.parent));
}

void set_parent(ID_TYPE node, ID_TYPE parentNode)
{
    if (node > config.nodeCount || node < 1)
    {
        return;
    }
    if (parentNode > config.nodeCount || parentNode < 1)
    {
        return;
    }
    config.parent[node] = parentNode;
}

Contactor_Edge get_Edge(int index)
{
    return config.candidates[index];
}
bool graphconfig_Canaries_Twittering(void)
{
    return (config.front_canary == FRONT_MAGICWORD && config.rear_canary == REAR_MAGICWORD);
}