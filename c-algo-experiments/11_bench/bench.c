/* bench.c —— 性能测试方法论：怎么测才可信
 *
 * 本实验要回答的问题：
 *   1. clock() 测的是什么？它和真实时间有什么区别？
 *   2. 为什么同一个函数测两次，结果差 2 倍？
 *   3. 编译器优化会不会把我要测的代码整个删掉？
 *   4. 大 O 表示法和实际耗时是什么关系？
 *
 * ★ 使用 -O0 和 -O2 各编译一次，对比结果 —— 这是本节的核心。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../timing.h"
#include "../bench.c"

/* ---------------------------------------------------------------- */
/* 被测函数                                                            */
/* ---------------------------------------------------------------- */

/* 纯计算：数组求和。容易被优化成向量指令 */
static long sum_array(const int *a, size_t n)
{
    long s = 0;
    for (size_t i = 0; i < n; i++) { s += a[i]; }
    return s;
}

/* 手动展开：编译器可能自动做，也可能不做 */
static long sum_unrolled(const int *a, size_t n)
{
    long s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    size_t i = 0;
    for (; i + 3 < n; i += 4) {
        s0 += a[i];
        s1 += a[i + 1];
        s2 += a[i + 2];
        s3 += a[i + 3];
    }
    for (; i < n; i++) { s0 += a[i]; }
    return s0 + s1 + s2 + s3;
}

/* 有副作用的版本：每次迭代都往全局 sink 写，阻止优化 */
static void busy_work_volatile(size_t iterations)
{
    for (size_t i = 0; i < iterations; i++) {
        g_bench_sink += 1;                 /* volatile 写：编译器不能删 */
    }
}

/* 无副作用的版本：结果被丢弃，编译器可以直接删掉整个循环 */
static long busy_work_pure(size_t iterations)
{
    long acc = 0;
    for (size_t i = 0; i < iterations; i++) { acc += (long)i; }
    return acc;
}

/* 内存访问模式对比：顺序 vs 跳跃 */
static long sum_strided(const int *a, size_t n, size_t stride)
{
    long s = 0;
    for (size_t i = 0; i < n; i += stride) { s += a[i]; }
    return s;
}

/* ---------------------------------------------------------------- */
/* 计时方式对比                                                        */
/* ---------------------------------------------------------------- */
static void compare_clock_sources(void)
{
    puts("========== 1. clock() vs 真实时间 ==========");
    puts("  clock()     返回【进程消耗的 CPU 时间】（精度通常 1us）");
    puts("  time()      返回【墙上时钟】，精度只有 1 秒");
    puts("  clock_gettime(CLOCK_MONOTONIC) 是 POSIX，精度纳秒级");

    printf("\n  CLOCKS_PER_SEC = %ld\n", (long)CLOCKS_PER_SEC);

    /* 测一个很短的操作 */
    {
        clock_t t0 = clock();
        volatile long long x = 0;
        for (int i = 0; i < 100000; i++) { x += i; }
        clock_t t1 = clock();
        printf("  10 万次加法: clock() 测到 %.3f ms\n",
               (double)(t1 - t0) * 1000.0 / (double)CLOCKS_PER_SEC);
    }

    /* 测一个"几乎不耗 CPU"的操作 —— 说明 clock() 的局限 */
    {
        clock_t t0 = clock();
        time_t w0 = time(NULL);
        struct timespec ts0, ts1;
        timespec_get(&ts0, TIME_UTC);
        /* 睡眠：CPU 时间几乎为 0，但墙上时间过了 0.1 秒 */
        struct timespec req = {0, 100000000L};   /* 100 ms */
        nanosleep(&req, NULL);
        clock_t t1 = clock();
        time_t w1 = time(NULL);
        timespec_get(&ts1, TIME_UTC);

        double cpu_ms = (double)(t1 - t0) * 1000.0 / (double)CLOCKS_PER_SEC;
        double wall_ms = (double)(ts1.tv_sec - ts0.tv_sec) * 1000.0
                       + (double)(ts1.tv_nsec - ts0.tv_nsec) / 1e6;
        printf("\n  sleep 100ms 期间：\n");
        printf("    clock() 测到的 CPU 时间 = %.3f ms  <-- 几乎为 0！\n", cpu_ms);
        printf("    墙上时间               = %.3f ms  <-- 这才是真实经过的时间\n", wall_ms);
        printf("    time() 精度太差，差值为 %ld 秒\n", (long)(w1 - w0));
        puts("    -> 测 I/O、锁等待、sleep 必须用墙上时钟，不能用 clock()");
    }
}

