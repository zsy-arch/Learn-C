/* demo.c —— 搜索：线性搜索、二分搜索、边界变体、mid 溢出
 *
 * 本实验要回答新手最常问的三个问题：
 *   1. 为什么我的二分查找会死循环？
 *   2. 为什么写完二分查找，总是差一个边界（要么多一个要么少一个）？
 *   3. 教科书说 mid = (lo + hi) / 2 会溢出 —— 这是真的吗？怎么复现？
 */
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

#include "../timing.h"
#include "../bench.c"

static double log2_approx(size_t n);   /* 前向声明 */

static void fill_sorted(int *a, size_t n, int start, int step)
{
    for (size_t i = 0; i < n; i++) { a[i] = start + (int)i * step; }
}

/* ---------------------------------------------------------------- */
/* 搜索次数计数器：用来「看见」二分查找到底比较了几次                     */
/* ---------------------------------------------------------------- */
static unsigned long long g_probes = 0;

/* ---------------------------------------------------------------- */
/* 1. 线性搜索 —— O(n)                                                 */
/* ---------------------------------------------------------------- */
static long linear_search(const int *a, size_t n, int target)
{
    for (size_t i = 0; i < n; i++) {
        g_probes++;
        if (a[i] == target) { return (long)i; }
    }
    return -1;
}

/* ---------------------------------------------------------------- */
/* 2. 经典二分查找 —— 找「任意一个」匹配                                 */
/* ---------------------------------------------------------------- */
/*
 * 循环不变式：答案若存在，一定在闭区间 [lo, hi] 内。
 * 因为 hi 是闭的，所以初始 hi = n - 1。
 */
static long binary_search_classic(const int *a, size_t n, int target)
{
    if (a == NULL || n == 0) { return -1; }
    size_t lo = 0;
    size_t hi = n - 1;                 /* 闭区间 [lo, hi] */

    while (lo <= hi) {
        g_probes++;
        size_t mid = lo + (hi - lo) / 2;   /* 不写 (lo+hi)/2，避免溢出 */
        if (a[mid] == target) { return (long)mid; }
        if (a[mid] < target) {
            lo = mid + 1;              /* 答案在 [mid+1, hi] */
        } else {
            if (mid == 0) { break; }   /* 防 size_t 下溢 */
            hi = mid - 1;              /* 答案在 [lo, mid-1] */
        }
    }
    return -1;
}

/* ---------------------------------------------------------------- */
/* 3. 下界 / 上界 —— 处理重复元素的标准工具                              */
/* ---------------------------------------------------------------- */
/*
 * lower_bound: 第一个 >= target 的位置（可能不存在，则返回 n）
 * upper_bound: 第一个 >  target 的位置（可能不存在，则返回 n）
 *
 * 这两个函数组合起来可以回答：
 *   - target 出现了几次？   upper_bound - lower_bound
 *   - target 是否存在？     lower_bound < n && a[lower_bound] == target
 *
 * 它们都用【半开区间 [lo, hi)】，这是写对边界的关键。
 */
static size_t lower_bound(const int *a, size_t n, int target)
{
    size_t lo = 0, hi = n;             /* 半开区间 [lo, hi) */
    while (lo < hi) {
        g_probes++;
        size_t mid = lo + (hi - lo) / 2;
        if (a[mid] < target) {
            lo = mid + 1;              /* 答案在 [mid+1, hi) */
        } else {
            hi = mid;                  /* a[mid] >= target，答案在 [lo, mid) */
        }
    }
    return lo;                         /* lo == hi，第一个 >= target 的位置 */
}

