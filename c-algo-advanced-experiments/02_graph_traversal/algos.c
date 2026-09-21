/* algos.c —— algos.h 的实现 */
#include "algos.h"

#include <stdio.h>
#include <stdlib.h>

/* ============================================================ */
/* BFS 最短路径                                                   */
/* ============================================================ */

BFSResult bfs(const Graph *g, int src, bool verbose)
{
    BFSResult r;
    r.dist = malloc(sizeof(int) * (size_t)g->n);
    r.prev = malloc(sizeof(int) * (size_t)g->n);
    r.order = malloc(sizeof(int) * (size_t)g->n);
    r.order_len = 0;
    if (g->n > 0 && (!r.dist || !r.prev || !r.order)) {
        perror("malloc"); exit(1);
    }
    for (int i = 0; i < g->n; i++) { r.dist[i] = -1; r.prev[i] = -1; }

    if (g->n == 0) return r;

    IntQueue *q = queue_create(g->n);
    r.dist[src] = 0;
    queue_push(q, src);

    while (!queue_empty(q)) {
        if (verbose) {
            printf("    队列: [");
            for (int i = 0; i < q->size; i++) {
                int idx = (q->head + i) % q->cap;
                printf("%d%s", q->data[idx], (i + 1 < q->size) ? "," : "");
            }
            printf("]\n");
        }
        int u = queue_pop(q);
        r.order[r.order_len++] = u;
        for (AdjNode *cur = g->adj[u]; cur; cur = cur->next) {
            int v = cur->to;
            if (r.dist[v] == -1) {
                r.dist[v] = r.dist[u] + 1;
                r.prev[v] = u;
                queue_push(q, v);
            }
        }
    }
    queue_destroy(q);
    return r;
}

void bfs_free(BFSResult *r)
{
    free(r->dist);
    free(r->prev);
    free(r->order);
}

int bfs_reconstruct_path(const BFSResult *r, int src, int dst, int *out)
{
    if (r->dist[dst] == -1) return -1;
    int len = 0;
    int tmp[4096];
    int cur = dst;
    while (cur != src) {
        tmp[len++] = cur;
        cur = r->prev[cur];
    }
    tmp[len++] = src;
    for (int i = 0; i < len; i++) {
        out[i] = tmp[len - 1 - i];
    }
    return len;
}

/* ============================================================ */
/* DFS：递归 / 迭代，带发现时间 / 完成时间                          */
/* ============================================================ */

typedef struct DFSCtx {
    const Graph *g;
    bool  *visited;
    int   *disc;
    int   *fin;
    int   *order;
    int    order_len;
    int    timer;
} DFSCtx;

static void dfs_visit_recursive(DFSCtx *ctx, int u)
{
    ctx->visited[u] = true;
    ctx->disc[u] = ctx->timer++;
    ctx->order[ctx->order_len++] = u;
    for (AdjNode *cur = ctx->g->adj[u]; cur; cur = cur->next) {
        int v = cur->to;
        if (!ctx->visited[v]) {
            dfs_visit_recursive(ctx, v);
        }
    }
    ctx->fin[u] = ctx->timer++;
}

DFSResult dfs_recursive(const Graph *g, int src)
{
    DFSResult r;
    r.disc = malloc(sizeof(int) * (size_t)g->n);
    r.fin  = malloc(sizeof(int) * (size_t)g->n);
    r.order = malloc(sizeof(int) * (size_t)g->n);
    r.order_len = 0;
    if (g->n > 0 && (!r.disc || !r.fin || !r.order)) { perror("malloc"); exit(1); }

    bool *visited = calloc((size_t)g->n, sizeof *visited);
    if (g->n > 0 && !visited) { perror("calloc"); exit(1); }
    for (int i = 0; i < g->n; i++) { r.disc[i] = -1; r.fin[i] = -1; }

    if (g->n > 0) {
        DFSCtx ctx = { g, visited, r.disc, r.fin, r.order, 0, 0 };
        dfs_visit_recursive(&ctx, src);
        r.order_len = ctx.order_len;
    }
    free(visited);
    return r;
}

/*
 * 迭代 DFS 的一个常见误区：如果只用"弹出即访问"，得到的顺序和递归版
 * 不一致（因为邻居入栈顺序和递归调用顺序相反）。这里为了让迭代版和
 * 递归版的 disc 顺序尽量一致，采用"显式帧 + 邻居下标"模拟调用栈，
 * 而不是简单的"访问节点时把所有邻居入栈"。
 */
typedef struct Frame {
    int      node;
    AdjNode *next_edge; /* 下一个还没处理的邻居 */
} Frame;

