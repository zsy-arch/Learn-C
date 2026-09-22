/* demo.c —— 图遍历算法演示：BFS/DFS/连通分量/拓扑排序/环检测/二分图 */
#include "algos.h"
#include "graph.h"

#include <stdio.h>
#include <stdlib.h>

static void print_int_array(const int *a, int n)
{
    printf("[");
    for (int i = 0; i < n; i++) printf("%d%s", a[i], (i + 1 < n) ? "," : "");
    printf("]");
}

static void section1_representation(void)
{
    printf("========== 1. 图的表示：邻接矩阵 vs 邻接表 ==========\n");
    /* 5 个节点的无向图：0-1 0-2 1-3 2-3 3-4 */
    GraphMatrix *gm = matrix_create(5, false);
    matrix_add_edge(gm, 0, 1);
    matrix_add_edge(gm, 0, 2);
    matrix_add_edge(gm, 1, 3);
    matrix_add_edge(gm, 2, 3);
    matrix_add_edge(gm, 3, 4);

    printf("  邻接矩阵 (5x5):\n");
    for (int i = 0; i < 5; i++) {
        printf("    ");
        for (int j = 0; j < 5; j++) {
            printf("%d ", matrix_has_edge(gm, i, j) ? 1 : 0);
        }
        printf("\n");
    }
    printf("  -> 5x5=25 格子，边很稀疏时大量格子是 0，浪费空间\n");
    matrix_destroy(gm);

    Graph *g = graph_create(5, false);
    graph_add_edge(g, 0, 1);
    graph_add_edge(g, 0, 2);
    graph_add_edge(g, 1, 3);
    graph_add_edge(g, 2, 3);
    graph_add_edge(g, 3, 4);
    printf("  同一个图的邻接表:\n");
    graph_print(g);
    printf("  -> 只存实际存在的边，空间 O(V+E)，适合稀疏图\n");
    graph_destroy(g);
    printf("\n");
}

static void section2_bfs(void)
{
    printf("========== 2. BFS 最短路径（无权图）==========\n");
    /*
     * 0 - 1 - 3
     * |       |
     * 2 ------4
     * 手算最短路径：0->4 应该是 0-2-4，长度 2
     */
    Graph *g = graph_create(5, false);
    graph_add_edge(g, 0, 1);
    graph_add_edge(g, 0, 2);
    graph_add_edge(g, 1, 3);
    graph_add_edge(g, 2, 4);
    graph_add_edge(g, 3, 4);
    graph_print(g);

    printf("  从节点 0 开始 BFS，观察队列变化：\n");
    BFSResult r = bfs(g, 0, true);
    printf("  访问顺序: ");
    print_int_array(r.order, r.order_len);
    printf("\n");
    for (int i = 0; i < g->n; i++) {
        printf("    dist[0][%d] = %d, prev = %d\n", i, r.dist[i], r.prev[i]);
    }
    int path[16];
    int len = bfs_reconstruct_path(&r, 0, 4, path);
    printf("  0 -> 4 的最短路径: ");
    print_int_array(path, len);
    printf(" (长度 %d 条边)\n", len - 1);
    printf("  -> 与手算结果一致：0-2-4，2 条边\n");

    bfs_free(&r);
    graph_destroy(g);
    printf("\n");
}

static void section3_dfs(void)
{
    printf("========== 3. DFS：递归 vs 迭代，发现/完成时间 ==========\n");
    Graph *g = graph_create(6, true);
    graph_add_edge(g, 0, 1);
    graph_add_edge(g, 0, 2);
    graph_add_edge(g, 1, 3);
    graph_add_edge(g, 2, 4);
    graph_add_edge(g, 4, 5);
    graph_print(g);

    DFSResult rec = dfs_recursive(g, 0);
    printf("  递归 DFS 访问顺序: ");
    print_int_array(rec.order, rec.order_len);
    printf("\n");
    for (int i = 0; i < g->n; i++) {
        printf("    node %d: disc=%d fin=%d\n", i, rec.disc[i], rec.fin[i]);
    }

    DFSResult it = dfs_iterative(g, 0);
    printf("  迭代 DFS 访问顺序: ");
    print_int_array(it.order, it.order_len);
    printf("\n");
    printf("  -> 迭代版用「显式帧+邻居下标」模拟调用栈，\n");
    printf("     访问顺序和递归版完全一致（不是简单地把所有邻居一次性入栈）\n");

    dfs_free(&rec);
    dfs_free(&it);
    graph_destroy(g);
    printf("\n");
}

static void section4_components(void)
{
    printf("========== 4. 连通分量（无向图）==========\n");
    /* 三个分量: {0,1,2} {3,4} {5} */
    Graph *g = graph_create(6, false);
    graph_add_edge(g, 0, 1);
    graph_add_edge(g, 1, 2);
    graph_add_edge(g, 3, 4);
    graph_print(g);

    int comp[6];
    int count = connected_components(g, comp);
    printf("  分量数: %d\n", count);
    for (int i = 0; i < g->n; i++) {
        printf("    node %d -> component %d\n", i, comp[i]);
    }
    printf("  -> 节点 5 没有任何边，自己单独成一个分量\n");

    graph_destroy(g);
    printf("\n");
}