static size_t upper_bound(const int *a, size_t n, int target)
{
    size_t lo = 0, hi = n;
    while (lo < hi) {
        g_probes++;
        size_t mid = lo + (hi - lo) / 2;
        if (a[mid] <= target) {        /* 唯一的区别：<= 而不是 < */
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

/* ---------------------------------------------------------------- */
/* 4. 反例：会死循环的二分查找                                          */
/* ---------------------------------------------------------------- */
/*
 * 【这是错误示例，只用来演示】
 *
 * 两个经典错误：
 *   a) 闭区间语义下 hi = mid（应该是 mid - 1），区间不收缩
 *   b) lo = mid 而不是 mid + 1，当 hi == lo+1 时 mid == lo，区间不收缩
 */
static long binary_search_deadloop(const int *a, size_t n, int target, int *guard)
{
    if (a == NULL || n == 0) { return -1; }
    size_t lo = 0;
    size_t hi = n - 1;
    *guard = 0;

    while (lo <= hi) {
        if (++(*guard) > 100) {        /* 安全阀：否则这个函数永远不会返回 */
            return -2;                 /* -2 表示「检测到死循环」 */
        }
        size_t mid = (lo + hi) / 2;
        if (a[mid] == target) { return (long)mid; }
        if (a[mid] < target) {
            lo = mid;                  /* ❌ 错误！应该是 mid + 1 */
        } else {
            hi = mid;                  /* ❌ 错误！闭区间下应该是 mid - 1 */
        }
    }
    return -1;
}

/* ---------------------------------------------------------------- */
/* 5. mid 溢出的真实复现                                               */
/* ---------------------------------------------------------------- */
/*
 * 教科书上的说法：mid = (lo + hi) / 2 在两个大下标相加时会溢出。
 *
 * 这个 bug 真实存在于 JDK 的 java.util.Arrays.binarySearch 里，
 * 从 2006 年一直留到 2015 年（近 10 年）才被修复。
 *
 * 怎么复现？数组必须足够大，让 lo + hi > SIZE_MAX。
 * n 个元素需要 4n 字节内存，SIZE_MAX ≈ 1.8e19 —— 不可能真的分配出来。
 *
 * 所以下面用两种「不用真的分配」的方式复现。
 */
static void demo_mid_overflow(void)
{
    puts("========== 5. mid = (lo + hi) / 2 的溢出 ==========");

    /* (a) 算术层面的复现：不需要真的分配内存 */
    {
        size_t lo = SIZE_MAX - 2;
        size_t hi = SIZE_MAX - 1;
        size_t bad = (lo + hi) / 2;
        size_t good = lo + (hi - lo) / 2;
        printf("  假设 lo = %zu, hi = %zu （SIZE_MAX = %zu）\n",
               lo, hi, (size_t)SIZE_MAX);
        printf("    (lo + hi) / 2      = %zu   <-- 回绕了！结果比 lo 还小\n", bad);
        printf("    lo + (hi - lo) / 2 = %zu   <-- 正确\n", good);
        printf("    正确的 mid 落在 [lo, hi] 之间：%s\n",
               (good >= lo && good <= hi) ? "是" : "否");
        printf("    错误的 mid 落在 [lo, hi] 之间：%s\n",
               (bad >= lo && bad <= hi) ? "是" : "否");
        printf("    （错误的 mid 比 lo 小了 %zu —— 直接越界到下界之外）\n",
               lo - bad);
    }

    /* (b) 用 uint8_t 在小范围上演示同样的形状。
     *
     * 注意这里必须先把加法结果截断到 uint8_t 再除，才是真实语义：
     * 真实的 C 代码里 mid 声明成同样宽度的类型，加法就在那个宽度上回绕。
     * 如果写成 (lo + hi) / 2 而 lo/hi 是 uint8_t，它们会先【整型提升】到 int，
     * 加法不会回绕 —— 那就重现不出 bug 了，千万注意这一点。 */
    {
        uint8_t lo = 200;
        uint8_t hi = 200;
        uint8_t sum_wrapped = (uint8_t)(lo + hi);      /* 400 mod 256 = 144 */
        uint8_t bad  = (uint8_t)(sum_wrapped / 2);
        uint8_t good = (uint8_t)(lo + (hi - lo) / 2);
        printf("\n  用 uint8_t（上限 255）在小范围上演示同样的形状：\n");
        printf("    lo = %u, hi = %u\n", (unsigned)lo, (unsigned)hi);
        printf("    (uint8_t)(lo + hi)     = %u   <-- 数学上是 400，被截断\n",
               (unsigned)sum_wrapped);
        printf("    (uint8_t)(lo+hi) / 2   = %u   <-- 落在 [lo, hi] 之外！\n",
               (unsigned)bad);
        printf("    lo + (hi - lo) / 2     = %u   <-- 正确，永远落在区间内\n",
               (unsigned)good);
        puts("    ⚠️ 关键：如果写成 (lo + hi) / 2 而 lo/hi 是 uint8_t，");
        puts("       C 会先把它们【整型提升】成 int 再相加，就不会回绕了。");
        puts("       所以要显式截断到 uint8_t 才能重现这个 bug。");
    }

    /* (c) 有符号下标更糟：是 UB，不只是回绕 */
    {
        int lo = INT_MAX - 2;
        int hi = INT_MAX - 1;
        printf("\n  如果下标是有符号 int，情况更糟：\n");
        printf("    lo = %d, hi = %d\n", lo, hi);
        printf("    lo + hi 在数学上是 %lld，超过 INT_MAX=%d\n",
               (long long)lo + (long long)hi, INT_MAX);
        printf("    -> 有符号整数溢出是 undefined behavior，不只是回绕！\n");
        printf("       编译器可以假设它不发生，从而删掉你的边界检查\n");
        printf("    lo + (hi - lo) / 2 = %d  <- 永远安全\n", lo + (hi - lo) / 2);
    }

    puts("\n  ★ 结论：永远写 mid = lo + (hi - lo) / 2");
    puts("    它和 (lo + hi) / 2 在数学上等价，但不会溢出。");
    puts("    这个 bug 在 JDK 的 Arrays.binarySearch 里真实存在了约 10 年。");
}

/* ---------------------------------------------------------------- */
/* 6. 旋转数组上的二分查找                                             */
/* ---------------------------------------------------------------- */
/*
 * 问题：一个升序数组被旋转了（如 [4,5,6,7,0,1,2]），找 target。
 * 关键观察：mid 把数组分成两半，至少有一半是有序的。
 *   如果 a[lo] <= a[mid]  -> 左半有序
 *   否则                  -> 右半有序
 * 判断 target 是否在有序的那一半里，就能决定往哪边走。
 */
static long search_rotated(const int *a, size_t n, int target)
{
    if (a == NULL || n == 0) { return -1; }
    size_t lo = 0, hi = n - 1;
    while (lo <= hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (a[mid] == target) { return (long)mid; }

        if (a[lo] <= a[mid]) {              /* 左半 [lo, mid] 有序 */
            if (a[lo] <= target && target < a[mid]) {
                if (mid == 0) { break; }
                hi = mid - 1;
            } else {
                lo = mid + 1;
            }
        } else {                            /* 右半 [mid, hi] 有序 */
            if (a[mid] < target && target <= a[hi]) {
                lo = mid + 1;
            } else {
                if (mid == 0) { break; }
                hi = mid - 1;
            }
        }
    }
    return -1;
}

/* ---------------------------------------------------------------- */
/* 7. 交叉验证：暴力法 vs 二分法                                       */
/* ---------------------------------------------------------------- */
static void cross_validate(void)
{
    puts("========== 7. 交叉验证：二分查找 vs 线性搜索 ==========");
    const size_t n = 5000;
    int *a = malloc(n * sizeof *a);
    if (a == NULL) { return; }

    /* 刻意制造大量重复值：每个值出现 5 次 */
    for (size_t i = 0; i < n; i++) { a[i] = (int)(i / 5); }

    int mismatch = 0, tested = 0;
    for (int t = -2; t <= (int)(n / 5) + 2; t++) {
        long lin = linear_search(a, n, t);
        long bin = binary_search_classic(a, n, t);
        size_t lb = lower_bound(a, n, t);
        size_t ub = upper_bound(a, n, t);

        tested++;
        /* 线性搜索和二分搜索都可能返回「任意一个」匹配位置，
         * 所以只比较「存在性」是否一致 */
        bool lin_found = (lin >= 0);
        bool bin_found = (bin >= 0);
        bool lb_found  = (lb < n && a[lb] == t);

        if (lin_found != bin_found || lin_found != lb_found) {
            printf("  MISMATCH target=%d: linear=%ld binary=%ld lower_bound=%zu\n",
                   t, lin, bin, lb);
            mismatch++;
        }
        /* ub - lb 应该等于出现次数 */
        size_t count = ub - lb;
        if (lin_found && count == 0) {
            printf("  COUNT MISMATCH target=%d: 找到了但 count=0\n", t);
            mismatch++;
        }
    }
    printf("  测试了 %d 个 target，不一致 %d 处\n", tested, mismatch);
    printf("  数组内容: 0,0,0,0,0,1,1,1,1,1,2,... 共 %zu 个元素\n", n);

    /* 展示 lower/upper bound 的用法 */
    puts("\n  lower_bound / upper_bound 的实战用途（找 3 出现的区间）：");
    size_t lb = lower_bound(a, n, 3);
    size_t ub = upper_bound(a, n, 3);
    printf("    target=3: lower_bound=%zu  upper_bound=%zu  出现 %zu 次\n",
           lb, ub, ub - lb);
    printf("    a[%zu]=%d  a[%zu]=%d\n", lb, a[lb], ub, (ub < n) ? a[ub] : -1);

    free(a);
}

/* ---------------------------------------------------------------- */
/* 8. 性能：二分 vs 线性                                               */
/* ---------------------------------------------------------------- */
static void bench_search(void)
{
    puts("\n========== 8. 性能：二分 vs 线性 ==========");
    size_t sizes[] = {1000, 100000, 10000000};

    printf("  %-12s %-10s %14s %14s\n", "n", "算法", "比较次数", "耗时(ms)");
    for (size_t k = 0; k < 3; k++) {
        size_t n = sizes[k];
        int *a = malloc(n * sizeof *a);
        if (a == NULL) { printf("  n=%zu 分配失败（内存不够）\n", n); continue; }
        fill_sorted(a, n, 0, 1);

        int target = (int)(n - 1);     /* 最坏情况：在末尾 */
        long r1 = 0, r2 = 0;

        g_probes = 0;
        Timer t1 = timer_start("");
        r1 = linear_search(a, n, target);
        double ms1 = timer_stop(t1);
        printf("  %-12zu %-10s %14llu %14.4f\n", n, "线性", g_probes, ms1);

        g_probes = 0;
        Timer t2 = timer_start("");
        r2 = binary_search_classic(a, n, target);
        double ms2 = timer_stop(t2);
        printf("  %-12s %-10s %14llu %14.4f\n", "", "二分", g_probes, ms2);

        printf("  %-12s 结果一致: %s   (log2(%zu) = %.1f)\n", "",
               (r1 == r2) ? "是" : "否", n, log2_approx(n));
        free(a);
    }
    puts("  线性搜索的比较次数 = n（最坏）；二分 = ⌈log2(n)⌉+1 左右");
    puts("  注意 n = 1000 万 时，线性要比较 1000 万次，二分只要 24 次 ——");
    puts("  差距是 40 万倍，这就是 O(n) 和 O(log n) 的区别。");
}

/* 不用 math.h 的 log2 近似（只要整数位） */
static double log2_approx(size_t n)
{
    double x = (double)n, r = 0;
    while (x >= 2.0) { x /= 2.0; r += 1.0; }
    return r;
}

/* ---------------------------------------------------------------- */
int main(void)
{
    puts("================ 搜索算法实验 ================\n");

    puts("========== 1. 线性搜索 ==========");
    {
        int a[] = {5, 3, 8, 1, 9, 2};
        size_t n = sizeof a / sizeof a[0];
        printf("  数组（未排序）: ");
        for (size_t i = 0; i < n; i++) { printf("%d ", a[i]); }
        putchar('\n');
        for (int t = 1; t <= 9; t += 4) {
            g_probes = 0;
            long idx = linear_search(a, n, t);
            printf("  查找 %d -> 下标 %-3ld (比较了 %llu 次)\n", t, idx, g_probes);
        }
        puts("  线性搜索不需要数组有序，但最坏 O(n)");
    }

    puts("\n========== 2. 二分查找（经典版）==========");
    {
        int a[16];
        fill_sorted(a, 16, 0, 2);        /* 0,2,4,...,30 */
        printf("  数组: ");
        for (size_t i = 0; i < 16; i++) { printf("%d ", a[i]); }
        putchar('\n');

        int targets[] = {0, 14, 30, 1, 31, -5};
        for (size_t i = 0; i < sizeof targets / sizeof targets[0]; i++) {
            g_probes = 0;
            long idx = binary_search_classic(a, 16, targets[i]);
            printf("  查找 %-4d -> 下标 %-4ld (比较了 %llu 次)\n",
                   targets[i], idx, g_probes);
        }
    }

    puts("\n========== 3. 二分查找的执行过程（区间变化）==========");
    {
        int a[11];
        fill_sorted(a, 11, 1, 1);        /* 1..11 */
        printf("  数组: ");
        for (size_t i = 0; i < 11; i++) { printf("%2d ", a[i]); }
        printf("\n  查找 target = 7\n\n");

        size_t lo = 0, hi = 10;
        int step = 0;
        while (lo <= hi) {
            size_t mid = lo + (hi - lo) / 2;
            printf("    第 %d 轮: lo=%2zu hi=%2zu mid=%2zu  a[mid]=%2d",
                   step++, lo, hi, mid, a[mid]);
            if (a[mid] == 7) { printf("   == target 找到了！\n"); break; }
            if (a[mid] < 7) {
                printf("   < target -> 放弃左半，lo = mid+1 = %zu\n", mid + 1);
                lo = mid + 1;
            } else {
                printf("   > target -> 放弃右半，hi = mid-1 = %zu\n", mid - 1);
                hi = mid - 1;
            }
        }
        printf("\n  观察：每轮区间长度大约减半，所以最多 ⌈log2(11)⌉ = 4 轮\n");
    }

    puts("\n========== 4. 反例：会死循环的二分查找 ==========");
    {
        int a[8];
        fill_sorted(a, 8, 0, 1);         /* 0..7 */
        int guard = 0;
        long r = binary_search_deadloop(a, 8, 99, &guard);
        printf("  数组 = [0 1 2 3 4 5 6 7], 查找一个不存在的大值 99\n");
        printf("  返回 %ld，一共循环了 %d 轮\n", r, guard);
        if (r == -2) {
            puts("  -> 检测到死循环！区间没有收缩，lo/hi 卡在相邻位置。");
        }
        puts("");
        puts("  错误代码长这样：");
        puts("      while (lo <= hi) {          // 闭区间语义");
        puts("          mid = (lo + hi) / 2;");
        puts("          if (a[mid] < target) lo = mid;   // ❌ 应该是 mid+1");
        puts("          else                 hi = mid;   // ❌ 应该是 mid-1");
        puts("      }");
        puts("");
        puts("  为什么会卡住？以 lo=6, hi=7 为例：");
        puts("      mid = (6+7)/2 = 6");
        puts("      如果 a[6] < target  ->  lo = mid = 6  （没变！）");
        puts("      下一轮 lo 还是 6，hi 还是 7，mid 还是 6 ...  永远出不去");
        puts("");
        puts("  修复：闭区间就把区间缩小到 mid 之外（mid±1）");
    }

    demo_mid_overflow();

    puts("\n========== 6. 下界 / 上界 ==========");
    {
        int a[] = {1, 2, 2, 2, 3, 3, 5, 5, 5, 5, 8};
        size_t n = sizeof a / sizeof a[0];
        printf("  数组: ");
        for (size_t i = 0; i < n; i++) { printf("%d ", a[i]); }
        printf("\n  %-8s %12s %12s %10s\n", "target", "lower_bound", "upper_bound", "出现次数");
        for (int t = 1; t <= 9; t++) {
            size_t lb = lower_bound(a, n, t);
            size_t ub = upper_bound(a, n, t);
            printf("  %-8d %12zu %12zu %10zu\n", t, lb, ub, ub - lb);
        }
        puts("  lower_bound = 第一个 >= target 的位置（返回 n 表示全部 < target）");
        puts("  upper_bound = 第一个 >  target 的位置");
        puts("  ub - lb = target 的出现次数；lb<n && a[lb]==target 才表示存在");
    }

    puts("\n========== 7. 旋转数组上的二分查找 ==========");
    {
        int a[] = {4, 5, 6, 7, 0, 1, 2};
        size_t n = sizeof a / sizeof a[0];
        printf("  旋转数组: ");
        for (size_t i = 0; i < n; i++) { printf("%d ", a[i]); }
        putchar('\n');
        for (int t = 0; t <= 7; t++) {
            long idx = search_rotated(a, n, t);
            printf("  查找 %d -> 下标 %-3ld %s\n", t, idx, (idx < 0) ? "(不存在)" : "");
        }
    }

    cross_validate();
    bench_search();

    puts("\n========== 9. 小结 ==========");
    puts("  - 二分查找的前提：数组【已排序】");
    puts("  - 写二分的第一件事：想清楚区间是【闭】还是【半开】");
    puts("     闭区间 [lo, hi]: 初始 hi = n-1; while (lo <= hi); hi = mid-1");
    puts("     半开 [lo, hi)   : 初始 hi = n;   while (lo <  hi); hi = mid");
    puts("  - 两种风格都对，但【不能混用】——混用就是死循环或漏解");
    puts("  - mid 永远写 lo + (hi - lo) / 2");
    puts("  - 处理重复元素用 lower_bound / upper_bound，不要自己瞎调边界");

    return 0;
}
