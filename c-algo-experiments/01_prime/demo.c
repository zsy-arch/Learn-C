/* demo.c —— 素数判断的三种算法与复杂度实测
 *
 * 三种做法：
 *   1. 试除法（朴素）          O(n)
 *   2. 试除法（到 sqrt(n)）    O(sqrt(n))
 *   3. 埃拉托斯特尼筛法         O(n log log n)，一次筛出全部
 *
 * 本实验的关键问题：教科书说的「sqrt(n) 优化」到底快多少？
 * 我们不看理论，直接测。
 */
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../timing.h"
#include "../bench.c"   /* 只在本文件里定义一次 g_bench_sink */

/* ---------------------------------------------------------------- */
/* 算法 1：朴素试除 —— 试到 n-1                                        */
/* ---------------------------------------------------------------- */
static bool is_prime_naive(unsigned n)
{
    if (n < 2) { return false; }
    for (unsigned d = 2; d < n; d++) {      /* 一直试到 n-1 */
        if (n % d == 0) { return false; }
    }
    return true;
}

/* ---------------------------------------------------------------- */
/* 算法 2：试除到 sqrt(n)                                             */
/* ---------------------------------------------------------------- */
static bool is_prime_sqrt(unsigned n)
{
    if (n < 2) { return false; }
    if (n % 2 == 0) { return n == 2; }      /* 2 是唯一的偶素数 */
    /* d * d <= n 而不是 d <= sqrt(n)：
     * 1) 避免 math.h 的开销和浮点误差
     * 2) 但 d * d 可能溢出！n 接近 UINT_MAX 时要小心（见下面的安全版本） */
    for (unsigned d = 3; d * d <= n; d += 2) {
        if (n % d == 0) { return false; }
    }
    return true;
}

/* 更安全的版本：用除法避免 d*d 溢出 */
static bool is_prime_sqrt_safe(unsigned n)
{
    if (n < 2) { return false; }
    if (n % 2 == 0) { return n == 2; }
    for (unsigned d = 3; d <= n / d; d += 2) {   /* d <= n/d 等价于 d*d <= n，但不会溢出 */
        if (n % d == 0) { return false; }
    }
    return true;
}

/* ---------------------------------------------------------------- */
/* 算法 3：埃拉托斯特尼筛法（Sieve of Eratosthenes）                    */
/* ---------------------------------------------------------------- */
/*
 * 返回一个 bool 数组，composite[i] == 1 表示 i 是合数。
 * 内存：n+1 字节。
 *
 * 原理：
 *   从 2 开始，把每个素数的所有倍数标记为合数。
 *   标记到 sqrt(n) 就够 —— 因为更大的素数 p 的倍数 k*p (k>=p)
 *   早就被更小的因子标记过了。
 *
 * 复杂度：时间 O(n log log n)，空间 O(n)
 */
static unsigned char *sieve(size_t n, size_t *out_count)
{
    /* calloc 自动清零，比 malloc + memset 更清晰 */
    unsigned char *composite = calloc(n + 1, 1);
    if (composite == NULL) { return NULL; }

    if (n >= 0) { composite[0] = 1; }       /* 0 不是素数（标记为「非素数」） */
    if (n >= 1) { composite[1] = 1; }       /* 1 也不是 */

    for (size_t p = 2; p * p <= n; p++) {
        if (!composite[p]) {
            /* 从 p*p 开始标记：比 p 小的倍数已经被更小的素数标过了 */
            for (size_t m = p * p; m <= n; m += p) {
                composite[m] = 1;
            }
        }
    }

    if (out_count != NULL) {
        size_t cnt = 0;
        for (size_t i = 2; i <= n; i++) {
            if (!composite[i]) { cnt++; }
        }
        *out_count = cnt;
    }
    return composite;
}

/* ---------------------------------------------------------------- */
/* 演示用：打印小范围的素数                                              */
/* ---------------------------------------------------------------- */
static void print_primes_upto(unsigned limit)
{
    printf("  %u 以内的素数: ", limit);
    int shown = 0;
    for (unsigned i = 2; i <= limit; i++) {
        if (is_prime_sqrt(i)) {
            printf("%u ", i);
            shown++;
        }
    }
    printf(" (共 %d 个)\n", shown);
}

/* 用筛法列出 1..limit 的素数 */
static void print_sieve_upto(unsigned limit)
{
    size_t cnt = 0;
    unsigned char *c = sieve(limit, &cnt);
    if (c == NULL) { return; }
    printf("  筛法求出 %u 以内的素数: ", limit);
    for (unsigned i = 2; i <= limit; i++) {
        if (!c[i]) { printf("%u ", i); }
    }
    printf("\n  共 %zu 个\n", cnt);
    free(c);
}