static void section5_topo(void)
{
    printf("========== 5. 拓扑排序：Kahn vs DFS ==========\n");
    /* 经典「穿衣服」DAG：
     * 内衣->裤子->鞋, 内衣->鞋, 裤子->皮带, 衬衫->外套, 外套->皮带, 袜子->鞋
     * 用数字编号: 0=内衣 1=裤子 2=鞋 3=皮带 4=衬衫 5=外套 6=袜子
     */
    Graph *g = graph_create(7, true);
    graph_add_edge(g, 0, 1); /* 内衣->裤子 */
    graph_add_edge(g, 1, 2); /* 裤子->鞋 */
    graph_add_edge(g, 0, 2); /* 内衣->鞋 */
    graph_add_edge(g, 1, 3); /* 裤子->皮带 */
    graph_add_edge(g, 4, 5); /* 衬衫->外套 */
    graph_add_edge(g, 5, 3); /* 外套->皮带 */
    graph_add_edge(g, 6, 2); /* 袜子->鞋 */
    graph_print(g);

    int out_kahn[7];
    bool ok1 = topo_sort_kahn(g, out_kahn);
    printf("  Kahn 算法结果: ");
    print_int_array(out_kahn, 7);
    printf(" (合法性: %s)\n", (ok1 && topo_order_is_valid(g, out_kahn)) ? "valid" : "INVALID");

    int out_dfs[7];
    bool ok2 = topo_sort_dfs(g, out_dfs);
    printf("  DFS   算法结果: ");
    print_int_array(out_dfs, 7);
    printf(" (合法性: %s)\n", (ok2 && topo_order_is_valid(g, out_dfs)) ? "valid" : "INVALID");
    printf("  -> 两个序列不一定相同（拓扑序不唯一），但都必须满足\n");
    printf("     「每条边 u->v，u 在结果里排在 v 前面」\n");

    graph_destroy(g);
    printf("\n");
}

static void section6_cycle(void)
{
    printf("========== 6. 环检测：有向图 vs 无向图 ==========\n");

    printf("  [有向图] 0->1->2->0 (有环):\n");
    Graph *gd1 = graph_create(3, true);
    graph_add_edge(gd1, 0, 1);
    graph_add_edge(gd1, 1, 2);
    graph_add_edge(gd1, 2, 0);
    printf("    has_cycle_directed = %s\n", has_cycle_directed(gd1) ? "true" : "false");
    graph_destroy(gd1);

    printf("  [有向图] 0->1->2 (无环):\n");
    Graph *gd2 = graph_create(3, true);
    graph_add_edge(gd2, 0, 1);
    graph_add_edge(gd2, 1, 2);
    printf("    has_cycle_directed = %s\n", has_cycle_directed(gd2) ? "true" : "false");
    graph_destroy(gd2);

    printf("  [无向图] 简单路径 A-B-C (0-1-2)，只有 2 条边:\n");
    Graph *gu1 = graph_create(3, false);
    graph_add_edge(gu1, 0, 1);
    graph_add_edge(gu1, 1, 2);
    printf("    has_cycle_undirected = %s\n", has_cycle_undirected(gu1) ? "true" : "false");
    printf("    -> 如果照抄有向图的写法（忘记排除父节点），\n");
    printf("       DFS 从 0 出发走 0 -> 1 -> 2，到 2 时检查 2 的邻接表，\n");
    printf("       会看到「2->1」这一项，而 1 正是刚刚走过来的父节点、\n");
    printf("       已经被标记成已访问——误判成环就是本章重点错误\n");
    printf("       （编译运行 buggy_cycle_demo.c 可以看到完整的假阳性过程）\n");
    graph_destroy(gu1);

    printf("  [无向图] 三角形 0-1-2-0 (真的有环):\n");
    Graph *gu2 = graph_create(3, false);
    graph_add_edge(gu2, 0, 1);
    graph_add_edge(gu2, 1, 2);
    graph_add_edge(gu2, 2, 0);
    printf("    has_cycle_undirected = %s\n", has_cycle_undirected(gu2) ? "true" : "false");
    graph_destroy(gu2);
    printf("\n");
}

static void section7_bipartite(void)
{
    printf("========== 7. 二分图判定 ==========\n");

    printf("  [二分图] 4 节点环 0-1-2-3-0 (偶数长度):\n");
    Graph *g1 = graph_create(4, false);
    graph_add_edge(g1, 0, 1);
    graph_add_edge(g1, 1, 2);
    graph_add_edge(g1, 2, 3);
    graph_add_edge(g1, 3, 0);
    int color1[4];
    bool bp1 = is_bipartite(g1, color1);
    printf("    is_bipartite = %s, 染色: ", bp1 ? "true" : "false");
    print_int_array(color1, 4);
    printf("\n");
    graph_destroy(g1);

    printf("  [非二分图] 三角形 0-1-2-0 (奇数长度环):\n");
    Graph *g2 = graph_create(3, false);
    graph_add_edge(g2, 0, 1);
    graph_add_edge(g2, 1, 2);
    graph_add_edge(g2, 2, 0);
    int color2[3];
    bool bp2 = is_bipartite(g2, color2);
    printf("    is_bipartite = %s\n", bp2 ? "true" : "false");
    printf("    -> 奇数长度的环一定不是二分图：染色到最后一条边必然冲突\n");
    graph_destroy(g2);
    printf("\n");
}

int main(void)
{
    section1_representation();
    section2_bfs();
    section3_dfs();
    section4_components();
    section5_topo();
    section6_cycle();
    section7_bipartite();
    printf("========== 全部演示完成 ==========\n");
    return 0;
}