/* ---------------------------------------------------------------- */
/* 测量噪声                                                            */
/* ---------------------------------------------------------------- */
static void measure_noise(void)
{
    puts("\n========== 2. 测量噪声：同一段代码测 10 次 ==========");
    const size_t N = 2000000;
    int *a = malloc(N * sizeof *a);
    if (a == NULL) { return; }
    for (size_t i = 0; i < N; i++) { a[i] = (int)(i % 1000); }

    printf("  %-6s %12s\n", "第几次", "耗时(ms)");
    double best = 1e18, worst = 0, total = 0;
    for (int rep = 0; rep < 10; rep++) {
        Timer t = timer_start("");
        volatile long s = sum_array(a, N);
        double ms = timer_stop(t);
        (void)s;
        printf("  %-6d %12.3f\n", rep + 1, ms);
        if (ms < best) { best = ms; }
        if (ms > worst) { worst = ms; }
        total += ms;
    }
    printf("  %-6s %12.3f\n", "平均", total / 10.0);
    printf("  %-6s %12.3f  <-- 最小值\n", "最好", best);
    printf("  %-6s %12.3f  <-- 最大值\n", "最差", worst);
    printf("  波动幅度：最差 / 最好 = %.2f 倍\n", worst / best);
    puts("");
    puts("  为什么会波动？");
    puts("    - 其他进程抢占 CPU");
    puts("    - CPU 频率调节（节能模式 vs 性能模式）");
    puts("    - cache 状态：第一次跑数据不在 cache 里");
    puts("    - 内存分配器的行为");
    puts("");
    puts("  实践准则：");
    puts("    1. 多次运行，取【最小值】而不是平均（最小值最接近无限快 CPU 的理想值）");
    puts("    2. 至少要跑 5~10 次热身之后再测");
    puts("    3. 差异小于 2 倍就不要下结论说「算法 A 比 B 快」");
    puts("    4. 用同一台机器、同一时间、同样的输入对比");

    free(a);
}

/* ---------------------------------------------------------------- */
/* 编译器优化对测量的干扰                                              */
/* ---------------------------------------------------------------- */
static void optimization_interference(void)
{
    puts("\n========== 3. 编译器优化会「吃掉」你的测试代码 ==========");
    const size_t ITER = 100000000;    /* 1 亿次 */

    puts("  下面测两段「做 1 亿次加法」的循环，它们的区别只有一点：");
    puts("    A. 结果写到 g_bench_sink（volatile）-> 编译器不能删");
    puts("    B. 结果被丢弃                    -> 编译器可以整个删掉");
    printf("\n  ITER = %zu\n\n", ITER);

    {
        Timer t = timer_start("A. volatile sink（真实执行）");
        busy_work_volatile(ITER);
        double ms = timer_stop(t);
        printf("  %-34s %10.3f ms\n", "A. volatile sink", ms);
    }
    {
        Timer t = timer_start("B. 结果丢弃（可能被优化掉）");
        long r = busy_work_pure(ITER);
        double ms = timer_stop(t);
        printf("  %-34s %10.3f ms   (结果 %ld)\n", "B. 结果丢弃", ms, r);
        KEEP(r);
    }

    puts("");
    puts("  ★ 用 -O0 编译时：A 和 B 耗时接近（都没有优化）");
    puts("    用 -O2 编译时：B 会变得极快甚至 0ms —— 因为循环被整个删除了！");
    puts("");
    puts("  防止这种情况的三种方法：");
    puts("    1. 把结果写入 volatile 变量（本实验的 KEEP / g_bench_sink）");
    puts("    2. 把被测代码放进单独的 .c 文件，用 -O2 编译但不开 LTO");
    puts("    3. 使用 inline asm 屏障：__asm__ volatile(\"\" ::: \"memory\")");
}

