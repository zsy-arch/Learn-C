/* tests.c —— 图算法多组测试用例 */
#include "algos.h"
#include "graph.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond) check_impl((cond), #cond, __func__, __LINE__)

static void check_impl(bool cond, const char *expr, const char *func, int line)
{
    if (!cond) {
        printf("[FAIL] %s (assertion failed: %s, line %d)\n", func, expr, line);
        g_fail++;
    }
}

#define TEST_BEGIN() int local_fail_before = g_fail
#define TEST_END(name) \
    do { \
        if (g_fail == local_fail_before) { printf("[PASS] %s\n", name); g_pass++; } \
        else { g_fail = local_fail_before + 1; } \
    } while (0)

/* ---------------------------------------------------------------- */
/* 1. 空图 / 单节点                                                   */
/* ---------------------------------------------------------------- */

static void test_empty_graph(void)
{
    TEST_BEGIN();
    Graph *g = graph_create(0, false);
    CHECK(g->n == 0);
    BFSResult r = bfs(g, 0, false); /* src 越界但 n==0 时不会真正循环 */
    (void)r; /* bfs 在 n==0 时直接返回全 -1 数组，不会解引用 */
    CHECK(r.order_len == 0);
    bfs_free(&r);

    int comp[1];
    int cnt = connected_components(g, comp);
    CHECK(cnt == 0);

    graph_destroy(g);
    TEST_END("test_empty_graph");
}

static void test_single_node(void)
{
    TEST_BEGIN();
    Graph *g = graph_create(1, false);
    BFSResult r = bfs(g, 0, false);
    CHECK(r.dist[0] == 0);
    CHECK(r.order_len == 1);
    bfs_free(&r);

    int comp[1];
    int cnt = connected_components(g, comp);
    CHECK(cnt == 1);
    CHECK(comp[0] == 0);

    CHECK(has_cycle_undirected(g) == false);
    graph_destroy(g);

    /* 单节点带自环：有向图自环算环 */
    Graph *g2 = graph_create(1, true);
    graph_add_edge(g2, 0, 0);
    CHECK(has_cycle_directed(g2) == true);
    graph_destroy(g2);

    TEST_END("test_single_node");
}

/* ---------------------------------------------------------------- */
/* 2. 多个连通分量                                                     */
/* ---------------------------------------------------------------- */

static void test_multiple_components(void)
{
    TEST_BEGIN();
    Graph *g = graph_create(7, false);
    graph_add_edge(g, 0, 1);
    graph_add_edge(g, 1, 2);
    graph_add_edge(g, 3, 4);
    /* 5, 6 孤立 */

    int comp[7];
    int cnt = connected_components(g, comp);
    CHECK(cnt == 4); /* {0,1,2} {3,4} {5} {6} */
    CHECK(comp[0] == comp[1] && comp[1] == comp[2]);
    CHECK(comp[3] == comp[4]);
    CHECK(comp[5] != comp[6]);
    CHECK(comp[0] != comp[3]);

    graph_destroy(g);
    TEST_END("test_multiple_components");
}

/* ---------------------------------------------------------------- */
/* 3. 有向图环检测                                                     */
/* ---------------------------------------------------------------- */

static void test_directed_cycle_present(void)
{
    TEST_BEGIN();
    Graph *g = graph_create(4, true);
    graph_add_edge(g, 0, 1);
    graph_add_edge(g, 1, 2);
    graph_add_edge(g, 2, 3);
    graph_add_edge(g, 3, 1); /* 1->2->3->1 环 */
    CHECK(has_cycle_directed(g) == true);
    graph_destroy(g);
    TEST_END("test_directed_cycle_present");
}

static void test_directed_no_cycle(void)
{
    TEST_BEGIN();
    Graph *g = graph_create(4, true);
    graph_add_edge(g, 0, 1);
    graph_add_edge(g, 0, 2);
    graph_add_edge(g, 1, 3);
    graph_add_edge(g, 2, 3); /* 菱形 DAG，无环 */
    CHECK(has_cycle_directed(g) == false);
    graph_destroy(g);
    TEST_END("test_directed_no_cycle");
}

/* ---------------------------------------------------------------- */
/* 4. 无向图环检测（重点：父节点边不能误判为环）                          */
/* ---------------------------------------------------------------- */

static void test_undirected_no_cycle_simple_path(void)
{
    TEST_BEGIN();
    /* A-B-C 简单路径：0-1-2，只有 2 条边，绝对没有环。
     * 如果实现忘记排除父节点，DFS 从 1 走到 0 后，会在 0 的邻接表
     * 里看到"回到 1"的边，误判为环——这个测试就是专门防这个 bug。 */
    Graph *g = graph_create(3, false);
    graph_add_edge(g, 0, 1);
    graph_add_edge(g, 1, 2);
    CHECK(has_cycle_undirected(g) == false);
    graph_destroy(g);
    TEST_END("test_undirected_no_cycle_simple_path");
}

