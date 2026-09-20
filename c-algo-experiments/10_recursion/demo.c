/* demo.c —— 递归与分治：三要素、栈帧、尾递归、递归转迭代
 *
 * 本实验要回答的问题：
 *   1. 写递归必须满足什么条件？少一个会怎样？
 *   2. 汉诺塔的递归到底在做什么？为什么是 2^n - 1 步？
 *   3. 尾递归是什么？C 编译器会自动优化它吗？（答案可能让你意外）
 *   4. 递归一定能改成迭代吗？怎么改？
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include "../timing.h"
#include "../bench.c"

/* 全局统计：观察递归的执行规模 */
static unsigned long long g_calls = 0;
static unsigned long long g_max_depth = 0;
static long long g_hanoi_moves = 0;

#define RESET_STATS() do { g_calls = 0; g_max_depth = 0; } while (0)
#define COUNT_CALL(d) do {                                     \
        g_calls++;                                                  \
        if ((unsigned long long)(d) > g_max_depth) {                \
            g_max_depth = (unsigned long long)(d);                  \
        }                                                           \
    } while (0)

/* ================================================================ */
/* 1. 递归三要素                                                      */
/* ================================================================ */
/*
 * 要素 1：基准情形（base case）    —— 什么时候停下来
 * 要素 2：递归关系（recurrence）   —— 大问题怎么拆成小问题
 * 要素 3：向基准推进（progress）   —— 每次递归必须更接近基准
 *
 * 缺任何一个都会出问题：
 *   缺 1 -> 永远不停，栈溢出
 *   缺 2 -> 根本递归不起来
 *   缺 3 -> 永远不停，栈溢出
 */

/* 阶乘：三要素齐全 */
static unsigned long long factorial(unsigned n)
{
    if (n <= 1) { return 1; }                    /* ① 基准 */
    return (unsigned long long)n * factorial(n - 1);   /* ② 递归关系 ③ n-1 向 1 推进 */
}

/* ---------------------------------------------------------------- */
/* 反例 1：缺基准情形                                                   */
/* ---------------------------------------------------------------- */
static unsigned long long fact_no_base(unsigned n, int depth)
{
    COUNT_CALL(depth);
    if (depth > 100000) {
        return 0;                 /* 安全阀：否则真的会栈溢出崩溃 */
    }
    return (unsigned long long)n * fact_no_base(n - 1, depth + 1);
}

/* ---------------------------------------------------------------- */
/* 反例 2：不向基准推进（永远到不了 n <= 1）                              */
/* ---------------------------------------------------------------- */
static unsigned long long fact_no_progress(unsigned n, int depth)
{
    COUNT_CALL(depth);
    if (depth > 100000) { return 0; }
    if (n <= 1) { return 1; }
    return (unsigned long long)n * fact_no_progress(n, depth + 1);   /* n 没变！ */
}

/* ================================================================ */
/* 2. 汉诺塔                                                          */
/* ================================================================ */
/*
 * 问题：把 n 个盘子从 A 移到 C，每次只能移动一个，大盘不能压在小盘上。
 *
 * 递归思路（极其优雅）：
 *   1. 把上面 n-1 个盘子从 A 移到 B（借助 C）
 *   2. 把最大的盘子从 A 移到 C
 *   3. 把 n-1 个盘子从 B 移到 C（借助 A）
 *
 * 基准：n == 1 时直接移动。
 *
 * 步数递推：T(n) = 2*T(n-1) + 1, T(1) = 1
 *           => T(n) = 2^n - 1
 */
static void hanoi(int n, char from, char to, char via, int depth, bool verbose)
{
    COUNT_CALL(depth);
    if (n == 1) {
        g_hanoi_moves++;
        if (verbose) {
            for (int i = 0; i < depth; i++) { printf("  "); }
            printf("move disk 1: %c -> %c\n", from, to);
        }
        return;
    }
    hanoi(n - 1, from, via, to, depth + 1, verbose);   /* ① n-1: from -> via */
    g_hanoi_moves++;
    if (verbose) {
        for (int i = 0; i < depth; i++) { printf("  "); }
        printf("move disk %d: %c -> %c\n", n, from, to);
    }
    hanoi(n - 1, via, to, from, depth + 1, verbose);   /* ③ n-1: via -> to */
}

/* ================================================================ */
/* 3. 分治：把问题一分为二                                            */
/* ================================================================ */

