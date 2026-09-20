/* test_main.c —— algolib 的测试套件
 *
 * 一个 40 行的手写测试框架，够用就好。
 * 返回非 0 表示有测试失败，可以直接接进 CI。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "algolib.h"

/* ================================================================ */
/* 极简测试框架                                                      */
/* ================================================================ */
static int g_pass = 0;
static int g_fail = 0;
static const char *g_section = "";

#define CHECK(cond)                                                         \
    do {                                                                    \
        if (cond) {                                                         \
            g_pass++;                                                       \
        } else {                                                            \
            g_fail++;                                                       \
            printf("  FAIL [%s] %s:%d  %s\n",                               \
                   g_section, __FILE__, __LINE__, #cond);                   \
        }                                                                   \
    } while (0)

#define CHECK_EQ_INT(actual, expect)                                        \
    do {                                                                    \
        long _a = (long)(actual), _e = (long)(expect);                      \
        if (_a == _e) {                                                     \
            g_pass++;                                                       \
        } else {                                                            \
            g_fail++;                                                       \
            printf("  FAIL [%s] %s:%d  %s == %s (实际 %ld, 期望 %ld)\n",    \
                   g_section, __FILE__, __LINE__, #actual, #expect, _a, _e);\
        }                                                                   \
    } while (0)

#define SECTION(name) do { g_section = (name); printf("== %s ==\n", g_section); } while (0)

/* 可复现的伪随机数 */
static unsigned g_rng = 12345u;
static void rng_seed(unsigned s) { g_rng = s ? s : 1u; }
static unsigned rng_next(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

/* ================================================================ */
/* 排序测试                                                          */
/* ================================================================ */
typedef void (*SortFn)(int *, size_t);

static void test_sorts(void)
{
    SECTION("sort");

    struct { const char *name; SortFn fn; } sorts[] = {
        {"bubble",    al_sort_bubble},
        {"insertion", al_sort_insertion},
        {"selection", al_sort_selection},
        {"shell",     al_sort_shell},
        {"quick",     al_sort_quick},
        {"merge",     al_sort_merge},
    };
    enum { NS = sizeof sorts / sizeof sorts[0] };

    const size_t N = 2000;
    int *base   = malloc(N * sizeof *base);
    int *expect = malloc(N * sizeof *expect);
    int *work   = malloc(N * sizeof *work);
    if (!base || !expect || !work) { exit(1); }

    /* 随机数据 */
    rng_seed(98765);
    for (size_t i = 0; i < N; i++) { base[i] = (int)(rng_next() % 100000u); }
    memcpy(expect, base, N * sizeof *expect);
    qsort(expect, N, sizeof *expect, cmp_int);

    for (size_t s = 0; s < NS; s++) {
        memcpy(work, base, N * sizeof *work);
        sorts[s].fn(work, N);
        CHECK(memcmp(work, expect, N * sizeof *work) == 0);
    }

    /* 已排序输入（快排的最坏情况候选） */
    for (size_t i = 0; i < N; i++) { base[i] = (int)i; }
    memcpy(expect, base, N * sizeof *expect);
    for (size_t s = 0; s < NS; s++) {
        memcpy(work, base, N * sizeof *work);
        sorts[s].fn(work, N);
        CHECK(memcmp(work, expect, N * sizeof *work) == 0);
    }

    /* 全部相等（Lomuto 快排的最坏情况） */
    for (size_t i = 0; i < N; i++) { base[i] = 42; }
    memcpy(expect, base, N * sizeof *expect);
    for (size_t s = 0; s < NS; s++) {
        memcpy(work, base, N * sizeof *work);
        sorts[s].fn(work, N);
        CHECK(memcmp(work, expect, N * sizeof *work) == 0);
    }

    /* 边界：n = 0、n = 1、NULL */
    for (size_t s = 0; s < NS; s++) {
        sorts[s].fn(NULL, 0);              /* 不应该崩溃 */
        int one = 7;
        sorts[s].fn(&one, 1);
        CHECK_EQ_INT(one, 7);
        int two[2] = {2, 1};
        sorts[s].fn(two, 2);
        CHECK(two[0] == 1 && two[1] == 2);
    }

    free(base); free(expect); free(work);
}

/* ================================================================ */
/* 搜索测试                                                          */
/* ================================================================ */
static void test_search(void)
{
    SECTION("search");

    /* 有重复值的数组：0,0,0,1,1,1,2,2,2,... */
    const size_t N = 3000;
    int *a = malloc(N * sizeof *a);
    if (a == NULL) { exit(1); }
    for (size_t i = 0; i < N; i++) { a[i] = (int)(i / 3); }

    CHECK_EQ_INT(al_linear_search(a, N, 0), 0);
    CHECK(al_linear_search(a, N, 999) >= 0);
    CHECK_EQ_INT(al_linear_search(a, N, -1), -1);
    CHECK_EQ_INT(al_linear_search(a, N, 99999), -1);

    /* 二分：返回任意一个匹配位置，必须确实是目标值 */
    long b = al_binary_search(a, N, 500);
    CHECK(b >= 0 && a[b] == 500);
    CHECK_EQ_INT(al_binary_search(a, N, -5), -1);
    CHECK_EQ_INT(al_binary_search(a, N, 100000), -1);
    CHECK_EQ_INT(al_binary_search(NULL, 0, 1), -1);

    /* lower/upper bound 的数学性质：对任意 target，a[lb-1] < t <= a[lb] */
    for (int t = -2; t <= 1002; t++) {
        size_t lb = al_lower_bound(a, N, t);
        size_t ub = al_upper_bound(a, N, t);
        CHECK(lb <= ub);
        if (lb > 0) { CHECK(a[lb - 1] < t); }
        if (lb < N) { CHECK(a[lb] >= t); }
        if (ub < N) { CHECK(a[ub] > t); }
    }

    /* 出现次数 = ub - lb，且和暴力计数一致 */
    for (int t = 0; t <= 1000; t += 137) {
        size_t lb = al_lower_bound(a, N, t);
        size_t ub = al_upper_bound(a, N, t);
        size_t brute = 0;
        for (size_t i = 0; i < N; i++) { if (a[i] == t) { brute++; } }
        CHECK_EQ_INT(ub - lb, brute);
    }

    /* 二分和线性对"存在性"的判断必须一致 */
    for (int t = -1; t <= 1001; t++) {
        bool lin = al_linear_search(a, N, t) >= 0;
        bool bin = al_binary_search(a, N, t) >= 0;
        CHECK(lin == bin);
    }

    free(a);
}

/* ================================================================ */
/* 动态数组测试                                                      */
/* ================================================================ */
static void test_vec(void)
{
    SECTION("vec");

    AlVec *v = al_vec_new(0);
    CHECK(v != NULL);
    CHECK_EQ_INT(al_vec_len(v), 0);
    CHECK_EQ_INT(al_vec_cap(v), 0);

    /* 扩容：push 1000 个，容量应该按翻倍增长 */
    for (int i = 0; i < 1000; i++) {
        CHECK(al_vec_push(v, i));
    }
    CHECK_EQ_INT(al_vec_len(v), 1000);
    CHECK(al_vec_cap(v) >= 1000);
    CHECK(al_vec_cap(v) < 2048);           /* 不应该浪费太多 */

    /* 值必须原样保留 */
    int out = 0;
    for (int i = 0; i < 1000; i++) {
        CHECK(al_vec_get(v, (size_t)i, &out));
        CHECK_EQ_INT(out, i);
    }

    /* 越界 */
    CHECK(!al_vec_get(v, 1000, &out));
    CHECK(!al_vec_get(v, (size_t)-1, &out));   /* 巨大的无符号值 */
    CHECK(!al_vec_set(v, 1000, 1));
    CHECK(!al_vec_get(v, 0, NULL));            /* NULL 出参 */

    /* set */
    CHECK(al_vec_set(v, 0, 999));
    CHECK(al_vec_get(v, 0, &out));
    CHECK_EQ_INT(out, 999);

    /* pop */
    CHECK(al_vec_pop(v, &out));
    CHECK_EQ_INT(out, 999);
    CHECK_EQ_INT(al_vec_len(v), 999);
    CHECK(!al_vec_get(v, 999, &out));          /* 刚 pop 掉的位置已经越界 */

    /* shrink */
    CHECK(al_vec_shrink(v));
    CHECK_EQ_INT(al_vec_cap(v), al_vec_len(v));

    /* 清空 */
    while (al_vec_pop(v, NULL)) { /* 一直弹到空 */ }
    CHECK_EQ_INT(al_vec_len(v), 0);

    al_vec_free(v);
    al_vec_free(NULL);                         /* 应该安全 */

    /* NULL 参数 */
    CHECK(!al_vec_push(NULL, 1));
    CHECK_EQ_INT(al_vec_len(NULL), 0);
    CHECK(!al_vec_shrink(NULL));

    /* 大数组压力测试：100 万个元素 */
    AlVec *big = al_vec_new(0);
    CHECK(big != NULL);
    for (int i = 0; i < 1000000; i++) {
        if (!al_vec_push(big, i)) { break; }
    }
    CHECK_EQ_INT(al_vec_len(big), 1000000);
    al_vec_free(big);
}

/* ================================================================ */
/* 链表测试                                                          */
/* ================================================================ */
static AlVec *list_snapshot(const AlList *l)
{
    return al_list_to_vec(l);
}

static void test_list(void)
{
    SECTION("list");

    AlList *l = al_list_new();
    CHECK(l != NULL);
    CHECK_EQ_INT(al_list_len(l), 0);

    /* 尾插 */
    for (int i = 1; i <= 5; i++) { CHECK(al_list_push_back(l, i)); }
    AlVec *s = list_snapshot(l);
    CHECK(s != NULL);
    CHECK_EQ_INT(al_vec_len(s), 5);
    for (int i = 0; i < 5; i++) {
        int out = 0;
        al_vec_get(s, (size_t)i, &out);
        CHECK_EQ_INT(out, i + 1);
    }
    al_vec_free(s);

    /* 头插 */
    CHECK(al_list_push_front(l, 0));
    s = list_snapshot(l);
    int out = 0;
    al_vec_get(s, 0, &out);
    CHECK_EQ_INT(out, 0);
    CHECK_EQ_INT(al_vec_len(s), 6);
    al_vec_free(s);

    /* 查找 */
    CHECK(al_list_find(l, 3));
    CHECK(!al_list_find(l, 99));

    /* 删除头节点（二级指针零特判的关键测试） */
    CHECK(al_list_remove(l, 0));
    CHECK(!al_list_find(l, 0));
    CHECK_EQ_INT(al_list_len(l), 5);

    /* 删除中间 */
    CHECK(al_list_remove(l, 3));
    CHECK(!al_list_find(l, 3));
    CHECK_EQ_INT(al_list_len(l), 4);

    /* 删除不存在的 */
    CHECK(!al_list_remove(l, 99));
    CHECK_EQ_INT(al_list_len(l), 4);

    /* 删除尾节点，并验证 tail 指针被正确更新 */
    CHECK(al_list_remove(l, 5));
    CHECK_EQ_INT(al_list_len(l), 3);
    CHECK(al_list_push_back(l, 100));       /* 如果 tail 没更新，这里可能出错 */
    CHECK(al_list_find(l, 100));
    s = list_snapshot(l);
    al_vec_get(s, al_vec_len(s) - 1, &out);
    CHECK_EQ_INT(out, 100);                 /* 100 应该真的在尾部 */
    al_vec_free(s);
    CHECK(al_list_remove(l, 100));

    /* 删除所有匹配 */
    for (int i = 0; i < 4; i++) { al_list_push_back(l, 7); }
    al_list_push_back(l, 8);
    CHECK_EQ_INT(al_list_remove_all(l, 7), 4);
    CHECK(!al_list_find(l, 7));
    CHECK(al_list_find(l, 8));

    /* 反转 */
    AlList *r = al_list_new();
    for (int i = 1; i <= 5; i++) { al_list_push_back(r, i); }
    al_list_reverse(r);
    s = list_snapshot(r);
    CHECK_EQ_INT(al_vec_len(s), 5);
    for (int i = 0; i < 5; i++) {
        al_vec_get(s, (size_t)i, &out);
        CHECK_EQ_INT(out, 5 - i);
    }
    al_vec_free(s);
    /* 反转后尾插仍要正确 */
    CHECK(al_list_push_back(r, 99));
    s = list_snapshot(r);
    al_vec_get(s, al_vec_len(s) - 1, &out);
    CHECK_EQ_INT(out, 99);
    al_vec_free(s);

    /* 空表反转不应崩溃 */
    AlList *empty = al_list_new();
    al_list_reverse(empty);
    CHECK_EQ_INT(al_list_len(empty), 0);
    al_list_free(empty);

    al_list_free(l);
    al_list_free(r);
    al_list_free(NULL);
    CHECK(!al_list_find(NULL, 1));
    CHECK(!al_list_push_back(NULL, 1));
    CHECK_EQ_INT(al_list_remove_all(NULL, 1), 0);
}

/* ================================================================ */
/* 哈希表测试                                                        */
/* ================================================================ */
static void test_hash(void)
{
    SECTION("hash");

    AlHash *h = al_hash_new();
    CHECK(h != NULL);
    CHECK_EQ_INT(al_hash_count(h), 0);

    /* 插入 5000 个 */
    const int N = 5000;
    char key[32];
    for (int i = 0; i < N; i++) {
        snprintf(key, sizeof key, "key%05d", i);
        CHECK(al_hash_put(h, key, i * 3));
    }
    CHECK_EQ_INT(al_hash_count(h), N);

    /* 负载因子不应该太高 */
    double load = (double)al_hash_count(h) / (double)al_hash_buckets(h);
    CHECK(load <= 0.75);

    /* 最长冲突链不应该太长 */
    CHECK(al_hash_max_chain(h) <= 12);

    /* 全部能取回来 */
    int out = 0, misses = 0;
    for (int i = 0; i < N; i++) {
        snprintf(key, sizeof key, "key%05d", i);
        if (!al_hash_get(h, key, &out) || out != i * 3) { misses++; }
    }
    CHECK_EQ_INT(misses, 0);

    /* 不存在的 key */
    CHECK(!al_hash_get(h, "no-such-key", &out));
    CHECK(!al_hash_get(h, "", &out));       /* 空字符串是合法的 key，只是没插入过 */

    /* 覆盖：count 不应该变 */
    CHECK(al_hash_put(h, "key00000", 777));
    CHECK_EQ_INT(al_hash_count(h), N);
    CHECK(al_hash_get(h, "key00000", &out));
    CHECK_EQ_INT(out, 777);

    /* 删除全部，并验证 */
    for (int i = 0; i < N; i++) {
        snprintf(key, sizeof key, "key%05d", i);
        CHECK(al_hash_del(h, key));
    }
    CHECK_EQ_INT(al_hash_count(h), 0);
    for (int i = 0; i < N; i++) {
        snprintf(key, sizeof key, "key%05d", i);
        CHECK(!al_hash_get(h, key, &out));
    }
    /* 重复删除应该失败 */
    CHECK(!al_hash_del(h, "key00000"));

    al_hash_free(h);
    al_hash_free(NULL);
    CHECK(!al_hash_put(NULL, "a", 1));
    CHECK(!al_hash_get(NULL, "a", NULL));
    CHECK_EQ_INT(al_hash_count(NULL), 0);
}

/* ================================================================ */
/* BST 测试                                                          */
/* ================================================================ */
static void test_tree(void)
{
    SECTION("tree");

    AlTree *t = al_tree_new();
    CHECK(t != NULL);
    CHECK_EQ_INT(al_tree_size(t), 0);
    CHECK(al_tree_is_valid(t));

    /* 插入 */
    int vals[] = {50, 30, 70, 20, 40, 60, 80, 10, 25, 35, 45};
    size_t n = sizeof vals / sizeof vals[0];
    for (size_t i = 0; i < n; i++) { CHECK(al_tree_insert(t, vals[i])); }
    CHECK_EQ_INT(al_tree_size(t), n);
    CHECK(al_tree_is_valid(t));

    /* 重复插入应该失败 */
    CHECK(!al_tree_insert(t, 50));
    CHECK_EQ_INT(al_tree_size(t), n);

    /* 查找 */
    for (size_t i = 0; i < n; i++) { CHECK(al_tree_contains(t, vals[i])); }
    CHECK(!al_tree_contains(t, 99));
    CHECK(!al_tree_contains(t, 5));

    /* 中序必须有序 */
    int buf[64];
    size_t got = al_tree_inorder(t, buf, 64);
    CHECK_EQ_INT(got, n);
    for (size_t i = 1; i < got; i++) { CHECK(buf[i - 1] < buf[i]); }

    /* 前序：第一个必须是根 */
    got = al_tree_preorder(t, buf, 64);
    CHECK_EQ_INT(got, n);
    CHECK_EQ_INT(buf[0], 50);

    /* 后序：最后一个必须是根 */
    got = al_tree_postorder(t, buf, 64);
    CHECK_EQ_INT(got, n);
    CHECK_EQ_INT(buf[n - 1], 50);

    /* 层序：第一个必须是根，且是 BFS 顺序 */
    got = al_tree_levelorder(t, buf, 64);
    CHECK_EQ_INT(got, n);
    CHECK_EQ_INT(buf[0], 50);       /* 第 0 层 */
    CHECK_EQ_INT(buf[1], 30);       /* 第 1 层 */
    CHECK_EQ_INT(buf[2], 70);

    /* 缓冲区太小应该安全截断 */
    got = al_tree_inorder(t, buf, 3);
    CHECK_EQ_INT(got, 3);
    got = al_tree_inorder(t, buf, 0);
    CHECK_EQ_INT(got, 0);

    /* 删除：三种情况 */
    CHECK(al_tree_delete(t, 10));           /* 叶子 */
    CHECK(!al_tree_contains(t, 10));
    CHECK(al_tree_is_valid(t));

    CHECK(al_tree_delete(t, 25));           /* 有一个孩子（无）... 换个有一个孩子的 */
    CHECK(al_tree_is_valid(t));

    CHECK(al_tree_delete(t, 30));           /* 有一个孩子或两个 */
    CHECK(!al_tree_contains(t, 30));
    CHECK(al_tree_is_valid(t));

    CHECK(al_tree_delete(t, 50));           /* 根节点，两个孩子 */
    CHECK(!al_tree_contains(t, 50));
    CHECK(al_tree_is_valid(t));
    CHECK_EQ_INT(al_tree_size(t), n - 4);

    /* 删除不存在的 */
    CHECK(!al_tree_delete(t, 999));

    /* 全部删空 */
    int remaining[64];
    got = al_tree_inorder(t, remaining, 64);
    for (size_t i = 0; i < got; i++) { CHECK(al_tree_delete(t, remaining[i])); }
    CHECK_EQ_INT(al_tree_size(t), 0);
    CHECK_EQ_INT(al_tree_height(t), 0);
    CHECK(al_tree_is_valid(t));

    al_tree_free(t);
    al_tree_free(NULL);
    CHECK(!al_tree_insert(NULL, 1));
    CHECK(!al_tree_contains(NULL, 1));
    CHECK_EQ_INT(al_tree_size(NULL), 0);
}

/* ================================================================ */
/* 集成测试：把各个数据结构串起来用                                     */
/* ================================================================ */
static void test_integration(void)
{
    SECTION("integration");

    /* 场景：读入一批数据 -> 去重 -> 排序 -> 存进哈希表建索引 -> 用 BST 统计 */

    /* 1. 用动态数组收集数据（含重复） */
    AlVec *raw = al_vec_new(0);
    CHECK(raw != NULL);
    rng_seed(4242);
    for (int i = 0; i < 5000; i++) {
        al_vec_push(raw, (int)(rng_next() % 1000u));
    }
    CHECK_EQ_INT(al_vec_len(raw), 5000);

    /* 2. 排序后去重 */
    al_sort_merge(al_vec_data(raw), al_vec_len(raw));
    AlVec *uniq = al_vec_new(0);
    CHECK(uniq != NULL);
    int prev = -1;
    for (size_t i = 0; i < al_vec_len(raw); i++) {
        int x = 0;
        al_vec_get(raw, i, &x);
        if (i == 0 || x != prev) { al_vec_push(uniq, x); prev = x; }
    }
    /* 验证去重结果确实严格递增 */
    bool sorted = true;
    for (size_t i = 1; i < al_vec_len(uniq); i++) {
        int a = 0, b = 0;
        al_vec_get(uniq, i - 1, &a);
        al_vec_get(uniq, i, &b);
        if (a >= b) { sorted = false; }
    }
    CHECK(sorted);

    /* 3. 建哈希索引：值 -> 在 uniq 中的下标 */
    AlHash *idx = al_hash_new();
    CHECK(idx != NULL);
    char key[32];
    for (size_t i = 0; i < al_vec_len(uniq); i++) {
        int x = 0;
        al_vec_get(uniq, i, &x);
        snprintf(key, sizeof key, "v%d", x);
        al_hash_put(idx, key, (int)i);
    }
    CHECK_EQ_INT(al_hash_count(idx), al_vec_len(uniq));

    /* 查回来验证 */
    int mismatches = 0;
    for (size_t i = 0; i < al_vec_len(uniq); i++) {
        int x = 0, pos = -1;
        al_vec_get(uniq, i, &x);
        snprintf(key, sizeof key, "v%d", x);
        if (!al_hash_get(idx, key, &pos) || pos != (int)i) { mismatches++; }
    }
    CHECK_EQ_INT(mismatches, 0);

    /* 4. 用链表存一份，再反转验证 */
    AlList *lst = al_list_new();
    for (size_t i = 0; i < al_vec_len(uniq); i++) {
        int x = 0;
        al_vec_get(uniq, i, &x);
        al_list_push_back(lst, x);
    }
    CHECK_EQ_INT(al_list_len(lst), al_vec_len(uniq));
    al_list_reverse(lst);
    AlVec *back = al_list_to_vec(lst);
    CHECK(back != NULL);
    CHECK_EQ_INT(al_vec_len(back), al_vec_len(uniq));
    /* 反转后应该和原数组逆序一致 */
    bool reversed_ok = true;
    for (size_t i = 0; i < al_vec_len(uniq); i++) {
        int a = 0, b = 0;
        al_vec_get(uniq, i, &a);
        al_vec_get(back, al_vec_len(back) - 1 - i, &b);
        if (a != b) { reversed_ok = false; }
    }
    CHECK(reversed_ok);

    /* 5. BST：插入所有唯一值，中序应该等于 uniq
     *
     * ⚠️ 这里顺便演示一个真实陷阱：
     *    uniq 是【已排序】的，按这个顺序插入 BST 会让树退化成链表！
     *    下面同时建两棵树对比：一棵有序插入，一棵打乱后插入。 */
    AlTree *tree = al_tree_new();
    AlTree *tree_shuffled = al_tree_new();
    for (size_t i = 0; i < al_vec_len(uniq); i++) {
        int x = 0;
        al_vec_get(uniq, i, &x);
        al_tree_insert(tree, x);              /* 有序插入 -> 退化 */
    }
    /* 打乱后再插入：Fisher-Yates 洗牌 */
    size_t m = al_vec_len(uniq);
    int *shuffled = malloc(m * sizeof *shuffled);
    CHECK(shuffled != NULL);
    for (size_t i = 0; i < m; i++) { al_vec_get(uniq, i, &shuffled[i]); }
    rng_seed(20240911);
    for (size_t i = m; i > 1; i--) {
        size_t j = rng_next() % i;
        int t = shuffled[i - 1];
        shuffled[i - 1] = shuffled[j];
        shuffled[j] = t;
    }
    for (size_t i = 0; i < m; i++) { al_tree_insert(tree_shuffled, shuffled[i]); }

    CHECK_EQ_INT(al_tree_size(tree), al_vec_len(uniq));
    CHECK(al_tree_is_valid(tree));
    CHECK_EQ_INT(al_tree_size(tree_shuffled), al_vec_len(uniq));
    CHECK(al_tree_is_valid(tree_shuffled));

    int *buf = malloc(al_vec_len(uniq) * sizeof *buf);
    CHECK(buf != NULL);
    size_t got = al_tree_inorder(tree, buf, al_vec_len(uniq));
    CHECK_EQ_INT(got, al_vec_len(uniq));
    bool same = true;
    for (size_t i = 0; i < got; i++) {
        int x = 0;
        al_vec_get(uniq, i, &x);
        if (buf[i] != x) { same = false; }
    }
    CHECK(same);

    printf("    处理了 5000 个随机数，去重后 %zu 个唯一值\n", al_vec_len(uniq));
    printf("    BST 树高（有序插入）= %d   <- 退化成链表！\n", al_tree_height(tree));
    printf("    BST 树高（打乱插入）= %d   <- 接近 O(log n)\n",
           al_tree_height(tree_shuffled));
    printf("    两者节点数相同（都是 %zu），但查找性能天差地别\n",
           al_tree_size(tree));
    printf("    哈希表桶数 = %zu，最长冲突链 = %zu\n",
           al_hash_buckets(idx), al_hash_max_chain(idx));

    /* 实测两种树的查找代价差异 */
    {
        volatile long long sink = 0;
        clock_t t0 = clock();
        for (int rep = 0; rep < 200000; rep++) {
            int x = 0;
            al_vec_get(uniq, (size_t)(rep % (int)m), &x);
            if (al_tree_contains(tree, x)) { sink++; }
        }
        clock_t t1 = clock();
        printf("    有序树 20 万次查找: %.3f ms\n",
               (double)(t1 - t0) * 1000.0 / (double)CLOCKS_PER_SEC);

        t0 = clock();
        for (int rep = 0; rep < 200000; rep++) {
            int x = 0;
            al_vec_get(uniq, (size_t)(rep % (int)m), &x);
            if (al_tree_contains(tree_shuffled, x)) { sink++; }
        }
        t1 = clock();
        printf("    打乱树 20 万次查找: %.3f ms\n",
               (double)(t1 - t0) * 1000.0 / (double)CLOCKS_PER_SEC);
        (void)sink;
    }

    free(shuffled);
    al_tree_free(tree_shuffled);

    free(buf);
    al_tree_free(tree);
    al_list_free(lst);
    al_hash_free(idx);
    al_vec_free(back);
    al_vec_free(uniq);
    al_vec_free(raw);
}

/* ================================================================ */
int main(void)
{
    puts("============ algolib 测试套件 ============\n");

    test_sorts();
    test_search();
    test_vec();
    test_list();
    test_hash();
    test_tree();
    test_integration();

    printf("\n============ 结果: %d passed, %d failed ============\n",
           g_pass, g_fail);
    return (g_fail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
