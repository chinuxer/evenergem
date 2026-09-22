#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXD 3

typedef struct {
    int R;
    int H;
    int T;
    int **adj;
    int *deg;
} Graph;

static int has_neighbor(const Graph *g, int a, int b)
{
    int k;
    for (k = 0; k < g->deg[a]; k++)
        if (g->adj[a][k] == b)
            return 1;
    return 0;
}

static void add_edge(Graph *g, int a, int b)
{
    if (has_neighbor(g, a, b))
        return;
    g->adj[a][g->deg[a]++] = b;
    g->adj[b][g->deg[b]++] = a;
}

static Graph *build_graph(int R)
{
    Graph *g;
    int i, k, a, b, cap;

    g = (Graph *)malloc(sizeof(Graph));
    g->R = R;
    g->H = R / 2;
    g->T = R + g->H;

    g->adj = (int **)malloc(sizeof(int *) * (size_t)g->T);
    g->deg = (int *)calloc((size_t)g->T, sizeof(int));
    for (i = 0; i < g->T; i++) {
        cap = (i + 1 <= R) ? 8 : (g->H + 2);
        g->adj[i] = (int *)malloc(sizeof(int) * (size_t)cap);
    }

    for (i = 1; i <= R; i++) {
        a = i;
        b = (i == R) ? 1 : i + 1;
        add_edge(g, a - 1, b - 1);
    }

    for (k = 1; k <= g->H; k++)
        add_edge(g, k - 1, k + g->H - 1);

    for (k = 1; k <= g->H; k++) {
        a = R + k;
        add_edge(g, a - 1, k - 1);
        add_edge(g, a - 1, k + g->H - 1);
    }

    for (a = R + 1; a <= R + g->H; a++)
        for (b = a + 1; b <= R + g->H; b++)
            add_edge(g, a - 1, b - 1);

    return g;
}

static void bfs(const Graph *g, int start, const int *locked, int nlock,
                int *dist, int *parent, int *reach, int *nreach)
{
    char *lockedFlag;
    int *queue;
    int head = 0, tail = 0;
    int i, u, k, w;

    lockedFlag = (char *)calloc((size_t)g->T, sizeof(char));
    queue = (int *)malloc(sizeof(int) * (size_t)g->T);

    for (i = 0; i < nlock; i++)
        if (locked[i] >= 1 && locked[i] <= g->T)
            lockedFlag[locked[i] - 1] = 1;

    for (i = 0; i < g->T; i++) {
        dist[i] = -1;
        parent[i] = -1;
    }

    if (!lockedFlag[start]) {
        dist[start] = 0;
        queue[tail++] = start;
    }

    while (head < tail) {
        u = queue[head++];
        for (k = 0; k < g->deg[u]; k++) {
            w = g->adj[u][k];
            if (lockedFlag[w] || dist[w] >= 0)
                continue;
            dist[w] = dist[u] + 1;
            parent[w] = u;
            queue[tail++] = w;
        }
    }

    *nreach = 0;
    for (i = 0; i < g->T; i++)
        if (dist[i] >= 0)
            reach[(*nreach)++] = i;

    free(lockedFlag);
    free(queue);
}

static void sort_nodes(int *arr, int len)
{
    int i, j, t;
    for (i = 1; i < len; i++) {
        t = arr[i];
        j = i - 1;
        while (j >= 0 && arr[j] > t) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = t;
    }
}

static void print_set(const int *sel, int count, const int *parent)
{
    int *sorted;
    int i, v;

    sorted = (int *)malloc(sizeof(int) * (size_t)count);
    memcpy(sorted, sel, sizeof(int) * (size_t)count);
    sort_nodes(sorted, count);

    printf("{");
    for (i = 0; i < count; i++) {
        printf("N%d", sorted[i] + 1);
        if (i < count - 1)
            printf(",");
    }
    printf("}\n");

    for (i = 0; i < count; i++) {
        int len = 0, j, x;
        int *path;
        v = sorted[i];
        if (parent[v] < 0)
            continue;
        x = v;
        while (x >= 0) {
            len++;
            x = parent[x];
        }
        path = (int *)malloc(sizeof(int) * (size_t)len);
        x = v;
        for (j = len - 1; j >= 0; j--) {
            path[j] = x;
            x = parent[x];
        }
        printf("   N%d 距离%d : ", sorted[i] + 1, len - 1);
        for (j = 0; j < len; j++) {
            printf("N%d", path[j] + 1);
            if (j < len - 1)
                printf("-");
        }
        printf("\n");
        free(path);
    }
    free(sorted);
}

static void emit(const int *pref, int plen, const int *tail, int tlen,
                 const int *parent)
{
    int *sel;
    int cnt = 0, i;
    sel = (int *)malloc(sizeof(int) * (size_t)(plen + tlen));
    for (i = 0; i < plen; i++)
        sel[cnt++] = pref[i];
    for (i = 0; i < tlen; i++)
        sel[cnt++] = tail[i];
    print_set(sel, cnt, parent);
    free(sel);
}

