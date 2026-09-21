/* algos.h —— BFS/DFS/拓扑排序/环检测/二分图，基于 graph.h 的邻接表 */
#ifndef ALGOS_H
#define ALGOS_H

#include "graph.h"
#include <stdbool.h>

/* ---------- BFS 最短路径 ---------- */

typedef struct BFSResult {
    int *dist;   /* dist[i]：从 src 到 i 的边数，不可达为 -1 */
    int *prev;   /* prev[i]：i 在最短路径树里的前驱，-1 表示无 */
    int *order;  /* order[k]：第 k 个被访问的节点编号 */
    int  order_len;
} BFSResult;

BFSResult bfs(const Graph *g, int src, bool verbose);
void      bfs_free(BFSResult *r);
/* 从 prev 数组重建 src -> dst 的路径，写入 out（调用者提供足够大的缓冲区），返回路径长度（节点数），不可达返回 -1 */
int       bfs_reconstruct_path(const BFSResult *r, int src, int dst, int *out);

/* ---------- DFS：递归 / 迭代，带发现时间 / 完成时间 ---------- */

typedef struct DFSResult {
    int *disc;   /* 发现时间 */
    int *fin;    /* 完成时间 */
    int *order;  /* 发现顺序 */
    int  order_len;
} DFSResult;

DFSResult dfs_recursive(const Graph *g, int src);
DFSResult dfs_iterative(const Graph *g, int src);
void      dfs_free(DFSResult *r);

/* ---------- 连通分量（无向图） ---------- */

/* comp[i] = 节点 i 所属分量编号（从 0 开始）；返回分量总数 */
int connected_components(const Graph *g, int *comp);

/* ---------- 拓扑排序（有向图） ---------- */

/* 成功返回 true 并把拓扑序写入 out（长度 g->n）；检测到环返回 false */
bool topo_sort_kahn(const Graph *g, int *out);
bool topo_sort_dfs(const Graph *g, int *out);
/* 验证 order 是否是合法拓扑序：对每条边 u->v，要求 pos[u] < pos[v] */
bool topo_order_is_valid(const Graph *g, const int *order);

/* ---------- 环检测 ---------- */

bool has_cycle_directed(const Graph *g);
bool has_cycle_undirected(const Graph *g);

/* ---------- 二分图判定 ---------- */

/* color 数组长度 g->n，判定成功时写入 0/1 染色（多分量图逐个染色） */
bool is_bipartite(const Graph *g, int *color);

#endif /* ALGOS_H */
