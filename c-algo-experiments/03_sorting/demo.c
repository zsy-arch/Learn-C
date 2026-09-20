/* demo.c —— 六种排序：正确性、稳定性、比较/交换次数、真实耗时
 *
 * 本实验要回答新手的几个真实困惑：
 *   1. 冒泡 / 选择 / 插入 都是 O(n^2)，为什么插入排序实际上快那么多？
 *   2. 归并排序「稳定」到底是什么意思？怎么用代码证明？
 *   3. 快排的「最坏情况」是真实存在的吗？（答案：是，而且很容易踩到）
 *   4. 小数据量下，O(n^2) 会不会比 O(n log n) 快？
 */
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "sorts.h"
#include "../timing.h"

/* bench.c 里定义 g_bench_sink；sorts.h 里声明 g_stats */
#include "../bench.c"

Stats g_stats;

/* ---------------------------------------------------------------- */
/* 测试辅助                                                            */
/* ---------------------------------------------------------------- */
typedef void (*SortFn)(int *a, size_t n);

typedef struct {
    const char *name;
    SortFn      fn;
} SortEntry;

static const SortEntry g_sorts[] = {
    {"bubble",    sort_bubble},
    {"selection", sort_selection},
    {"insertion", sort_insertion},
    {"shell",     sort_shell},
    {"quick",     sort_quick},
    {"merge",     sort_merge},
};
#define NSORTS (sizeof g_sorts / sizeof g_sorts[0])

static void fill_random(int *a, size_t n, unsigned seed)
{
    /* 自己实现的 xorshift：不依赖 rand() 的平台差异，结果可复现 */
    unsigned x = seed ? seed : 1u;
    for (size_t i = 0; i < n; i++) {
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        a[i] = (int)(x % 100000u);
    }
}

static void fill_sorted(int *a, size_t n)
{
    for (size_t i = 0; i < n; i++) { a[i] = (int)i; }
}

static void fill_reversed(int *a, size_t n)
{
    for (size_t i = 0; i < n; i++) { a[i] = (int)(n - i); }
}

static void fill_all_equal(int *a, size_t n)
{
    for (size_t i = 0; i < n; i++) { a[i] = 42; }
}

static bool is_sorted(const int *a, size_t n)
{
    for (size_t i = 1; i < n; i++) {
        if (a[i - 1] > a[i]) { return false; }
    }
    return true;
}

static bool same_array(const int *a, const int *b, size_t n)
{
    return memcmp(a, b, n * sizeof *a) == 0;
}

/* ---------------------------------------------------------------- */
/* 1. 正确性：六种算法结果必须完全一致                                   */
/* ---------------------------------------------------------------- */
static void test_correctness(void)
{
    puts("========== 1. 正确性交叉验证 ==========");
    const size_t n = 200;
    int *base   = malloc(n * sizeof *base);
    int *expect = malloc(n * sizeof *expect);
    int *work   = malloc(n * sizeof *work);
    if (!base || !expect || !work) { exit(1); }

    fill_random(base, n, 12345);
    memcpy(expect, base, n * sizeof *base);
    stats_reset();
    sort_insertion(expect, n);        /* 插入排序最简单，容易验证正确性，当参照答案 */

    printf("  参照答案（插入排序）: ");
    for (size_t i = 0; i < 10; i++) { printf("%d ", expect[i]); }
    printf("... (共 %zu 个元素)\n", n);

    int all_ok = 1;
    for (size_t s = 0; s < NSORTS; s++) {
        memcpy(work, base, n * sizeof *work);
        stats_reset();
        g_sorts[s].fn(work, n);
        bool sorted = is_sorted(work, n);
        bool match  = same_array(work, expect, n);
        printf("  %-10s 有序=%-3s 与参照一致=%-3s  比较=%8llu 交换=%8llu\n",
               g_sorts[s].name, sorted ? "是" : "否", match ? "是" : "否",
               g_stats.cmp, g_stats.swap);
        if (!sorted || !match) { all_ok = 0; }
    }
    printf("  结论: %s\n", all_ok ? "全部正确" : "有算法出错！");

    free(base); free(expect); free(work);
}

/* ---------------------------------------------------------------- */
/* 2. 稳定性                                                          */
/* ---------------------------------------------------------------- */
/*
 * 稳定性定义：值相等的元素，排序后相对顺序不变。
 *
 * 【重要】测试稳定性必须满足两个条件，缺一不可：
 *   (a) 存在真正相等的元素（比较时「看不出区别」）
 *   (b) 元素身上带着能区分原始顺序的额外信息
 *
 * 一个常见错误做法是把 key 和 id 编码进一个整数（key*1000+id）：
 * 那样比较的是整个编码值，id 也参与了比较，所有元素互不相等，
 * 任何算法看起来都「稳定」—— 这个测试就没意义了。
 *
 * 正确做法：用 struct 保存 (key, id)，比较时【只看 key】。
 */