DFSResult dfs_iterative(const Graph *g, int src)
{
    DFSResult r;
    r.disc = malloc(sizeof(int) * (size_t)g->n);
    r.fin  = malloc(sizeof(int) * (size_t)g->n);
    r.order = malloc(sizeof(int) * (size_t)g->n);
    r.order_len = 0;
    if (g->n > 0 && (!r.disc || !r.fin || !r.order)) { perror("malloc"); exit(1); }
    for (int i = 0; i < g->n; i++) { r.disc[i] = -1; r.fin[i] = -1; }

    if (g->n == 0) return r;

    bool *visited = calloc((size_t)g->n, sizeof *visited);
    if (!visited) { perror("calloc"); exit(1); }

    Frame *stack = malloc(sizeof(Frame) * (size_t)g->n);
    if (!stack) { perror("malloc"); exit(1); }
    int top = 0;
    int timer = 0;

    visited[src] = true;
    r.disc[src] = timer++;
    r.order[r.order_len++] = src;
    stack[top].node = src;
    stack[top].next_edge = g->adj[src];
    top++;

    while (top > 0) {
        Frame *f = &stack[top - 1];
        if (f->next_edge == NULL) {
            /* 这个节点的所有邻居都处理完了：完成 */
            r.fin[f->node] = timer++;
            top--;
        } else {
            int v = f->next_edge->to;
            f->next_edge = f->next_edge->next;
            if (!visited[v]) {
                visited[v] = true;
                r.disc[v] = timer++;
                r.order[r.order_len++] = v;
                stack[top].node = v;
                stack[top].next_edge = g->adj[v];
                top++;
            }
        }
    }

    free(stack);
    free(visited);
    return r;
}

void dfs_free(DFSResult *r)
{
    free(r->disc);
    free(r->fin);
    free(r->order);
}

/* ============================================================ */
/* 连通分量（无向图）                                             */
/* ============================================================ */

int connected_components(const Graph *g, int *comp)
{
    for (int i = 0; i < g->n; i++) comp[i] = -1;
    if (g->n == 0) return 0;

    IntQueue *q = queue_create(g->n);
    int comp_id = 0;
    for (int s = 0; s < g->n; s++) {
        if (comp[s] != -1) continue;
        comp[s] = comp_id;
        queue_push(q, s);
        while (!queue_empty(q)) {
            int u = queue_pop(q);
            for (AdjNode *cur = g->adj[u]; cur; cur = cur->next) {
                int v = cur->to;
                if (comp[v] == -1) {
                    comp[v] = comp_id;
                    queue_push(q, v);
                }
            }
        }
        comp_id++;
    }
    queue_destroy(q);
    return comp_id;
}

/* ============================================================ */
/* 拓扑排序                                                       */
/* ============================================================ */

bool topo_sort_kahn(const Graph *g, int *out)
{
    int n = g->n;
    int *indeg = calloc((size_t)n, sizeof *indeg);
    if (n > 0 && !indeg) { perror("calloc"); exit(1); }
    for (int u = 0; u < n; u++) {
        for (AdjNode *cur = g->adj[u]; cur; cur = cur->next) {
            indeg[cur->to]++;
        }
    }

    IntQueue *q = queue_create(n > 0 ? n : 1);
    for (int i = 0; i < n; i++) {
        if (indeg[i] == 0) queue_push(q, i);
    }

    int cnt = 0;
    while (!queue_empty(q)) {
        int u = queue_pop(q);
        out[cnt++] = u;
        for (AdjNode *cur = g->adj[u]; cur; cur = cur->next) {
            int v = cur->to;
            if (--indeg[v] == 0) {
                queue_push(q, v);
            }
        }
    }

    queue_destroy(q);
    free(indeg);
    return cnt == n; /* 处理的节点数不足 n，说明有环 */
}

/* DFS 版拓扑排序：后序（完成顺序）逆序即为拓扑序。
 * 用三色标记检测环：0=白(未访问) 1=灰(在当前递归栈上) 2=黑(已完成)。
 */
typedef struct TopoDfsCtx {
    const Graph *g;
    int  *color;
    int  *out;
    int   out_pos;  /* 从数组末尾往前写，最后不需要额外反转 */
    bool  cyclic;
} TopoDfsCtx;

static void topo_dfs_visit(TopoDfsCtx *ctx, int u)
{
    ctx->color[u] = 1; /* 灰 */
    for (AdjNode *cur = ctx->g->adj[u]; cur; cur = cur->next) {
        int v = cur->to;
        if (ctx->color[v] == 1) {
            ctx->cyclic = true;
            return;
        }
        if (ctx->color[v] == 0) {
            topo_dfs_visit(ctx, v);
            if (ctx->cyclic) return;
        }
    }
    ctx->color[u] = 2; /* 黑：完成 */
    ctx->out[ctx->out_pos--] = u;
}

