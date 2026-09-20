/* demo.c —— 斐波那契数列的四种实现 + 调用次数实测
 *
 * 这个实验要回答一个具体问题：
 *   fib(40) 的朴素递归到底调用了多少次函数？
 * 我们不猜，直接数。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include "../timing.h"
#include "../bench.c"

/* ---------------------------------------------------------------- */
/* 全局调用计数器：用来「看见」递归树的规模                              */
/* ---------------------------------------------------------------- */
static unsigned long long g_calls = 0;
static unsigned long long g_max_depth = 0;

/* ---------------------------------------------------------------- */
/* 实现 1：朴素递归                                                    */
/* ---------------------------------------------------------------- */
static uint64_t fib_naive(int n)
{
    g_calls++;
    if (n < 2) { return (uint64_t)n; }
    return fib_naive(n - 1) + fib_naive(n - 2);
}

/* 带深度追踪的版本：用来观察栈帧数量 */
static uint64_t fib_depth(int n, unsigned long long depth)
{
    if (depth > g_max_depth) { g_max_depth = depth; }
    if (n < 2) { return (uint64_t)n; }
    return fib_depth(n - 1, depth + 1) + fib_depth(n - 2, depth + 1);
}

/* ---------------------------------------------------------------- */
/* 实现 2：记忆化（memoization）—— 自顶向下                              */
/* ---------------------------------------------------------------- */
/*
 * 关键点：memo 要初始化为「未计算」的标记。
 * 用 0 当标记是错的！因为 fib(0) == 0 也是合法结果。
 * 这里用 UINT64_MAX 当「未计算」。
 */
#define FIB_UNSET UINT64_MAX

static uint64_t fib_memo_rec(int n, uint64_t *memo)
{
    g_calls++;
    if (n < 2) { return (uint64_t)n; }
    if (memo[n] != FIB_UNSET) { return memo[n]; }    /* 命中缓存 */
    memo[n] = fib_memo_rec(n - 1, memo) + fib_memo_rec(n - 2, memo);
    return memo[n];
}

static uint64_t fib_memo(int n)
{
    uint64_t *memo = malloc(((size_t)n + 1) * sizeof *memo);
    if (memo == NULL) { return 0; }
    for (int i = 0; i <= n; i++) { memo[i] = FIB_UNSET; }
    uint64_t r = fib_memo_rec(n, memo);
    free(memo);
    return r;
}

/* ---------------------------------------------------------------- */
/* 实现 3：自底向上迭代 —— O(n) 时间，O(1) 空间                          */
/* ---------------------------------------------------------------- */
static uint64_t fib_iter(int n)
{
    uint64_t a = 0, b = 1;      /* fib(0), fib(1) */
    for (int i = 0; i < n; i++) {
        uint64_t next = a + b;
        a = b;
        b = next;
    }
    return a;
}

/* 打印整个序列的版本 */
static void fib_print_sequence(int count)
{
    uint64_t a = 0, b = 1;
    printf("  ");
    for (int i = 0; i < count; i++) {
        printf("%" PRIu64 " ", a);
        uint64_t next = a + b;
        a = b;
        b = next;
    }
    putchar('\n');
}

/* ---------------------------------------------------------------- */
/* 实现 4：矩阵快速幂 —— O(log n)                                      */
/* ---------------------------------------------------------------- */
/*
 * 原理：
 *   [F(n+1) F(n)  ]   [1 1]^n
 *   [F(n)   F(n-1)] = [1 0]
 *
 * 用二进制快速幂把 n 次矩阵乘法降到 log n 次。
 * 对 int 范围来说这是杀鸡用牛刀，但它是理解「快速幂」的入口。
 */
typedef struct { uint64_t m[2][2]; } Mat2;

static Mat2 mat_mul(Mat2 a, Mat2 b)
{
    Mat2 r;
    r.m[0][0] = a.m[0][0]*b.m[0][0] + a.m[0][1]*b.m[1][0];
    r.m[0][1] = a.m[0][0]*b.m[0][1] + a.m[0][1]*b.m[1][1];
    r.m[1][0] = a.m[1][0]*b.m[0][0] + a.m[1][1]*b.m[1][0];
    r.m[1][1] = a.m[1][0]*b.m[0][1] + a.m[1][1]*b.m[1][1];
    return r;
}

static uint64_t fib_matrix(unsigned n)
{
    Mat2 result = {{{1,0},{0,1}}};      /* 单位矩阵 */
    Mat2 base   = {{{1,1},{1,0}}};
    unsigned e = n;

    while (e > 0) {
        if (e & 1u) { result = mat_mul(result, base); }
        base = mat_mul(base, base);
        e >>= 1;                         /* 每轮指数减半 */
    }
    return result.m[0][1];               /* F(n) */
}

