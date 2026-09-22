/* graph.c —— graph.h 的实现 */
#include "graph.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

Graph *graph_create(int n, bool directed)
{
    Graph *g = malloc(sizeof *g);
    if (!g) { perror("malloc"); exit(1); }
    g->n = n;
    g->directed = directed;
    g->edge_count = 0;
    g->adj = calloc((size_t)n, sizeof *g->adj); /* 全部初始化为 NULL */
    if (n > 0 && !g->adj) { perror("calloc"); exit(1); }
    return g;
}

void graph_destroy(Graph *g)
{
    if (!g) return;
    for (int i = 0; i < g->n; i++) {
        AdjNode *cur = g->adj[i];
        while (cur) {
            AdjNode *next = cur->next;
            free(cur);
            cur = next;
        }
    }
    free(g->adj);
    free(g);
}

static void add_directed_edge(Graph *g, int u, int v)
{
    AdjNode *node = malloc(sizeof *node);
    if (!node) { perror("malloc"); exit(1); }
    node->to = v;
    node->next = g->adj[u];
    g->adj[u] = node; /* 头插，O(1) */
}

void graph_add_edge(Graph *g, int u, int v)
{
    /* 越界的节点编号是调用者的编程错误，不是运行时可恢复的情况：
     * 没有这两行的话，graph_add_edge(g, 99, 0) 会直接往 g->adj[99] 写，
     * 是一次静默的堆越界写——ASan 能抓到，但报错位置离真正的错误很远。 */
    assert(u >= 0 && u < g->n);
    assert(v >= 0 && v < g->n);
    add_directed_edge(g, u, v);
    if (!g->directed) {
        add_directed_edge(g, v, u);
    }
    g->edge_count++;
}

void graph_print(const Graph *g)
{
    printf("  graph(n=%d, %s, edges=%d):\n",
           g->n, g->directed ? "directed" : "undirected", g->edge_count);
    for (int i = 0; i < g->n; i++) {
        printf("    %d:", i);
        for (AdjNode *cur = g->adj[i]; cur; cur = cur->next) {
            printf(" -> %d", cur->to);
        }
        printf("\n");
    }
}

/* ---------- 邻接矩阵 ---------- */

GraphMatrix *matrix_create(int n, bool directed)
{
    GraphMatrix *g = malloc(sizeof *g);
    if (!g) { perror("malloc"); exit(1); }
    g->n = n;
    g->directed = directed;
    g->m = calloc((size_t)n * (size_t)n, sizeof *g->m);
    if (n > 0 && !g->m) { perror("calloc"); exit(1); }
    return g;
}

void matrix_destroy(GraphMatrix *g)
{
    if (!g) return;
    free(g->m);
    free(g);
}

void matrix_add_edge(GraphMatrix *g, int u, int v)
{
    assert(u >= 0 && u < g->n);
    assert(v >= 0 && v < g->n);
    g->m[u * g->n + v] = true;
    if (!g->directed) {
        g->m[v * g->n + u] = true;
    }
}

bool matrix_has_edge(const GraphMatrix *g, int u, int v)
{
    assert(u >= 0 && u < g->n);
    assert(v >= 0 && v < g->n);
    return g->m[u * g->n + v];
}

/* ---------- 队列（数组模拟，用于 BFS） ---------- */

IntQueue *queue_create(int cap)
{
    IntQueue *q = malloc(sizeof *q);
    if (!q) { perror("malloc"); exit(1); }
    q->data = malloc(sizeof(int) * (size_t)cap);
    if (cap > 0 && !q->data) { perror("malloc"); exit(1); }
    q->head = q->tail = q->size = 0;
    q->cap = cap;
    return q;
}

void queue_destroy(IntQueue *q)
{
    if (!q) return;
    free(q->data);
    free(q);
}

bool queue_empty(const IntQueue *q) { return q->size == 0; }

void queue_push(IntQueue *q, int v)
{
    q->data[q->tail] = v;
    q->tail = (q->tail + 1) % q->cap;
    q->size++;
}

int queue_pop(IntQueue *q)
{
    int v = q->data[q->head];
    q->head = (q->head + 1) % q->cap;
    q->size--;
    return v;
}

/* ---------- 栈（数组模拟，用于迭代 DFS） ---------- */

IntStack *stack_create(int cap)
{
    IntStack *s = malloc(sizeof *s);
    if (!s) { perror("malloc"); exit(1); }
    s->data = malloc(sizeof(int) * (size_t)cap);
    if (cap > 0 && !s->data) { perror("malloc"); exit(1); }
    s->top = 0;
    s->cap = cap;
    return s;
}

void stack_destroy(IntStack *s)
{
    if (!s) return;
    free(s->data);
    free(s);
}

bool stack_empty(const IntStack *s) { return s->top == 0; }

void stack_push(IntStack *s, int v) { s->data[s->top++] = v; }

int stack_pop(IntStack *s) { return s->data[--s->top]; }