typedef struct { int key; int id; } Pair;

static int pair_cmp(const Pair *x, const Pair *y)
{
    return (x->key > y->key) - (x->key < y->key);   /* 只看 key */
}

static void pair_swap(Pair *a, Pair *b) { Pair t = *a; *a = *b; *b = t; }

static void pair_insertion(Pair *a, size_t n)
{
    for (size_t i = 1; i < n; i++) {
        Pair key = a[i];
        size_t j = i;
        while (j > 0 && pair_cmp(&a[j - 1], &key) > 0) { a[j] = a[j - 1]; j--; }
        a[j] = key;
    }
}

static void pair_selection(Pair *a, size_t n)
{
    for (size_t i = 0; i + 1 < n; i++) {
        size_t m = i;
        for (size_t j = i + 1; j < n; j++) {
            if (pair_cmp(&a[j], &a[m]) < 0) { m = j; }
        }
        if (m != i) { pair_swap(&a[i], &a[m]); }
    }
}

static void pair_merge_rec(Pair *a, Pair *tmp, size_t lo, size_t hi)
{
    if (lo >= hi) { return; }
    size_t mid = lo + (hi - lo) / 2;
    pair_merge_rec(a, tmp, lo, mid);
    pair_merge_rec(a, tmp, mid + 1, hi);

    size_t i = lo, j = mid + 1, k = lo;
    while (i <= mid && j <= hi) {
        /* <= 保证稳定：相等时先取左半边的 */
        if (pair_cmp(&a[i], &a[j]) <= 0) { tmp[k++] = a[i++]; }
        else                             { tmp[k++] = a[j++]; }
    }
    while (i <= mid) { tmp[k++] = a[i++]; }
    while (j <= hi)  { tmp[k++] = a[j++]; }
    for (size_t t = lo; t <= hi; t++) { a[t] = tmp[t]; }
}

static void pair_merge(Pair *a, size_t n)
{
    Pair *tmp = malloc(n * sizeof *tmp);
    if (tmp == NULL) { return; }
    pair_merge_rec(a, tmp, 0, n - 1);
    free(tmp);
}

static size_t pair_partition(Pair *a, size_t lo, size_t hi)
{
    Pair pivot = a[hi];
    size_t i = lo;
    for (size_t j = lo; j < hi; j++) {
        if (pair_cmp(&a[j], &pivot) < 0) { pair_swap(&a[i], &a[j]); i++; }
    }
    pair_swap(&a[i], &a[hi]);
    return i;
}

static void pair_quick_rec(Pair *a, size_t lo, size_t hi)
{
    if (lo >= hi) { return; }
    size_t p = pair_partition(a, lo, hi);
    if (p > lo)     { pair_quick_rec(a, lo, p - 1); }
    if (p + 1 < hi) { pair_quick_rec(a, p + 1, hi); }
}

static void pair_quick(Pair *a, size_t n)
{
    if (n < 2) { return; }
    pair_quick_rec(a, 0, n - 1);
}

static void pair_bubble(Pair *a, size_t n)
{
    for (size_t i = 0; i + 1 < n; i++) {
        int swapped = 0;
        for (size_t j = 0; j + 1 < n - i; j++) {
            if (pair_cmp(&a[j], &a[j + 1]) > 0) { pair_swap(&a[j], &a[j + 1]); swapped = 1; }
        }
        if (!swapped) { break; }
    }
}

static void print_pairs(const char *tag, const Pair *a, size_t n)
{
    printf("  %-14s ", tag);
    for (size_t i = 0; i < n; i++) {
        printf("%d#%d ", a[i].key, a[i].id);
    }
    putchar('\n');
}