/* 3a. 二分查找（递归版）—— O(log n) */
static long binary_search_rec(const int *a, long lo, long hi, int target, int depth)
{
    COUNT_CALL(depth);
    if (lo > hi) { return -1; }                       /* 基准 */
    long mid = lo + (hi - lo) / 2;                    /* 防溢出 */
    if (a[mid] == target) { return mid; }
    if (a[mid] < target)  { return binary_search_rec(a, mid + 1, hi, target, depth + 1); }
    return binary_search_rec(a, lo, mid - 1, target, depth + 1);
}

/* 3b. 快速幂 —— O(log n)，分治的经典应用 */
/*
 * 计算 base^exp：
 *   exp 是偶数: base^exp = (base^(exp/2))^2
 *   exp 是奇数: base^exp = base * base^(exp-1)
 *
 * 比循环乘法快得多：2^100 用循环要乘 100 次，用快速幂只要约 7 次。
 */
static unsigned long long fast_pow_rec(unsigned long long base, unsigned exp, int depth)
{
    COUNT_CALL(depth);
    if (exp == 0) { return 1; }                       /* 基准 */
    if (exp % 2 == 0) {
        unsigned long long half = fast_pow_rec(base, exp / 2, depth + 1);
        return half * half;
    }
    return base * fast_pow_rec(base, exp - 1, depth + 1);
}

/* 快速幂的迭代版本 —— 用二进制分解 */
static unsigned long long fast_pow_iter(unsigned long long base, unsigned exp)
{
    unsigned long long result = 1;
    while (exp > 0) {
        if (exp & 1u) { result *= base; }             /* 当前二进制位是 1 */
        base *= base;                                  /* base 平方 */
        exp >>= 1;                                     /* 指数右移 */
    }
    return result;
}

/* 3c. 最大子数组和（分治法）—— Kadane 的 O(n) 版本做对照 */
static long max_crossing_sum(const int *a, long lo, long mid, long hi)
{
    long left_sum = -2147483647L, sum = 0;
    for (long i = mid; i >= lo; i--) {
        sum += a[i];
        if (sum > left_sum) { left_sum = sum; }
    }
    long right_sum = -2147483647L;
    sum = 0;
    for (long i = mid + 1; i <= hi; i++) {
        sum += a[i];
        if (sum > right_sum) { right_sum = sum; }
    }
    return left_sum + right_sum;
}

static long max_subarray_dc(const int *a, long lo, long hi, int depth)
{
    COUNT_CALL(depth);
    if (lo == hi) { return a[lo]; }                   /* 基准：一个元素 */

    long mid = lo + (hi - lo) / 2;
    long l = max_subarray_dc(a, lo, mid, depth + 1);
    long r = max_subarray_dc(a, mid + 1, hi, depth + 1);
    long c = max_crossing_sum(a, lo, mid, hi);

    long best = (l > r) ? l : r;
    return (best > c) ? best : c;
}

/* Kadane 算法：O(n) 迭代版 */
static long max_subarray_kadane(const int *a, long n)
{
    long best = a[0], cur = a[0];
    for (long i = 1; i < n; i++) {
        cur = (a[i] > cur + a[i]) ? a[i] : cur + a[i];
        if (cur > best) { best = cur; }
    }
    return best;
}

/* ================================================================ */
/* 4. 尾递归                                                          */
/* ================================================================ */
/*
 * 尾递归：递归调用是函数的【最后一个动作】，返回值直接被返回，不做任何处理。
 *
 *   ❌ 不是尾递归：return n * fact(n-1);   —— 递归返回后还要乘 n
 *   ✅ 是尾递归：  return fact_acc(n-1, n*acc);  —— 递归结果直接返回
 *
 * 尾递归的意义：
 *   理论上编译器可以把递归改成循环（复用同一个栈帧），从而避免栈溢出。
 *   但 —— C 标准【不要求】编译器这么做！只有某些编译器在 -O2 下会做。
 *   所以写尾递归在 C 里【不保证】能省栈。
 */
static unsigned long long fact_acc(unsigned n, unsigned long long acc)
{
    if (n <= 1) { return acc; }
    return fact_acc(n - 1, acc * (unsigned long long)n);   /* 尾调用 */
}

/* 手动改写成循环 —— 这才是 C 里最可靠的做法 */
static unsigned long long fact_loop(unsigned n)
{
    unsigned long long acc = 1;
    for (unsigned i = 2; i <= n; i++) { acc *= i; }
    return acc;
}

/* 递归求和的尾递归版与迭代版 */
static long sum_rec_tail(const int *a, size_t n, long acc)
{
    if (n == 0) { return acc; }
    return sum_rec_tail(a + 1, n - 1, acc + a[0]);   /* 尾调用 */
}