bool topo_sort_dfs(const Graph *g, int *out)
{
    int n = g->n;
    int *color = calloc((size_t)n, sizeof *color);
    if (n > 0 && !color) { perror("calloc"); exit(1); }

    TopoDfsCtx ctx = { g, color, out, n - 1, false };
    for (int i = 0; i < n && !ctx.cyclic; i++) {
        if (color[i] == 0) {
            topo_dfs_visit(&ctx, i);
        }
    }
    free(color);
    return !ctx.cyclic;
}

bool topo_order_is_valid(const Graph *g, const int *order)
{
    int n = g->n;
    int *pos = malloc(sizeof(int) * (size_t)(n > 0 ? n : 1));
    if (n > 0 && !pos) { perror("malloc"); exit(1); }
    for (int i = 0; i < n; i++) pos[order[i]] = i;

    bool ok = true;
    for (int u = 0; u < n && ok; u++) {
        for (AdjNode *cur = g->adj[u]; cur; cur = cur->next) {
            if (pos[u] >= pos[cur->to]) { ok = false; break; }
        }
    }
    free(pos);
    return ok;
}

/* ============================================================ */
/* 环检测                                                         */
/* ============================================================ */

/* 有向图：三色标记法。遇到灰色节点（还在当前 DFS 路径上）即为回边=环。 */
static bool cycle_directed_visit(const Graph *g, int *color, int u)
{
    color[u] = 1;
    for (AdjNode *cur = g->adj[u]; cur; cur = cur->next) {
        int v = cur->to;
        if (color[v] == 1) return true;                  /* 回边，找到环 */
        if (color[v] == 0 && cycle_directed_visit(g, color, v)) return true;
    }
    color[u] = 2;
    return false;
}

bool has_cycle_directed(const Graph *g)
{
    int *color = calloc((size_t)g->n, sizeof *color);
    if (g->n > 0 && !color) { perror("calloc"); exit(1); }
    bool found = false;
    for (int i = 0; i < g->n && !found; i++) {
        if (color[i] == 0 && cycle_directed_visit(g, color, i)) found = true;
    }
    free(color);
    return found;
}

/*
 * 无向图：不能照抄有向图的"遇到已访问节点就是环"。
 * 因为无向图存边时 u-v 会同时生成 u->v 和 v->u 两条邻接表项，
 * DFS 从 u 走到 v 后，v 的邻接表里第一个就会遇到"回头指向 u"的边——
 * 那不是环，只是刚刚走过来的那条边的镜像。
 * 正确做法：记录"从哪个父节点走过来"，遇到已访问节点时，
 * 只有当它不是父节点，才判定为环。
 */
static bool cycle_undirected_visit(const Graph *g, bool *visited, int u, int parent)
{
    visited[u] = true;
    for (AdjNode *cur = g->adj[u]; cur; cur = cur->next) {
        int v = cur->to;
        if (!visited[v]) {
            if (cycle_undirected_visit(g, visited, v, u)) return true;
        } else if (v != parent) {
            return true; /* 访问过、且不是父节点 -> 环 */
        }
    }
    return false;
}

bool has_cycle_undirected(const Graph *g)
{
    bool *visited = calloc((size_t)g->n, sizeof *visited);
    if (g->n > 0 && !visited) { perror("calloc"); exit(1); }
    bool found = false;
    for (int i = 0; i < g->n && !found; i++) {
        if (!visited[i] && cycle_undirected_visit(g, visited, i, -1)) found = true;
    }
    free(visited);
    return found;
}

/* ============================================================ */
/* 二分图判定                                                     */
/* ============================================================ */

bool is_bipartite(const Graph *g, int *color)
{
    int n = g->n;
    for (int i = 0; i < n; i++) color[i] = -1;
    if (n == 0) return true;

    IntQueue *q = queue_create(n);
    bool ok = true;
    for (int s = 0; s < n && ok; s++) {
        if (color[s] != -1) continue;
        color[s] = 0;
        queue_push(q, s);
        while (!queue_empty(q) && ok) {
            int u = queue_pop(q);
            for (AdjNode *cur = g->adj[u]; cur; cur = cur->next) {
                int v = cur->to;
                if (color[v] == -1) {
                    color[v] = 1 - color[u];
                    queue_push(q, v);
                } else if (color[v] == color[u]) {
                    ok = false;
                    break;
                }
            }
        }
    }
    queue_destroy(q);
    return ok;
}
