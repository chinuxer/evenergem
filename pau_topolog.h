#ifndef PAUTOPOLOG_H
#define PAUTOPOLOG_H

#include "pau_broker.h"
#include "pau_vector.h"

#define hops_refresh(start,plug) (void)get_hops_occupied(start,0,plug)
int get_hops_occupied(ID_TYPE start, ID_TYPE nodeid, ID_TYPE plugid);
int get_dist(ID_TYPE nodeid);
void set_dist(ID_TYPE nodeid, int value);
void set_locked(ID_TYPE plugid, ID_TYPE nodeid);
int get_locked(ID_TYPE nodeid);
void dual_endings_bfs_shell(ID_TYPE start, ID_TYPE plugid, bool find_type);
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

#endif