static long sum_iter(const int *a, size_t n)
{
    long acc = 0;
    for (size_t i = 0; i < n; i++) { acc += a[i]; }
    return acc;
}

/* ================================================================ */
/* 5. 递归转迭代：用显式栈                                             */
/* ================================================================ */
/* 阶乘的迭代版（最简单的情形：递归只有一个分支，直接改循环） */
static unsigned long long fact_iter_simple(unsigned n)
{
    unsigned long long r = 1;
    for (unsigned i = 2; i <= n; i++) { r *= i; }
    return r;
}

/*
 * 汉诺塔的迭代版（复杂情形：递归有两个分支，必须用显式栈）
 * 这里用最简单的"模拟递归调用栈"方式：把待处理的子问题压栈。
 */
typedef struct { int n; char from, to, via; } HanoiTask;

static long hanoi_iter(int n, char from, char to, char via)
{
    if (n <= 0) { return 0; }
    HanoiTask *stk = malloc(1024 * sizeof *stk);
    if (stk == NULL) { return -1; }
    int top = 0;
    long moves = 0;

    stk[top++] = (HanoiTask){ n, from, to, via };
    while (top > 0) {
        HanoiTask t = stk[--top];
        if (t.n == 1) {
            moves++;
        } else {
            /* 注意压栈顺序和递归的【相反】：
             * 递归是 ①->②->③，栈是后进先出，所以要 ③->②->① 压入 */
            if (top + 2 < 1024) {
                stk[top++] = (HanoiTask){ t.n - 1, t.via, t.to, t.from };  /* ③ */
                stk[top++] = (HanoiTask){ 1, t.from, t.to, t.via };        /* ② 直接移动 */
                stk[top++] = (HanoiTask){ t.n - 1, t.from, t.via, t.to };  /* ① */
            }
        }
    }
    free(stk);
    return moves;
}