/* ---------------------------------------------------------------- */
/* 复杂度：理论 vs 实测                                                */
/* ---------------------------------------------------------------- */
static void complexity_in_practice(void)
{
    puts("\n========== 4. 大 O 只是趋势，常数因子才是实际耗时 ==========");
    puts("  比较两个算法：");
    puts("    linear : O(n)      —— 一次遍历");
    puts("    twice  : O(n)      —— 展开成 4 路累加（常数因子更小）");
    printf("\n  %-12s %14s %14s %10s\n", "n", "linear(ms)", "twice(ms)", "比值");

    size_t sizes[] = {100000, 1000000, 10000000};
    for (size_t k = 0; k < 3; k++) {
        size_t n = sizes[k];
        int *a = malloc(n * sizeof *a);
        if (a == NULL) { continue; }
        for (size_t i = 0; i < n; i++) { a[i] = (int)(i % 100); }

        double m1, m2;
        {
            Timer t = timer_start("");
            for (int rep = 0; rep < 5; rep++) { KEEP(sum_array(a, n)); }
            m1 = timer_stop(t);
        }
        {
            /* 同样的 O(n)，但循环体手动展开成 4 路累加 */
            Timer t = timer_start("");
            for (int rep = 0; rep < 5; rep++) {
                KEEP(sum_unrolled(a, n));
                KEEP(sum_unrolled(a, n));
            }
            m2 = timer_stop(t);
        }
        printf("  %-12zu %14.3f %14.3f %10.2f\n", n, m1, m2, m2 / m1);
        free(a);
    }
    puts("  -> 同样的复杂度，常数因子直接体现在耗时上。");
    puts("     所以「O(n) 一定比 O(n log n) 快」是错的 —— 要算上常数。");
}

/* ---------------------------------------------------------------- */
/* 内存访问模式的影响                                                  */
/* ---------------------------------------------------------------- */
static void memory_pattern(void)
{
    puts("\n========== 5. 内存访问模式：同一个算法，5 倍差距 ==========");
    const size_t N = 8000000;
    int *a = malloc(N * sizeof *a);
    if (a == NULL) { return; }
    for (size_t i = 0; i < N; i++) { a[i] = 1; }

    printf("  %-22s %12s %14s\n", "访问模式", "耗时(ms)", "访问元素数");
    size_t strides[] = {1, 2, 8, 64};
    for (size_t k = 0; k < 4; k++) {
        size_t stride = strides[k];
        volatile long s = 0;
        Timer t = timer_start("");
        for (int rep = 0; rep < 3; rep++) { s += sum_strided(a, N, stride); }
        double ms = timer_stop(t);
        printf("  stride = %-13zu %12.3f %14zu\n", stride, ms, N / stride);
        (void)s;
    }
    puts("");
    puts("  注意步长 1 和步长 2 的对比：元素数减半，但耗时没有减半！");
    puts("  原因：CPU 一次读一整个 cache line（通常 64 字节 = 16 个 int）。");
    puts("        stride=1 时每个 cache line 全用上；stride=2 时只用了一半。");
    puts("        -> 这就是「cache 局部性」的实际影响。");

    free(a);
}

/* ---------------------------------------------------------------- */
int main(void)
{
    puts("================ 性能测试方法论 ================\n");

    printf("当前编译选项: ");
#if defined(__OPTIMIZE__)
    puts("开了优化（-O1 或更高）");
#else
    puts("-O0（未优化）");
#endif
    printf("__OPTIMIZE__ = %s\n",
#if defined(__OPTIMIZE__)
           "已定义"
#else
           "未定义"
#endif
    );
    puts("");
    puts("★ 请分别用 -O0 和 -O2 编译运行本文件，对比结果差异。");
    puts("  -O0: cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g bench.c -o bench_O0");
    puts("  -O2: cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O2 -g bench.c -o bench_O2");
    puts("");

    compare_clock_sources();
    measure_noise();
    optimization_interference();
    complexity_in_practice();
    memory_pattern();

    puts("\n========== 6. 性能测试检查清单 ==========");
    puts("  □ 用 clock() 测 CPU 时间，用 timespec_get 测墙上时间");
    puts("  □ 多次运行，取最小值（或至少避免只看单次结果）");
    puts("  □ 先热身几轮，让 cache 进入稳定状态");
    puts("  □ 用 volatile 或 KEEP 宏消费结果，防止被优化掉");
    puts("  □ 至少用 -O0 和 -O2 各测一次，看优化是否改变了结论");
    puts("  □ 差异小于 2 倍时，不要急着下结论");
    puts("  □ 记录环境：机器型号、编译器版本、优化级别");
    puts("  □ 测内存密集的算法时，注意 cache 效应可能盖过复杂度差异");

    return 0;
}