static void test_stability_direct(void)
{
    puts("\n========== 2. 稳定性：逐算法实测 ==========");

    /* ---- 第一组：4 个 key，每个出现 5 次，key 组内 id 递减 ---- */
    enum { NK = 4, NPER = 5, N = NK * NPER };
    Pair original[N];
    for (int k = 0; k < NK; k++) {
        for (int j = 0; j < NPER; j++) {
            original[k * NPER + j].key = k;
            original[k * NPER + j].id  = NPER - 1 - j;   /* 4 3 2 1 0 */
        }
    }

    struct { const char *name; void (*fn)(Pair *, size_t); } sorts[] = {
        {"bubble",    pair_bubble},
        {"selection", pair_selection},
        {"insertion", pair_insertion},
        {"quick",     pair_quick},
        {"merge",     pair_merge},
    };
    enum { NS = sizeof sorts / sizeof sorts[0] };

    print_pairs("原始 (key#id)", original, N);

    puts("");
    for (size_t s = 0; s < NS; s++) {
        Pair a[N];
        memcpy(a, original, sizeof a);
        sorts[s].fn(a, N);

        /* 稳定性判据：同一 key 组内，id 必须保持原来的递减顺序 */
        int stable = 1;
        for (int i = 1; i < N; i++) {
            if (a[i].key == a[i - 1].key && a[i].id > a[i - 1].id) {
                stable = 0;
                break;
            }
        }
        printf("  %-10s %-6s  ", sorts[s].name, stable ? "稳定" : "不稳定");
        for (int i = 0; i < N; i++) { printf("%d#%d ", a[i].key, a[i].id); }
        putchar('\n');
    }

    /* ---- 第二组：最小反例 ----
     * 上面那组数据太规律，选择排序「凑巧」保持了稳定。
     * 三元素反例才是它不稳定的最小证明。 */
    puts("\n  ---- 选择排序的最小反例（3 个元素）----");
    {
        Pair a[3] = {{2, 0}, {2, 1}, {1, 2}};
        print_pairs("原始", a, 3);
        pair_selection(a, 3);
        print_pairs("选择排序后", a, 3);
        puts("                   ^ 两个 key=2 的元素，id 从 0,1 变成了 1,0 —— 顺序被交换了！");
        puts("  原因：第一轮找到最小值 1（在下标 2），把它和下标 0 交换。");
        puts("        下标 0 上的 2#0 被扔到了下标 2，跨过了 2#1。");
    }

    puts("\n  规律：");
    puts("    - 只比较/移动相邻元素的算法（冒泡、插入）天然稳定");
    puts("    - 归并在 merge 时用 <= 保证稳定；写成 < 就变成不稳定");
    puts("    - 选择排序的「交换到 i 位置」跨越中间元素，破坏稳定性");
    puts("    - 快排的 Lomuto 分区同样跨越式交换，不稳定");
    puts("    - 希尔排序跨 gap 交换，也不稳定");
}

/* ---------------------------------------------------------------- */
/* 3. 比较/交换次数                                                   */
/* ---------------------------------------------------------------- */
static void test_opcounts(void)
{
    puts("\n========== 3. 操作次数对比（n = 2000，随机）==========");
    const size_t n = 2000;
    int *base = malloc(n * sizeof *base);
    int *work = malloc(n * sizeof *work);
    if (!base || !work) { exit(1); }
    fill_random(base, n, 7);

    printf("  %-10s %12s %12s %8s\n", "算法", "比较次数", "移动次数", "轮数");
    for (size_t s = 0; s < NSORTS; s++) {
        memcpy(work, base, n * sizeof *work);
        stats_reset();
        g_sorts[s].fn(work, n);
        printf("  %-10s %12llu %12llu %8llu\n",
               g_sorts[s].name, g_stats.cmp, g_stats.swap, g_stats.passes);
    }
    printf("  n*log2(n) ≈ %.0f,  n^2 = %zu\n", (double)n * 11.0, n * n);
    puts("  注意 selection：比较次数最多（n^2/2），但【移动次数最少】（n-1 次交换）。");
    puts("  如果「移动元素」比「比较元素」贵得多（比如元素很大），选择排序反而有优势。");
    free(base); free(work);
}

