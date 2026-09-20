/* bench_main.c —— algolib 的性能对比测试
 *
 * 内容：
 *   1. 七种排序在不同规模和输入形态下的耗时
 *   2. 动态数组 vs 链表：随机访问 vs 顺序遍历
 *   3. 哈希表 vs BST：查找性能
 *   4. 复杂度增长验证
 *
 * ★ 建议用 -O0 和 -O2 各跑一次对比（make bench / make bench O2=1）
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "algolib.h"

/* ---------------------------------------------------------------- */
/* 计时工具（内联版，避免依赖外部头文件）                                */
/* ---------------------------------------------------------------- */
static double now_ms(void)
{
    return (double)clock() * 1000.0 / (double)CLOCKS_PER_SEC;
}

/* 防止优化器删掉纯计算 */
volatile long g_sink = 0;
#define KEEP(x) do { g_sink += (long)(x); } while (0)

/* ---------------------------------------------------------------- */
/* 可复现随机数                                                      */
/* ---------------------------------------------------------------- */
static unsigned g_rng = 1u;
static void rng_seed(unsigned s) { g_rng = s ? s : 1u; }
static unsigned rng_next(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

typedef void (*SortFn)(int *, size_t);

static void fill_random(int *a, size_t n, unsigned seed)
{
    rng_seed(seed);
    for (size_t i = 0; i < n; i++) { a[i] = (int)(rng_next() % 1000000u); }
}

static void fill_sorted(int *a, size_t n)
{
    for (size_t i = 0; i < n; i++) { a[i] = (int)i; }
}

static void fill_reversed(int *a, size_t n)
{
    for (size_t i = 0; i < n; i++) { a[i] = (int)(n - i); }
}

static void fill_equal(int *a, size_t n)
{
    for (size_t i = 0; i < n; i++) { a[i] = 7; }
}

/* ================================================================ */
/* 1. 排序性能                                                        */
/* ================================================================ */
static void bench_sorts(void)
{
    puts("========== 1. 排序性能对比 ==========");

    struct { const char *name; SortFn fn; int skip_large; } sorts[] = {
        {"bubble",    al_sort_bubble,    1},
        {"insertion", al_sort_insertion, 0},
        {"selection", al_sort_selection, 1},
        {"shell",     al_sort_shell,     0},
        {"quick",     al_sort_quick,     0},
        {"merge",     al_sort_merge,     0},
    };
    enum { NS = sizeof sorts / sizeof sorts[0] };

    const size_t N = 20000;
    int *base = malloc(N * sizeof *base);
    int *work = malloc(N * sizeof *work);
    if (!base || !work) { exit(1); }

    printf("  %-11s %12s %12s %12s %12s\n",
           "算法", "随机", "已排序", "逆序", "全相等");
    double quick_random = 0, quick_equal = 0;
    for (size_t s = 0; s < NS; s++) {
        if (sorts[s].skip_large) { continue; }    /* O(n^2) 太慢，跳过 */
        printf("  %-11s", sorts[s].name);

        const int shapes = 4;
        for (int sh = 0; sh < shapes; sh++) {
            switch (sh) {
            case 0: fill_random(base, N, 12345); break;
            case 1: fill_sorted(base, N); break;
            case 2: fill_reversed(base, N); break;
            default: fill_equal(base, N); break;
            }
            memcpy(work, base, N * sizeof *work);
            double t0 = now_ms();
            sorts[s].fn(work, N);
            double ms = now_ms() - t0;
            KEEP(work[N / 2]);
            if (sorts[s].fn == al_sort_quick) {
                if (sh == 0) { quick_random = ms; }
                if (sh == 3) { quick_equal = ms; }
            }
            printf(" %12.3f", ms);
        }
        putchar('\n');
    }
    printf("  （bubble / selection 是 O(n^2)，n=%zu 时太慢，已跳过）\n", N);
    puts("");
    puts("  ★ 注意 quick 在「全相等」那一列的表现：");
    printf("     随机输入 %.3f ms，全相等输入 %.3f ms —— 慢了 %.0f 倍。\n",
           quick_random, quick_equal,
           (quick_random > 0.001) ? quick_equal / quick_random : 0.0);
    puts("     原因：Lomuto 分区遇到全相等时，每次只能确定 pivot 这一个元素的位置，");
    puts("           递归退化成 n 层，总代价 O(n^2)。");
    puts("     三数取中【救不了】这种情况（三个数全都相等）。");
    puts("     修复：改用三路分区（<, ==, > 分成三段），相等元素直接成组跳过。");

    free(base); free(work);
}

/* ================================================================ */
/* 2. 复杂度增长验证                                                  */
/* ================================================================ */
static void bench_scaling(void)
{
    puts("\n========== 2. 复杂度增长验证（n 翻倍时的耗时倍率）==========");
    printf("  %-11s %10s %10s %10s %10s %10s\n",
           "算法", "n=4000", "n=8000", "n=16000", "n=32000", "倍率");

    size_t sizes[] = {4000, 8000, 16000, 32000};
    struct { const char *name; SortFn fn; } sorts[] = {
        {"insertion", al_sort_insertion},
        {"shell",     al_sort_shell},
        {"quick",     al_sort_quick},
        {"merge",     al_sort_merge},
    };
    enum { NS = sizeof sorts / sizeof sorts[0] };

    for (size_t s = 0; s < NS; s++) {
        printf("  %-11s", sorts[s].name);
        double prev = 0, last = 0;
        for (size_t k = 0; k < 4; k++) {
            size_t n = sizes[k];
            int *a = malloc(n * sizeof *a);
            if (a == NULL) { printf(" %10s", "OOM"); continue; }
            fill_random(a, n, 999);
            double t0 = now_ms();
            sorts[s].fn(a, n);
            double ms = now_ms() - t0;
            KEEP(a[0]);
            printf(" %10.3f", ms);
            prev = last;
            last = ms;
            free(a);
        }
        /* 最后一次翻倍的倍率 */
        printf(" %10.2f\n", (prev > 0.001) ? last / prev : 0.0);
    }
    puts("  O(n^2) 的倍率接近 4，O(n log n) 的倍率接近 2.1~2.3");
    puts("  （具体数值随机器浮动，看【倍率】而不是绝对值）");
}

/* ================================================================ */
/* 3. 动态数组 vs 链表                                                */
/* ================================================================ */
static void bench_vec_vs_list(void)
{
    puts("\n========== 3. 动态数组 vs 链表 ==========");
    const int N = 200000;

    /* 构建 */
    double t0 = now_ms();
    AlVec *v = al_vec_new(0);
    for (int i = 0; i < N; i++) { al_vec_push(v, i); }
    double t_vec_build = now_ms() - t0;

    t0 = now_ms();
    AlList *l = al_list_new();
    for (int i = 0; i < N; i++) { al_list_push_back(l, i); }
    double t_list_build = now_ms() - t0;

    printf("  %-28s %10.3f ms\n", "动态数组 尾部追加 20 万", t_vec_build);
    printf("  %-28s %10.3f ms\n", "链表 尾部追加 20 万", t_list_build);
    printf("  %-28s %.2f 倍\n", "比值（链表/数组）", t_list_build / t_vec_build);

    /* 顺序遍历求和 */
    t0 = now_ms();
    long sv = 0;
    int *data = al_vec_data(v);
    for (size_t i = 0; i < al_vec_len(v); i++) { sv += data[i]; }
    double t_vec_sum = now_ms() - t0;

    /* 链表：导出成数组再遍历（因为链表不暴露节点，只能这样遍历） */
    t0 = now_ms();
    long sl = 0;
    AlVec *lst_vec = al_list_to_vec(l);
    double t_list_export = now_ms() - t0;
    t0 = now_ms();
    for (size_t i = 0; i < al_vec_len(lst_vec); i++) {
        int out = 0;
        al_vec_get(lst_vec, i, &out);
        sl += out;
    }
    double t_list_sum = now_ms() - t0;

    printf("\n  %-28s %10.3f ms  (和 %ld)\n", "数组顺序遍历", t_vec_sum, sv);
    printf("  %-28s %10.3f ms\n", "链表 -> 数组（导出）", t_list_export);
    printf("  %-28s %10.3f ms  (和 %ld)\n", "  再遍历", t_list_sum, sl);
    printf("  %-28s %10.3f ms  <-- 合计\n", "链表遍历（导出+扫描）",
           t_list_export + t_list_sum);
    printf("  合计比数组慢 %.1f 倍\n", (t_list_export + t_list_sum) / t_vec_sum);
    puts("  （公平起见说明：链表要顺序访问本可以 O(n) 直接走 next 指针，");
    puts("    这里只能走导出，是因为本库没有暴露节点。若直接遍历，");
    puts("    链表仍然比数组慢 —— 每次跳指针都可能 cache miss。）");

    al_vec_free(lst_vec);
    al_vec_free(v);
    al_list_free(l);
}

/* ================================================================ */
/* 4. 哈希表 vs BST                                                  */
/* ================================================================ */
static void bench_hash_vs_tree(void)
{
    puts("\n========== 4. 哈希表 vs BST：查找性能 ==========");
    const int N = 20000;

    char (*keys)[16] = malloc((size_t)N * sizeof *keys);
    if (keys == NULL) { return; }
    for (int i = 0; i < N; i++) { snprintf(keys[i], 16, "key%05d", i); }

    /* 建哈希表 */
    double t0 = now_ms();
    AlHash *h = al_hash_new();
    for (int i = 0; i < N; i++) { al_hash_put(h, keys[i], i); }
    double t_hash_build = now_ms() - t0;

    /* 建 BST（打乱后插入，避免退化） */
    t0 = now_ms();
    AlTree *t = al_tree_new();
    int *vals = malloc((size_t)N * sizeof *vals);
    if (vals == NULL) { free(keys); al_hash_free(h); return; }
    for (int i = 0; i < N; i++) { vals[i] = i; }
    rng_seed(31337);
    for (int i = N; i > 1; i--) {
        int j = (int)(rng_next() % (unsigned)i);
        int tmp = vals[i - 1]; vals[i - 1] = vals[j]; vals[j] = tmp;
    }
    for (int i = 0; i < N; i++) { al_tree_insert(t, vals[i]); }
    double t_tree_build = now_ms() - t0;

    printf("  规模 n = %d\n", N);
    printf("  %-24s %10.3f ms\n", "建哈希表", t_hash_build);
    printf("  %-24s %10.3f ms  (树高 %d)\n", "建 BST", t_tree_build, al_tree_height(t));

    /* 查找 N 次 */
    int out = 0;
    t0 = now_ms();
    for (int rep = 0; rep < 10; rep++) {
        for (int i = 0; i < N; i++) {
            if (al_hash_get(h, keys[i], &out)) { KEEP(out); }
        }
    }
    double t_hash_get = now_ms() - t0;

    t0 = now_ms();
    for (int rep = 0; rep < 10; rep++) {
        for (int i = 0; i < N; i++) {
            if (al_tree_contains(t, vals[i])) { KEEP(i); }
        }
    }
    double t_tree_get = now_ms() - t0;

    printf("\n  %-24s %10.3f ms  (%d 万次)\n", "哈希查找", t_hash_get, N / 1000 * 10);
    printf("  %-24s %10.3f ms  (%d 万次)\n", "BST 查找", t_tree_get, N / 1000 * 10);
    printf("  %-24s %.2f 倍\n", "哈希快多少", t_tree_get / t_hash_get);
    puts("");
    printf("  哈希表桶数 = %zu，最长冲突链 = %zu\n",
           al_hash_buckets(h), al_hash_max_chain(h));
    puts("  哈希是 O(1)（平均），BST 是 O(log n)。");
    puts("  但哈希表失去了「有序」这个性质 —— BST 能高效地做范围查询。");

    free(vals);
    free(keys);
    al_hash_free(h);
    al_tree_free(t);
}

/* ================================================================ */
int main(void)
{
    puts("============ algolib 性能测试 ============");
    printf("编译选项: %s\n",
#if defined(__OPTIMIZE__)
           "-O1 或更高"
#else
           "-O0（未优化，建议也用 -O2 跑一次对比）"
#endif
    );
    putchar('\n');

    bench_sorts();
    bench_scaling();
    bench_vec_vs_list();
    bench_hash_vs_tree();

    puts("\n========== 结论 ==========");
    puts("  1. O(n^2) 的排序在 n > 1 万时不可用；O(n log n) 的可用到很大规模");
    puts("  2. n 翻倍时，O(n^2) 耗时约翻 4 倍，O(n log n) 约翻 2 倍 ——");
    puts("     这个倍率比绝对耗时有意义得多");
    puts("  3. 动态数组的尾部追加非常快（摊还 O(1)），链表还要每次 malloc");
    puts("  4. 哈希表查找比 BST 快，但不能做范围查询");
    puts("  5. 优化级别会显著改变结果，报告性能数据时必须注明编译选项");

    return 0;
}