/* ---------------------------------------------------------------- */
int main(void)
{
    puts("========== 1. 四种实现的一致性（n = 0..20）==========");
    printf("  n  :  朴素    记忆化   迭代     矩阵\n");
    for (int n = 0; n <= 20; n++) {
        uint64_t a = fib_naive(n);
        uint64_t b = fib_memo(n);
        uint64_t c = fib_iter(n);
        uint64_t d = fib_matrix((unsigned)n);
        int ok = (a == b && b == c && c == d);
        printf("  %2d : %6" PRIu64 "  %6" PRIu64 "  %6" PRIu64 "  %6" PRIu64 "  %s\n",
               n, a, b, c, d, ok ? "" : "  <-- 不一致！");
    }

    puts("\n========== 2. 序列本身 ==========");
    puts("  F(0)..F(19):");
    fib_print_sequence(20);

    puts("\n========== 3. 朴素递归的调用次数（这是本节的重点）==========");
    printf("  %-6s %-16s\n", "n", "函数调用次数");
    for (int n = 5; n <= 30; n += 5) {
        g_calls = 0;
        uint64_t r = fib_naive(n);
        printf("  %-6d %-16llu F(n)=%" PRIu64 "\n", n, g_calls, r);
    }
    {
        g_calls = 0;
        uint64_t r = fib_naive(35);
        printf("  %-6d %-16llu F(n)=%" PRIu64 "\n", 35, g_calls, r);
        printf("\n  看出规律了吗？n 每加 5，调用次数约乘以 1.618^5 ≈ 11 倍。\n");
        printf("  n=35 已经要调用 %llu 次函数了！\n", g_calls);
    }

    puts("\n========== 4. 递归深度 = 栈帧数量 ==========");
    {
        g_max_depth = 0;
        (void)fib_depth(20, 0);
        printf("  fib(20) 的最大递归深度 = %llu\n", g_max_depth);
        g_max_depth = 0;
        (void)fib_depth(30, 0);
        printf("  fib(30) 的最大递归深度 = %llu\n", g_max_depth);
        puts("  深度只有 n，但调用次数是 1.618^n —— 差异在于「重复计算」");
    }

    puts("\n========== 5. 性能实测：fib(35) 四种实现 ==========");
    {
        volatile uint64_t sink = 0;
        uint64_t r = 0;

        TIME_IT("fib_naive(35)",   { r = fib_naive(35);   sink += r; });
        printf("     -> %" PRIu64 "\n", r);

        TIME_IT("fib_memo(35)",    { r = fib_memo(35);    sink += r; });
        printf("     -> %" PRIu64 "\n", r);

        TIME_IT("fib_iter(35)",    { r = fib_iter(35);    sink += r; });
        printf("     -> %" PRIu64 "\n", r);

        TIME_IT("fib_matrix(35)",  { r = fib_matrix(35);  sink += r; });
        printf("     -> %" PRIu64 "\n", r);

        (void)sink;
    }

    puts("\n========== 6. 性能实测：fib(45) 三种快速实现 ==========");
    {
        volatile uint64_t sink = 0;
        uint64_t r = 0;
        puts("  （朴素递归 45 会调用约 36 亿次函数，不测了）");

        TIME_IT("fib_memo(45)",    { r = fib_memo(45);    sink += r; });
        printf("     -> %" PRIu64 "\n", r);
        TIME_IT("fib_iter(45)",    { r = fib_iter(45);    sink += r; });
        printf("     -> %" PRIu64 "\n", r);
        TIME_IT("fib_matrix(45)",  { r = fib_matrix(45);  sink += r; });
        printf("     -> %" PRIu64 "\n", r);

        (void)sink;
    }

    puts("\n========== 7. 溢出：uint64_t 能装到第几项？==========");
    {
        printf("  %-6s %-22s\n", "n", "F(n)");
        int overflow_at = -1;
        for (int n = 88; n <= 95; n++) {
            uint64_t a = fib_iter(n);
            int of = (n > 0) && (a < fib_iter(n - 1));
            printf("  %-6d %-22" PRIu64 "%s\n", n, a, of ? "   <-- 溢出了！" : "");
            if (overflow_at < 0 && of) { overflow_at = n; }
        }
        printf("  因为 F(93) ≈ 1.22e19 已接近 UINT64_MAX ≈ 1.8e19，");
        printf("F(94) = 1.97e19 超过上界，所以 n >= 94 开始回绕\n");
        printf("  检测到溢出起点: n = %d\n", overflow_at);
        puts("  注意：无符号溢出是【有定义】的（模 2^64 回绕），");
        puts("        所以程序不会崩，只会静默给出错误结果 —— 这更危险。");
    }

    puts("\n========== 8. 结论 ==========");
    puts("  朴素递归  : O(1.618^n) 时间, O(n) 栈空间  <- 只适合教学");
    puts("  记忆化    : O(n) 时间,   O(n) 空间        <- 自顶向下，好写");
    puts("  迭代      : O(n) 时间,   O(1) 空间        <- 首选");
    puts("  矩阵快速幂 : O(log n) 时间, O(1) 空间      <- n 极大时才有意义");

    return 0;
}