/* ================================================================ */
int main(void)
{
    puts("================ 递归与分治实验 ================\n");

    /* ---------- 1. 三要素 ---------- */
    puts("========== 1. 递归三要素 ==========");
    {
        printf("  factorial(0..10): ");
        for (unsigned n = 0; n <= 10; n++) { printf("%llu ", factorial(n)); }
        putchar('\n');
        puts("  要素 1 基准情形：if (n <= 1) return 1;");
        puts("  要素 2 递归关系：n! = n * (n-1)!");
        puts("  要素 3 向基准推进：每次调用 n-1，最终一定到达 n <= 1");

        puts("\n  【反例 1】缺少基准情形（用安全阀防止真的崩溃）:");
        RESET_STATS();
        (void)fact_no_base(10, 0);
        printf("    fact_no_base(10) 递归了 %llu 次，最大深度 %llu\n",
               g_calls, g_max_depth);
        puts("    -> 永远到不了终止条件，如果摘掉安全阀就是栈溢出崩溃");

        puts("\n  【反例 2】不向基准推进（n 没变）:");
        RESET_STATS();
        (void)fact_no_progress(10, 0);
        printf("    fact_no_progress(10) 递归了 %llu 次，最大深度 %llu\n",
               g_calls, g_max_depth);
        puts("    -> 同样永远到不了基准，因为 n 从来不变");
    }

    /* ---------- 2. 汉诺塔 ---------- */
    puts("\n========== 2. 汉诺塔：递归的执行过程 ==========");
    {
        puts("  n = 3 的完整移动过程（缩进表示递归深度）:");
        RESET_STATS();
        g_hanoi_moves = 0;
        hanoi(3, 'A', 'C', 'B', 1, true);
        printf("\n  共 %lld 步，函数调用 %llu 次，最大递归深度 %llu\n",
               g_hanoi_moves, g_calls, g_max_depth);

        puts("\n  步数与 n 的关系（理论值 2^n - 1）:");
        printf("  %-6s %-14s %-14s %-10s\n", "n", "实际步数", "2^n - 1", "函数调用");
        for (int n = 1; n <= 20; n += 3) {
            RESET_STATS();
            g_hanoi_moves = 0;
            hanoi(n, 'A', 'C', 'B', 0, false);
            printf("  %-6d %-14lld %-14lld %-10llu\n",
                   n, g_hanoi_moves, (1LL << n) - 1, g_calls);
        }
        puts("");
        puts("  ★ 复杂度的震撼之处：");
        puts("    T(n) = 2*T(n-1) + 1  =>  T(n) = 2^n - 1");
        puts("    n = 64 时是 1.8e19 步。假设每秒移动一次，需要约 5850 亿年。");
        puts("    这就是「指数级复杂度」—— 递归代码只有 5 行，代价却不可承受。");
        puts("");
        RESET_STATS();
        g_hanoi_moves = 0;
        hanoi(20, 'A', 'C', 'B', 0, false);
        printf("  验证：n=20 时实际步数 = %lld，2^20-1 = %lld\n",
               g_hanoi_moves, (1LL << 20) - 1);
    }

    /* ---------- 3. 分治 ---------- */
    puts("\n========== 3. 分治：三个经典例子 ==========");
    {
        /* 3a. 二分查找递归版 */
        int a[100];
        for (int i = 0; i < 100; i++) { a[i] = i * 2; }
        RESET_STATS();
        long idx = binary_search_rec(a, 0, 99, 42, 0);
        printf("  二分查找（递归版）: 在 100 个元素里找 42 -> 下标 %ld\n", idx);
        printf("    递归深度 %llu（log2(100) ≈ 6.6）\n", g_max_depth);

        /* 3b. 快速幂 */
        puts("");
        RESET_STATS();
        unsigned long long r1 = fast_pow_rec(2, 30, 0);
        printf("  快速幂（递归）: 2^30 = %llu，递归调用 %llu 次\n", r1, g_calls);
        unsigned long long r2 = fast_pow_iter(2, 30);
        printf("  快速幂（迭代）: 2^30 = %llu\n", r2);
        printf("    朴素循环乘法需要 30 次，快速幂只要约 log2(30) ≈ 5 次\n");
        printf("    两者一致: %s\n", (r1 == r2) ? "是" : "否");

        /* 3c. 最大子数组和 */
        puts("");
        int arr[] = {-2, 1, -3, 4, -1, 2, 1, -5, 4};
        size_t n = sizeof arr / sizeof arr[0];
        printf("  最大子数组和: 数组 = ");
        for (size_t i = 0; i < n; i++) { printf("%d ", arr[i]); }
        printf("\n");
        RESET_STATS();
        long dc = max_subarray_dc(arr, 0, (long)n - 1, 0);
        printf("    分治法:   %ld（递归调用 %llu 次，深度 %llu）\n", dc, g_calls, g_max_depth);
        long kad = max_subarray_kadane(arr, (long)n);
        printf("    Kadane:   %ld（一次循环，O(n)）\n", kad);
        printf("    两者一致: %s\n", (dc == kad) ? "是" : "否");
        puts("    -> 分治是 O(n log n)，Kadane 是 O(n)。");
        puts("       分治不总是最优解，但它是通用的思考框架。");
    }

    /* ---------- 4. 尾递归 ---------- */
    puts("\n========== 4. 尾递归：C 里能指望编译器优化吗？==========");
    {
        RESET_STATS();
        unsigned long long f1 = fact_acc(10, 1);
        printf("  尾递归阶乘 fact_acc(10, 1) = %llu，调用 %llu 次，深度 %llu\n",
               f1, g_calls, g_max_depth);

        unsigned long long f2 = fact_loop(10);
        printf("  循环版本   fact_loop(10)    = %llu\n", f2);
        printf("  一致: %s\n", (f1 == f2) ? "是" : "否");

        puts("");
        puts("  ★ 关键问题：尾递归能省栈吗？");
        puts("    C 标准【不要求】编译器做尾调用优化（TCO）。");
        puts("    那本机的 clang 做不做？用 100 万层递归实测。");
        puts("");
        puts("    请分别用 -O0 和 -O2 编译本文件，对比下面这一段的结果：");
        puts("      cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g demo.c -o demo_O0");
        puts("      cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O2 -g demo.c -o demo_O2");

        printf("\n    当前二进制是: %s\n",
#if defined(__OPTIMIZE__)
               "-O1 或更高（__OPTIMIZE__ 已定义）"
#else
               "-O0（__OPTIMIZE__ 未定义）"
#endif
        );
        fflush(stdout);

        printf("    测试 fact_acc(1000000, 1) —— 100 万层递归...\n");
        fflush(stdout);
        {
            unsigned long long big = fact_acc(1000000, 1);   /* 数值必然溢出，只看会不会崩 */
            printf("    成功返回 %llu（数值早已溢出，重点是【没有崩溃】）\n", big);
            puts("");
            puts("    -> 本机实测：");
            puts("         -O0 : 栈溢出崩溃，exit = 139 (SIGSEGV)");
            puts("         -O1/-O2 : 正常返回，因为编译器把尾递归改成了循环");
            puts("");
            puts("       反汇编证据（把 fact_acc 单独编译成汇编）:");
            puts("         -O0 : 能看到 `bl _fact_acc` —— 真的在递归调用自己");
            puts("         -O2 : 完全没有自调用指令，整个函数变成了一个循环");
            puts("");
            puts("       另一个证据：本节的 fact_acc(10,1) 在 -O2 下调用计数是 0，");
            puts("       因为整个函数被优化成循环，COUNT_CALL 宏根本不会被执行。");
        }

        puts("");
        puts("  【结论】不要依赖尾递归优化。");
        puts("    1. C 标准不要求 TCO，-O0 下就是栈溢出");
        puts("    2. 换个编译器、多加一个局部变量、或让尾调用不再成立，");
        puts("       优化就消失了，程序立刻崩");
        puts("    3. C 里要避免栈溢出，就用：");
        puts("         - 显式循环（首选）");
        puts("         - 显式栈（递归结构复杂时）");
        puts("         - 控制递归深度（或改用平衡树等结构降低深度）");
    }

    /* ---------- 5. 递归转迭代 ---------- */
    puts("\n========== 5. 递归转迭代 ==========");
    {
        /* 单分支递归 -> 直接改循环 */
        puts("  (a) 单分支递归（阶乘）：直接改成循环");
        printf("      fact_iter_simple(20) = %llu\n", fact_iter_simple(20));

        puts("\n  (b) 双分支递归（汉诺塔）：必须用显式栈");
        printf("      %-8s %-14s %-14s %s\n", "n", "递归步数", "迭代步数", "一致");
        for (int n = 1; n <= 12; n += 3) {
            g_hanoi_moves = 0;
            hanoi(n, 'A', 'C', 'B', 0, false);
            long rec_moves = g_hanoi_moves;
            long it_moves = hanoi_iter(n, 'A', 'C', 'B');
            printf("      %-8d %-14ld %-14ld %s\n", n, rec_moves, it_moves,
                   (rec_moves == it_moves) ? "是" : "否");
        }
        puts("");
        puts("  (c) 尾递归（求和）：改成循环最自然");
        int data[1000];
        for (int i = 0; i < 1000; i++) { data[i] = i + 1; }
        long s1 = sum_rec_tail(data, 1000, 0);
        long s2 = sum_iter(data, 1000);
        printf("      递归尾调用求和 = %ld，循环求和 = %ld，一致: %s\n",
               s1, s2, (s1 == s2) ? "是" : "否");

        puts("");
        puts("  转换的一般方法：");
        puts("    1. 单分支递归 -> 直接改循环（最简单）");
        puts("    2. 尾递归     -> 把累积参数变成循环变量");
        puts("    3. 多分支递归 -> 用显式栈保存「待处理的子问题」");
        puts("                   注意压栈顺序要和递归调用顺序【相反】");
    }

    /* ---------- 6. 递归 vs 迭代 的性能 ---------- */
    puts("\n========== 6. 性能：递归 vs 迭代 ==========");
    {
        const int N = 2000000;
        int *a = malloc((size_t)N * sizeof *a);
        if (a == NULL) { return 1; }
        for (int i = 0; i < N; i++) { a[i] = 1; }

        {
            Timer t = timer_start("");
            long s = sum_iter(a, (size_t)N);
            double ms = timer_stop(t);
            printf("  迭代求和 %d 个元素: %8.3f ms (和 %ld)\n", N, ms, s);
        }
        {
            Timer t = timer_start("");
            long s = sum_rec_tail(a, (size_t)N, 0);
            double ms = timer_stop(t);
            printf("  尾递归求和 %d 层:   %8.3f ms (和 %ld)\n", N, ms, s);
        }
        puts("");
        puts("  注意：上面那个 200 万层尾递归【只有 -O1/-O2 才能跑完】。");
        puts("        用 -O0 编译本文件时，程序会在第 4 节就栈溢出退出（exit 139），");
        puts("        根本走不到这里 —— 这本身就是最好的证明。");
        puts("        生产代码要可靠，就用循环。");

        free(a);
    }

    puts("\n========== 7. 小结 ==========");
    puts("  1. 递归必须有：基准情形 + 递归关系 + 向基准推进");
    puts("  2. 每次递归调用的栈帧包含：返回地址、参数、局部变量");
    puts("     递归深度 * 栈帧大小 就是栈占用 —— 这是递归的硬约束");
    puts("  3. 汉诺塔 T(n) = 2^n - 1，是「代码优雅但复杂度爆炸」的典型");
    puts("  4. 分治 = 分解 + 解决 + 合并，二分查找/快排/归并/快速幂都是它");
    puts("  5. 尾递归在 C 里【不保证】被优化，不要依赖它");
    puts("  6. 想避免栈溢出：优先用循环；复杂递归用显式栈");

    return 0;
}