/* ---------------------------------------------------------------- */
/* 4. 不同输入形态下各算法的表现                                        */
/* ---------------------------------------------------------------- */
static void test_input_shapes(void)
{
    puts("\n========== 4. 输入形态对算法的影响（n = 5000）==========");
    const size_t n = 5000;
    int *base = malloc(n * sizeof *base);
    int *work = malloc(n * sizeof *work);
    if (!base || !work) { exit(1); }

    struct { const char *name; void (*fill)(int *, size_t); } shapes[] = {
        {"随机",     NULL},
        {"已排序",   fill_sorted},
        {"逆序",     fill_reversed},
        {"全部相等", fill_all_equal},
    };

    printf("  %-10s %-9s %12s %10s\n", "算法", "输入", "比较次数", "耗时(ms)");
    for (size_t sh = 0; sh < sizeof shapes / sizeof shapes[0]; sh++) {
        if (shapes[sh].fill == NULL) { fill_random(base, n, 99); }
        else { shapes[sh].fill(base, n); }

        for (size_t s = 0; s < NSORTS; s++) {
            memcpy(work, base, n * sizeof *work);
            stats_reset();
            Timer t = timer_start("");
            g_sorts[s].fn(work, n);
            double ms = timer_stop(t);
            if (!is_sorted(work, n)) { printf("  排序失败!\n"); }
            printf("  %-10s %-9s %12llu %10.3f\n",
                   g_sorts[s].name, shapes[sh].name, g_stats.cmp, ms);
        }
        putchar('\n');
    }
    puts("  ★ 三个值得注意的地方：");
    puts("    1. bubble / insertion 在「已排序」输入下只比较 n-1 次 —— O(n)");
    puts("       （bubble 靠 !swapped 提前退出；insertion 的内层 while 一次都不进）");
    puts("    2. selection 在「已排序」下依然比较 n^2/2 次 —— 它从不提前退出");
    puts("    3. quick 在「全部相等」下比较次数接近 n^2/2！");
    puts("       Lomuto 分区遇到全相等时每次只确定 1 个元素的位置，退化成 O(n^2)。");
    puts("       修复方案：三路分区（<, ==, > 分成三段），见本章练习 3。");
    free(base); free(work);
}

/* ---------------------------------------------------------------- */
/* 5. 规模对比：验证 O(n log n) 和 O(n^2) 的实际差距                     */
/* ---------------------------------------------------------------- */
static void test_scaling(void)
{
    puts("\n========== 5. 规模增长测试 ==========");
    printf("  %-10s %12s %12s %12s\n", "算法", "n=2000", "n=4000", "n=8000");

    size_t sizes[] = {2000, 4000, 8000};
    for (size_t s = 0; s < NSORTS; s++) {
        if (strcmp(g_sorts[s].name, "selection") == 0) { continue; }  /* 太慢，跳过 */
        if (strcmp(g_sorts[s].name, "bubble") == 0)    { continue; }

        printf("  %-10s", g_sorts[s].name);
        for (size_t k = 0; k < 3; k++) {
            size_t n = sizes[k];
            int *a = malloc(n * sizeof *a);
            if (a == NULL) { printf(" %12s", "OOM"); continue; }
            fill_random(a, n, 3);
            stats_reset();
            Timer t = timer_start("");
            g_sorts[s].fn(a, n);
            double ms = timer_stop(t);
            printf(" %12.3f", ms);
            free(a);
        }
        putchar('\n');
    }
    puts("  注意观察：n 翻倍时，O(n log n) 的耗时约翻 2 倍多一点，");
    puts("            O(n^2) 的耗时约翻 4 倍。");
    puts("  （上面的 insertion 行：n 每翻一倍耗时约翻 4 倍 —— O(n^2) 特征）");
    puts("  （quick / merge / shell 行：约翻 2.0~2.3 倍 —— O(n log n) 特征）");
    puts("  注：具体数值每次运行略有浮动，看【倍率】而不是绝对值。");
}

/* ---------------------------------------------------------------- */
/* 6. 小数组：谁最快？                                                 */
/* ---------------------------------------------------------------- */
static void test_small_what(void)
{
    puts("\n========== 6. 小规模对比：n = 32 时谁最快？==========");
    puts("  （单次排序太快，clock() 精度不够；改为跑 20000 次取平均）");
    const size_t n = 32;
    enum { REPS = 20000 };
    int *base = malloc(n * sizeof *base);
    int *work = malloc(n * sizeof *work);
    if (!base || !work) { exit(1); }
    fill_random(base, n, 5);

    printf("  %-10s %14s %14s\n", "算法", "总耗时(ms)", "单次(ns)");
    for (size_t s = 0; s < NSORTS; s++) {
        stats_reset();
        Timer t = timer_start("");
        for (int rep = 0; rep < REPS; rep++) {
            memcpy(work, base, n * sizeof *work);
            g_sorts[s].fn(work, n);
        }
        double ms = timer_stop(t);
        printf("  %-10s %14.3f %14.1f\n",
               g_sorts[s].name, ms, ms * 1e6 / (double)REPS);
    }
    puts("  n 很小时各算法差别不大，甚至插入排序可能更快");
    puts("  （常数因子小、无递归开销、对 cache 友好）");
    puts("  -> 这解释了为什么标准库的排序在递归到小区间时，会切换成插入排序。");
    free(base); free(work);
}

int main(void)
{
    puts("================ 排序算法实验 ================\n");
    test_correctness();
    test_stability_direct();
    test_opcounts();
    test_input_shapes();
    test_scaling();
    test_small_what();
    return 0;
}