static void test_undirected_no_cycle_star(void)
{
    TEST_BEGIN();
    /* 星形图：0 连接 1,2,3,4，也没有环 */
    Graph *g = graph_create(5, false);
    graph_add_edge(g, 0, 1);
    graph_add_edge(g, 0, 2);
    graph_add_edge(g, 0, 3);
    graph_add_edge(g, 0, 4);
    CHECK(has_cycle_undirected(g) == false);
    graph_destroy(g);
    TEST_END("test_undirected_no_cycle_star");
}

static void test_undirected_cycle_triangle(void)
{
    TEST_BEGIN();
    Graph *g = graph_create(3, false);
    graph_add_edge(g, 0, 1);
    graph_add_edge(g, 1, 2);
    graph_add_edge(g, 2, 0);
    CHECK(has_cycle_undirected(g) == true);
    graph_destroy(g);
    TEST_END("test_undirected_cycle_triangle");
}

static void test_undirected_cycle_with_extra_branch(void)
{
    TEST_BEGIN();
    /* 三角形 0-1-2-0，外加一条不在环上的分支 2-3。
     * 分支本身不是环，但整个图因为三角形而有环。 */
    Graph *g = graph_create(4, false);
    graph_add_edge(g, 0, 1);
    graph_add_edge(g, 1, 2);
    graph_add_edge(g, 2, 0);
    graph_add_edge(g, 2, 3);
    CHECK(has_cycle_undirected(g) == true);
    graph_destroy(g);
    TEST_END("test_undirected_cycle_with_extra_branch");
}

/* ---------------------------------------------------------------- */
/* 5. 拓扑排序                                                        */
/* ---------------------------------------------------------------- */

static void test_topo_sort_valid_dag(void)
{
    TEST_BEGIN();
    Graph *g = graph_create(6, true);
    graph_add_edge(g, 5, 2);
    graph_add_edge(g, 5, 0);
    graph_add_edge(g, 4, 0);
    graph_add_edge(g, 4, 1);
    graph_add_edge(g, 2, 3);
    graph_add_edge(g, 3, 1);

    int out1[6];
    bool ok1 = topo_sort_kahn(g, out1);
    CHECK(ok1 == true);
    CHECK(topo_order_is_valid(g, out1) == true);

    int out2[6];
    bool ok2 = topo_sort_dfs(g, out2);
    CHECK(ok2 == true);
    CHECK(topo_order_is_valid(g, out2) == true);

    graph_destroy(g);
    TEST_END("test_topo_sort_valid_dag");
}

static void test_topo_sort_detects_cycle(void)
{
    TEST_BEGIN();
    Graph *g = graph_create(3, true);
    graph_add_edge(g, 0, 1);
    graph_add_edge(g, 1, 2);
    graph_add_edge(g, 2, 0);

    int out1[3];
    CHECK(topo_sort_kahn(g, out1) == false);

    int out2[3];
    CHECK(topo_sort_dfs(g, out2) == false);

    graph_destroy(g);
    TEST_END("test_topo_sort_detects_cycle");
}

/* 一个"看起来是错误答案但其实合法"的场景：验证函数不能直接比较数组 */
static void test_topo_sort_nonunique_but_both_valid(void)
{
    TEST_BEGIN();
    /* 两条独立链：0->1, 2->3。拓扑序里 0,1,2,3 和 2,3,0,1 都合法 */
    Graph *g = graph_create(4, true);
    graph_add_edge(g, 0, 1);
    graph_add_edge(g, 2, 3);

    int out1[4], out2[4];
    topo_sort_kahn(g, out1);
    topo_sort_dfs(g, out2);
    CHECK(topo_order_is_valid(g, out1));
    CHECK(topo_order_is_valid(g, out2));
    /* 不要求 out1 == out2，只要求各自合法 */

    graph_destroy(g);
    TEST_END("test_topo_sort_nonunique_but_both_valid");
}

/* ---------------------------------------------------------------- */
/* 6. 二分图判定                                                      */
/* ---------------------------------------------------------------- */

static void test_bipartite_even_cycle(void)
{
    TEST_BEGIN();
    Graph *g = graph_create(4, false);
    graph_add_edge(g, 0, 1);
    graph_add_edge(g, 1, 2);
    graph_add_edge(g, 2, 3);
    graph_add_edge(g, 3, 0);
    int color[4];
    CHECK(is_bipartite(g, color) == true);
    CHECK(color[0] != color[1]);
    CHECK(color[1] != color[2]);
    graph_destroy(g);
    TEST_END("test_bipartite_even_cycle");
}

