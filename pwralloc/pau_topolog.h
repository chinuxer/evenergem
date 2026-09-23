#ifndef PAUTOPOLOG_H
#define PAUTOPOLOG_H

#include "pau_broker.h"
#include "pau_vector.h"

#define hops_refresh(start, plug) (void)get_hops_occupied(start, 0, plug)
/*
 * 统一图上单个节点的最大邻接点数。
 * 线环节点最多 4 个（右、左、对径、对应矩阵节点）；
 * 矩阵节点最多 = 2 个下端点线环节点 + (矩阵节点数-1) 个母线邻接点 = 矩阵节点数 + 1。
 * 任意节点的邻接数都不可能超过节点总数，故以节点总数上限 MAXNODES_MEM_LMT 兜底，
 * 避免矩阵母线全连接时邻接点被截断。
 */
#define MAX_NODE_NEIGHBORS MAXNODES_MEM_LMT

/*
 * 统一图（线环 + 半矩阵）的边数上限，用于前向星邻接表与候选边缓冲的容量。
 *
 * 设 N = MAXNODES_MEM_LMT（节点总数上限），R = 线环节点数，H = 矩阵节点数：
 *   无向边 = 3R（线环左/右/对径） + 2H（矩阵下端点） + H(H-1)/2（矩阵母线全连接）；
 *   有向边 = 无向边的 2 倍。
 * SemiHybrid 下 H = R/2 且 R+H ≤ N，故 H ≤ N/3；边数关于 H 单调递增，
 * 取 H = N/3（代入 R=2H）得：
 *   有向边上界 = 6R + 4H + H(H-1) = H^2 + 15H；
 *   无向边为其一半。末尾 +8 兜底整数取整误差。
 */
#define PAU_GRAPH_H_MAX (MAXNODES_MEM_LMT / 3)
#define MAX_GRAPH_DIRECTED_EDGES ((PAU_GRAPH_H_MAX) * (PAU_GRAPH_H_MAX) + 15 * (PAU_GRAPH_H_MAX) + 8)
#define MAX_GRAPH_UNDIRECTED_EDGES (MAX_GRAPH_DIRECTED_EDGES / 2)
int get_hops_occupied(ID_TYPE start, ID_TYPE nodeid, ID_TYPE plugid);
int get_dist(ID_TYPE nodeid);
void set_dist(ID_TYPE nodeid, int value);
void set_locked(ID_TYPE plugid, ID_TYPE nodeid);
int get_locked(ID_TYPE nodeid);
void dual_endings_bfs_shell(ID_TYPE start, ID_TYPE plugid, bool find_type);
void get_neighbors(ID_TYPE nodeid, ID_TYPE *neighbors);
void add_candidate_edge(size_t *candidateCnt, ID_TYPE u, ID_TYPE v, bool isDiagonal);
void clear_parent(void);
void set_parent(ID_TYPE node, ID_TYPE parentNode);
bool graphconfig_Canaries_Twittering(void);
typedef struct Edge
{
    int u, v;
    bool diagonal;
} Contactor_Edge;
Contactor_Edge get_Edge(int index);
int find(int x);
void unite(int x, int y);

#endif // PAUTOPOLOG_H