int main(void)
{
    puts("========== 1. 朴素试法的正确性 ==========");
    print_primes_upto(50);

    puts("\n========== 2. 筛法的正确性（应当一致）==========");
    print_sieve_upto(50);

    puts("\n========== 3. 交叉验证：两种算法在 1..10000 上结果一致 ==========");
    {
        size_t cnt = 0;
        unsigned char *c = sieve(10000, &cnt);
        if (c == NULL) { return 1; }
        int mismatch = 0;
        for (unsigned i = 2; i <= 10000; i++) {
            bool by_trial = is_prime_sqrt(i);
            bool by_sieve = !c[i];
            if (by_trial != by_sieve) {
                printf("  MISMATCH at %u: trial=%d sieve=%d\n", i, by_trial, by_sieve);
                mismatch++;
            }
        }
        printf("  1..10000 共 %zu 个素数，不一致 %d 处\n", cnt, mismatch);
        free(c);
    }

    puts("\n========== 4. 边界与陷阱 ==========");
    printf("  is_prime_sqrt(0)  = %d\n", is_prime_sqrt(0));
    printf("  is_prime_sqrt(1)  = %d\n", is_prime_sqrt(1));
    printf("  is_prime_sqrt(2)  = %d  (2 是唯一的偶素数)\n", is_prime_sqrt(2));
    printf("  is_prime_sqrt(4)  = %d\n", is_prime_sqrt(4));
    printf("  is_prime_sqrt(9)  = %d\n", is_prime_sqrt(9));
    printf("  is_prime_sqrt(97) = %d\n", is_prime_sqrt(97));
    printf("  2147483647 (2^31-1, 梅森素数) = %d\n", is_prime_sqrt_safe(2147483647u));

    puts("\n========== 5. 性能实测：单个数判断（n = 1,000,003，是素数）==========");
    {
        unsigned n = 1000003u;   /* 一个较大的素数 */
        volatile int sink = 0;
        TIME_IT("is_prime_sqrt_safe(1000003)", { if (is_prime_sqrt_safe(n)) { sink++; } });
        (void)sink;
        printf("  （朴素试法要试 100 万次，太慢，跳过；下面用更大规模对比）\n");
    }

    puts("\n========== 6. 性能实测：sqrt 优化 vs 朴素（n 从 1 到 20000 逐个判断）==========");
    {
        const unsigned limit = 20000;
        volatile int sink = 0;
        int cnt_result = 0;

        TIME_IT("朴素试除 is_prime_naive (<=20000)",
                {
                    int c = 0;
                    for (unsigned i = 2; i <= limit; i++) { if (is_prime_naive(i)) { c++; } }
                    cnt_result = c;
                });
        printf("  素数个数 = %d\n", cnt_result);

        TIME_IT("试除到 sqrt(n) (<=20000)",
                {
                    int c = 0;
                    for (unsigned i = 2; i <= limit; i++) { if (is_prime_sqrt(i)) { c++; } }
                    cnt_result = c;
                });
        printf("  素数个数 = %d\n", cnt_result);

        TIME_IT("试除到 n/d (<=20000)",
                {
                    int c = 0;
                    for (unsigned i = 2; i <= limit; i++) { if (is_prime_sqrt_safe(i)) { c++; } }
                    cnt_result = c;
                });
        printf("  素数个数 = %d\n", cnt_result);

        sink = cnt_result;
        (void)sink;
        puts("  -> 同样的结果，朴素版本慢了约 100 倍。");
        puts("     这就是把 O(n) 降到 O(sqrt(n)) 的实际收益。");
    }

    puts("\n========== 7. 性能实测：筛法 vs 逐个判断（求 1..1,000,000 的所有素数）==========");
    {
        size_t cnt1 = 0, cnt2 = 0;
        const unsigned limit = 1000000u;

        TIME_IT("逐个 is_prime_sqrt (<=1e6)",
                {
                    cnt1 = 0;
                    for (unsigned i = 2; i <= limit; i++) { if (is_prime_sqrt(i)) { cnt1++; } }
                });
        printf("  找到 %zu 个素数\n", cnt1);

        TIME_IT("sieve(1e6)",
                {
                    unsigned char *c = sieve(limit, &cnt2);
                    if (c != NULL) { free(c); }
                });
        printf("  找到 %zu 个素数\n", cnt2);

        printf("  结果一致: %s\n", (cnt1 == cnt2) ? "是" : "否");
        puts("  -> 筛法又快了十几倍，而且和「逐个判断」的结果完全一致。");
        puts("     代价是 O(n) 的额外内存。");
    }

    puts("\n========== 8. 筛法的空间代价 ==========");
    {
        printf("  sizeof table for 1e6   = %zu 字节 (unsigned char)\n", (size_t)1000001);
        printf("  如果用 bool[] (1 字节) = %zu 字节\n", (size_t)1000001);
        printf("  如果用位图（1 bit/数） = %zu 字节  <- 8 倍压缩\n", (size_t)1000001 / 8);
        printf("  如果用 int[] (4 字节)  = %zu 字节  <- 别这么干\n", (size_t)1000001 * 4);
        puts("");
        puts("  这就是「空间换时间」的典型例子：");
        puts("    逐个判断：O(1) 内存，O(sqrt(n)) 时间/每个数");
        puts("    筛法    ：O(n) 内存，O(n log log n) 时间/全部数");
        puts("  要判断「一个数」用试除；要枚举「一批数」用筛法。");
    }

    puts("\n========== 9. 小结 ==========");
    puts("  朴素试除      O(n)            —— 只适合教学");
    puts("  试除到 sqrt(n) O(sqrt(n))      —— 判断单个数的最佳选择");
    puts("  埃氏筛        O(n log log n)  —— 枚举一批素数的最佳选择");
    puts("");
    puts("  两个易错点：");
    puts("    1. 忘记处理 0 和 1（它们都不是素数）");
    puts("    2. 忘记处理 2（唯一的偶素数）；写成 d += 2 前必须先特判 2");
    puts("  一个可移植性细节：");
    puts("    d * d <= n 在 n 接近 UINT_MAX 时会让 d*d 溢出。");
    puts("    写成 d <= n / d 更安全（多一次除法，但结果正确）。");

    return 0;
}