static void test_bipartite_odd_cycle_fails(void)
{
    TEST_BEGIN();
    Graph *g = graph_create(3, false);
    graph_add_edge(g, 0, 1);
    graph_add_edge(g, 1, 2);
    graph_add_edge(g, 2, 0);
    int color[3];
    CHECK(is_bipartite(g, color) == false);
    graph_destroy(g);
    TEST_END("test_bipartite_odd_cycle_fails");
}

static void test_bipartite_disconnected(void)
{
    TEST_BEGIN();
    /* 一个二分的偶数环 + 一个孤立点，整体仍是二分图 */
    Graph *g = graph_create(5, false);
    graph_add_edge(g, 0, 1);
    graph_add_edge(g, 1, 2);
    graph_add_edge(g, 2, 3);
    graph_add_edge(g, 3, 0);
    /* 节点 4 孤立 */
    int color[5];
    CHECK(is_bipartite(g, color) == true);
    graph_destroy(g);
    TEST_END("test_bipartite_disconnected");
}

/* ---------------------------------------------------------------- */
/* 7. BFS 最短路径与已知答案对比                                        */
/* ---------------------------------------------------------------- */

static void test_bfs_shortest_path_known_answer(void)
{
    TEST_BEGIN();
    /*
     * 手工构造并手算最短路径的图：
     *   0 - 1 - 3
     *   |       |
     *   2 ------4
     * 0->4: 0-2-4 (2 条边)  最短
     * 0->3: 0-1-3 (2 条边)  最短（0-2-4-3 是 3 条边，更长）
     */
    Graph *g = graph_create(5, false);
    graph_add_edge(g, 0, 1);
    graph_add_edge(g, 0, 2);
    graph_add_edge(g, 1, 3);
    graph_add_edge(g, 2, 4);
    graph_add_edge(g, 3, 4);

    BFSResult r = bfs(g, 0, false);
    CHECK(r.dist[0] == 0);
    CHECK(r.dist[1] == 1);
    CHECK(r.dist[2] == 1);
    CHECK(r.dist[3] == 2);
    CHECK(r.dist[4] == 2);

    int path[16];
    int len = bfs_reconstruct_path(&r, 0, 4, path);
    CHECK(len == 3); /* 3 个节点 = 2 条边 */
    CHECK(path[0] == 0 && path[2] == 4);

    bfs_free(&r);
    graph_destroy(g);
    TEST_END("test_bfs_shortest_path_known_answer");
}

static void test_bfs_unreachable(void)
{
    TEST_BEGIN();
    Graph *g = graph_create(4, false);
    graph_add_edge(g, 0, 1);
    /* 2, 3 与 0,1 不连通 */
    graph_add_edge(g, 2, 3);

    BFSResult r = bfs(g, 0, false);
    CHECK(r.dist[0] == 0);
    CHECK(r.dist[1] == 1);
    CHECK(r.dist[2] == -1);
    CHECK(r.dist[3] == -1);

    int path[16];
    int len = bfs_reconstruct_path(&r, 0, 2, path);
    CHECK(len == -1);

    bfs_free(&r);
    graph_destroy(g);
    TEST_END("test_bfs_unreachable");
}

/* ---------------------------------------------------------------- */
/* 8. DFS 递归 vs 迭代 一致性                                          */
/* ---------------------------------------------------------------- */

static void test_dfs_recursive_matches_iterative(void)
{
    TEST_BEGIN();
    Graph *g = graph_create(8, true);
    graph_add_edge(g, 0, 1);
    graph_add_edge(g, 0, 2);
    graph_add_edge(g, 1, 3);
    graph_add_edge(g, 1, 4);
    graph_add_edge(g, 2, 5);
    graph_add_edge(g, 5, 6);
    graph_add_edge(g, 6, 7);

    DFSResult rec = dfs_recursive(g, 0);
    DFSResult it = dfs_iterative(g, 0);

    CHECK(rec.order_len == it.order_len);
    for (int i = 0; i < rec.order_len; i++) {
        CHECK(rec.order[i] == it.order[i]);
    }
    /* disc/fin 的相对顺序性质：fin[u] > disc[u] 对每个已访问节点都成立 */
    for (int i = 0; i < g->n; i++) {
        CHECK(rec.fin[i] > rec.disc[i]);
    }

    dfs_free(&rec);
    dfs_free(&it);
    graph_destroy(g);
    TEST_END("test_dfs_recursive_matches_iterative");
}

/* ---------------------------------------------------------------- */
/* 9. 大规模随机图（固定种子）                                          */
/* ---------------------------------------------------------------- */

