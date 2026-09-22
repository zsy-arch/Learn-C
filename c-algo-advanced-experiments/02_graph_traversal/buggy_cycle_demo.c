/*
 * buggy_cycle_demo.c —— ⚠️ 错误示例
 *
 * 展示一个新手很容易写出来的 bug：把有向图的环检测思路直接搬到无向图上，
 * 即"DFS 时遇到任何已访问节点就判定为环"，不排除父节点。
 *
 * 这不是内存安全问题（不会被 sanitizer 抓到），是纯粹的逻辑错误——
 * 所以本文件故意保留这个 bug，用来对比 algos.c 里 has_cycle_undirected
 * 的正确实现。
 */
#include "graph.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* ❌ 错误实现：没有排除父节点 */
static bool buggy_visit(const Graph *g, bool *visited, int u)
{
    visited[u] = true;
    for (AdjNode *cur = g->adj[u]; cur; cur = cur->next) {
        int v = cur->to;
        if (visited[v]) {
            return true; /* ← bug：v 可能就是刚刚走过来的父节点 */
        }
        if (buggy_visit(g, visited, v)) return true;
    }
    return false;
}

static bool buggy_has_cycle_undirected(const Graph *g)
{
    bool *visited = calloc((size_t)g->n, sizeof *visited);
    if (g->n > 0 && !visited) { perror("calloc"); exit(1); }
    bool found = false;
    for (int i = 0; i < g->n && !found; i++) {
        if (!visited[i] && buggy_visit(g, visited, i)) found = true;
    }
    free(visited);
    return found;
}

int main(void)
{
    printf("========== 错误示例：无向图环检测忘记排除父节点 ==========\n");

    printf("\n用例 1: 简单路径 A-B-C (0-1-2)，没有环\n");
    Graph *g1 = graph_create(3, false);
    graph_add_edge(g1, 0, 1);
    graph_add_edge(g1, 1, 2);
    bool r1 = buggy_has_cycle_undirected(g1);
    printf("  错误实现结果: has_cycle = %s\n", r1 ? "true" : "false");
    printf("  正确答案应该是: false\n");
    printf("  -> %s\n", r1 ? "假阳性！！！这就是本文件要展示的 bug" : "这次凑巧对了（见下面用例 2 的分析）");
    graph_destroy(g1);

    printf("\n用例 2: 一条更长的链 0-1-2-3-4\n");
    Graph *g2 = graph_create(5, false);
    graph_add_edge(g2, 0, 1);
    graph_add_edge(g2, 1, 2);
    graph_add_edge(g2, 2, 3);
    graph_add_edge(g2, 3, 4);
    bool r2 = buggy_has_cycle_undirected(g2);
    printf("  错误实现结果: has_cycle = %s\n", r2 ? "true" : "false");
    printf("  正确答案应该是: false\n");
    graph_destroy(g2);

    printf("\n========== 原因分析 ==========\n");
    printf("加边 graph_add_edge(g, u, v) 对无向图会同时生成两条邻接表项：\n");
    printf("  u 的邻接表里加一条 u->v\n");
    printf("  v 的邻接表里加一条 v->u\n");
    printf("以用例 1（0-1-2）为例，DFS 从 0 出发，实际走的是 0 -> 1 -> 2。\n");
    printf("走到 2 之后检查 2 的邻接表，会看到「2->1」这一项——\n");
    printf("这其实就是刚才从 1 走过来那条边的另一半，不是新发现的环。\n");
    printf("错误实现只要看到「已访问」就报环，于是在这里假报了一个环。\n");
    printf("推广一下：只要图里存在任意一条边，DFS 往下走一层之后，\n");
    printf("在最深处那个节点上必然会看到「已访问的父节点」，所以这个\n");
    printf("错误实现对任何有边的图都会报环——它其实什么也没在检测。\n");
    printf("\n正确写法：记录 DFS 是从哪个父节点走过来的，\n");
    printf("遇到已访问节点时，只有当它不是父节点，才是真正的环。\n");
    printf("对比 algos.c 里的 has_cycle_undirected(const Graph *g)。\n");

    return 0;
}