static void solve(const Graph *g, int start, int n,
                  const int *locked, int nlock)
{
    int *dist, *parent, *reach;
    int *ordered, *odist;
    int *pref;
    int nreach = 0, i, j;
    int plen = 0;
    int remain;

    if (start < 0 || start >= g->T)
        return;
    if (n < 1)
        n = 1;
    if (n > g->T)
        n = g->T;

    dist = (int *)malloc(sizeof(int) * (size_t)g->T);
    parent = (int *)malloc(sizeof(int) * (size_t)g->T);
    reach = (int *)malloc(sizeof(int) * (size_t)g->T);
    ordered = (int *)malloc(sizeof(int) * (size_t)g->T);
    odist = (int *)malloc(sizeof(int) * (size_t)g->T);
    pref = (int *)malloc(sizeof(int) * (size_t)g->T);

    bfs(g, start, locked, nlock, dist, parent, reach, &nreach);

    for (i = 0; i < nreach; i++) {
        ordered[i] = reach[i];
        odist[i] = dist[reach[i]];
    }
    for (i = 1; i < nreach; i++) {
        int keyV = ordered[i], keyD = odist[i];
        j = i - 1;
        while (j >= 0 && (odist[j] > keyD ||
                          (odist[j] == keyD && ordered[j] > keyV))) {
            ordered[j + 1] = ordered[j];
            odist[j + 1] = odist[j];
            j--;
        }
        ordered[j + 1] = keyV;
        odist[j + 1] = keyD;
    }

    remain = n;
    i = 0;
    while (i < nreach && remain > 0) {
        int runDist = odist[i];
        int j0 = i;
        while (j0 < nreach && odist[j0] == runDist)
            j0++;
        if (runDist > MAXD)
            break;
        {
            int runlen = j0 - i;
            if (runlen <= remain) {
                for (j = i; j < j0; j++)
                    pref[plen++] = ordered[j];
                remain -= runlen;
                i = j0;
            } else {
                int *c;
                int k, done = 0, t;
                c = (int *)malloc(sizeof(int) * (size_t)remain);
                for (t = 0; t < remain; t++)
                    c[t] = t;
                while (!done) {
                    int *tail;
                    tail = (int *)malloc(sizeof(int) * (size_t)remain);
                    for (t = 0; t < remain; t++)
                        tail[t] = ordered[i + c[t]];
                    emit(pref, plen, tail, remain, parent);
                    free(tail);
                    k = remain - 1;
                    while (k >= 0 && c[k] == runlen - remain + k)
                        k--;
                    if (k < 0)
                        done = 1;
                    else {
                        c[k]++;
                        for (t = k + 1; t < remain; t++)
                            c[t] = c[t - 1] + 1;
                    }
                }
                free(c);
                free(dist); free(parent); free(reach);
                free(ordered); free(odist); free(pref);
                return;
            }
        }
    }

    if (remain == 0 || plen > 0)
        emit(pref, plen, NULL, 0, parent);

    free(dist); free(parent); free(reach);
    free(ordered); free(odist); free(pref);
}

static void free_graph(Graph *g)
{
    int i;
    for (i = 0; i < g->T; i++)
        free(g->adj[i]);
    free(g->adj);
    free(g->deg);
    free(g);
}

int main(void)
{
    int R, start, n, i;

    while (scanf("%d", &R) == 1) {
        int cap = 32;
        int nlock = 0, x;
        int *lock;
        Graph *g;

        lock = (int *)malloc(sizeof(int) * (size_t)cap);
        nlock = 0;
        for (;;) {
            scanf("%d", &x);
            if (x == 0)
                break;
            if (nlock == cap) {
                cap *= 2;
                lock = (int *)realloc(lock, sizeof(int) * (size_t)cap);
            }
            lock[nlock++] = x;
        }
        scanf("%d %d", &start, &n);

        if (R < 2 || R % 2 != 0) {
            printf("环形节点数R=%d无效(需为>=2的偶数)\n\n", R);
            free(lock);
            continue;
        }

        g = build_graph(R);

        printf("R=%d 总节点%d (环1..%d, 半矩阵%d..%d)",
               R, g->T, R, R + 1, g->T);
        printf(" 起点N%d 连通%d个点", start, n);
        printf(" 锁定{");
        for (i = 0; i < nlock; i++) {
            printf("N%d", lock[i]);
            if (i < nlock - 1)
                printf(",");
        }
        printf("}\n");

        if (start < 1 || start > g->T) {
            printf("起点越界\n\n");
        } else {
            int *nlockArr;
            nlockArr = (int *)malloc(sizeof(int) * (size_t)(nlock ? nlock : 1));
            for (i = 0; i < nlock; i++)
                nlockArr[i] = lock[i];
            solve(g, start - 1, n, nlockArr, nlock);
            free(nlockArr);
        }
        printf("\n");

        free_graph(g);
        free(lock);
    }
    return 0;
}