static void test_large_random_graph_bfs_dfs_consistency(void)
{
    TEST_BEGIN();
    srand(20260921u);
    const int n = 200;
    Graph *g = graph_create(n, false);
    for (int i = 0; i < n * 3; i++) {
        int u = rand() % n;
        int v = rand() % n;
        if (u != v) graph_add_edge(g, u, v);
    }

    int comp[200];
    int comp_count = connected_components(g, comp);
    CHECK(comp_count >= 1);

    /* 每个分量的大小之和必须等于 n，且每个节点只属于一个分量 */
    int *comp_size = calloc((size_t)comp_count, sizeof *comp_size);
    for (int i = 0; i < n; i++) {
        CHECK(comp[i] >= 0 && comp[i] < comp_count);
        comp_size[comp[i]]++;
    }
    int total = 0;
    for (int i = 0; i < comp_count; i++) total += comp_size[i];
    CHECK(total == n);
    free(comp_size);

    /* BFS 从任意节点出发，可达节点数应等于该节点所在分量的大小 */
    BFSResult r = bfs(g, 0, false);
    int reachable = 0;
    for (int i = 0; i < n; i++) {
        if (r.dist[i] != -1) reachable++;
    }
    int expected = 0;
    for (int i = 0; i < n; i++) {
        if (comp[i] == comp[0]) expected++;
    }
    CHECK(reachable == expected);
    CHECK(r.order_len == expected);

    /* DFS 递归与迭代访问到的节点集合应该一致（不要求顺序，因为这里只验证集合） */
    DFSResult rd = dfs_recursive(g, 0);
    DFSResult id = dfs_iterative(g, 0);
    CHECK(rd.order_len == expected);
    CHECK(id.order_len == expected);

    bfs_free(&r);
    dfs_free(&rd);
    dfs_free(&id);
    graph_destroy(g);
    TEST_END("test_large_random_graph_bfs_dfs_consistency");
}

static void test_large_random_dag_topo_sort(void)
{
    TEST_BEGIN();
    srand(777u);
    const int n = 150;
    Graph *g = graph_create(n, true);
    /* 只从小编号指向大编号，保证一定是 DAG */
    for (int i = 0; i < n * 4; i++) {
        int u = rand() % n;
        int v = rand() % n;
        if (u < v) graph_add_edge(g, u, v);
    }

    int out1[150];
    bool ok1 = topo_sort_kahn(g, out1);
    CHECK(ok1 == true);
    CHECK(topo_order_is_valid(g, out1) == true);

    int out2[150];
    bool ok2 = topo_sort_dfs(g, out2);
    CHECK(ok2 == true);
    CHECK(topo_order_is_valid(g, out2) == true);

    CHECK(has_cycle_directed(g) == false);

    graph_destroy(g);
    TEST_END("test_large_random_dag_topo_sort");
}

/* ---------------------------------------------------------------- */
/* 10. 邻接矩阵基本行为                                                */
/* ---------------------------------------------------------------- */

static void test_adjacency_matrix_basic(void)
{
    TEST_BEGIN();
    GraphMatrix *g = matrix_create(4, true);
    matrix_add_edge(g, 0, 1);
    matrix_add_edge(g, 1, 2);
    CHECK(matrix_has_edge(g, 0, 1) == true);
    CHECK(matrix_has_edge(g, 1, 0) == false); /* 有向图不对称 */
    CHECK(matrix_has_edge(g, 1, 2) == true);
    CHECK(matrix_has_edge(g, 0, 2) == false);
    matrix_destroy(g);

    GraphMatrix *gu = matrix_create(4, false);
    matrix_add_edge(gu, 0, 1);
    CHECK(matrix_has_edge(gu, 0, 1) == true);
    CHECK(matrix_has_edge(gu, 1, 0) == true); /* 无向图对称 */
    matrix_destroy(gu);

    TEST_END("test_adjacency_matrix_basic");
}

int main(void)
{
    test_empty_graph();
    test_single_node();
    test_multiple_components();
    test_directed_cycle_present();
    test_directed_no_cycle();
    test_undirected_no_cycle_simple_path();
    test_undirected_no_cycle_star();
    test_undirected_cycle_triangle();
    test_undirected_cycle_with_extra_branch();
    test_topo_sort_valid_dag();
    test_topo_sort_detects_cycle();
    test_topo_sort_nonunique_but_both_valid();
    test_bipartite_even_cycle();
    test_bipartite_odd_cycle_fails();
    test_bipartite_disconnected();
    test_bfs_shortest_path_known_answer();
    test_bfs_unreachable();
    test_dfs_recursive_matches_iterative();
    test_large_random_graph_bfs_dfs_consistency();
    test_large_random_dag_topo_sort();
    test_adjacency_matrix_basic();

    printf("\n========== 测试汇总 ==========\n");
    printf("PASS: %d, FAIL: %d, TOTAL: %d\n", g_pass, g_fail, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}
