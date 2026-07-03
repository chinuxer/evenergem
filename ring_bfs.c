#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- 接口：按需改 ---------- */
#define MAXN 36          /* 最大节点数，可再开大 */
int  n;                      /* 真实节点数（偶数） */
char locked[MAXN+1];         /* 1 表示被锁，节点编号 1..n */
/* ---------- 全局变量 ---------- */
int  head[MAXN+1], nxt[6*MAXN+1], to[6*MAXN+1], tot; /* 邻接表，最多 3n 条边 */
int  dist[MAXN+1];           /* 对当前起点，dist[i] 为到 i 的最短距离 */
int  q[MAXN], qh, qt;        /* 手写队列，比 STL 快 */

static inline void add_edge(int u, int v){
    ++tot; nxt[tot]=head[u]; head[u]=tot; to[tot]=v;
}
/* 建图：每个节点连 左、右、对径 */
void build_graph(void){
    tot=0; memset(head,0,sizeof(head));
    for(int u=1; u<=n; ++u){
        int v1 = (u==1 ? n : u-1);        /* 左邻居 */
        int v2 = (u==n ? 1 : u+1);        /* 右邻居 */
        int v3 = (u>n/2 ? u-n/2 : u+n/2); /* 对径 */
        add_edge(u,v1); add_edge(v1,u);
        add_edge(u,v2); add_edge(v2,u);
        add_edge(u,v3); add_edge(v3,u);
    }
}
/* 从 start 出发跑一次 BFS，结果写进全局 dist[] */
void bfs(int start){
    memset(dist, -1, sizeof(dist));
    qh=qt=0;
    dist[start]=0;
    q[qt++]=start;
    while(qh<qt){
        int u=q[qh++];
        for(int e=head[u]; e; e=nxt[e]){
            int v=to[e];
            if(locked[v]) continue;   /* 锁节点直接跳过 */
            if(dist[v]==-1){
                dist[v]=dist[u]+1;
                q[qt++]=v;
            }
        }
    }
}
/* ---------- 主程序：演示 ---------- */
int main(){
    /* 1. 读入 */
    printf("Input n (even): ");
    scanf("%d",&n);
    if(n&1){ puts("n must be even"); return 0; }
    printf("Input locked mask (0/1) for node 1..n: ");
    for(int i=1;i<=n;++i) scanf("%hhd",&locked[i]);

    /* 2. 建图 */
    build_graph();

    /* 3. 对每个起点跑 BFS 并输出 */
    for(int s=1;s<=n;++s){
        if(locked[s]){                 /* 起点自己被锁，到谁都 -1 */
            for(int t=1;t<=n;++t) printf("-1 ");
            puts("");
            continue;
        }
        bfs(s);
        for(int t=1;t<=n;++t) printf("%d ",dist[t]);
        puts("");
    }
    return 0;
}
