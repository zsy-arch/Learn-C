/* graph.h —— 邻接表 + 邻接矩阵，供 demo.c / tests.c 共用 */
#ifndef GRAPH_H
#define GRAPH_H

#include <stdbool.h>
#include <stddef.h>

/* ---------- 邻接表 ---------- */

typedef struct AdjNode {
    int              to;      /* 邻居编号 */
    struct AdjNode  *next;
} AdjNode;

typedef struct Graph {
    int       n;          /* 节点数，编号 0..n-1 */
    bool      directed;
    AdjNode **adj;        /* adj[i] 是节点 i 的邻接链表头 */
    int       edge_count;
} Graph;

Graph *graph_create(int n, bool directed);
void   graph_destroy(Graph *g);
void   graph_add_edge(Graph *g, int u, int v);
void   graph_print(const Graph *g);

/* ---------- 邻接矩阵（用于小规模演示/对照） ---------- */

typedef struct GraphMatrix {
    int   n;
    bool  directed;
    bool *m;   /* n*n 的一维数组，m[u*n+v] 表示 u->v 是否有边 */
} GraphMatrix;

GraphMatrix *matrix_create(int n, bool directed);
void         matrix_destroy(GraphMatrix *g);
void         matrix_add_edge(GraphMatrix *g, int u, int v);
bool         matrix_has_edge(const GraphMatrix *g, int u, int v);

/* ---------- 简单队列 / 栈（数组模拟，用于 BFS / 迭代 DFS） ---------- */

typedef struct IntQueue {
    int *data;
    int  head, tail, size, cap;
} IntQueue;

IntQueue *queue_create(int cap);
void      queue_destroy(IntQueue *q);
bool      queue_empty(const IntQueue *q);
void      queue_push(IntQueue *q, int v);
int       queue_pop(IntQueue *q);

typedef struct IntStack {
    int *data;
    int  top;   /* 下一个可写位置；top==0 表示空 */
    int  cap;
} IntStack;

IntStack *stack_create(int cap);
void      stack_destroy(IntStack *s);
bool      stack_empty(const IntStack *s);
void      stack_push(IntStack *s, int v);
int       stack_pop(IntStack *s);

#endif /* GRAPH_H */
