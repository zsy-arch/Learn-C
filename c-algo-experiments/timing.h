/* timing.h —— 计时工具：所有性能测试共用
 *
 * 为什么用 clock() 而不是 time()？
 *   time() 精度只有 1 秒，测不出小算法的差别。
 *   clock() 返回「进程消耗的 CPU 时间」，精度通常是微秒级。
 *
 * 注意：clock() 测的是 CPU 时间，不是墙上时钟（wall clock）。
 *       多线程程序里 CPU 时间会大于墙钟时间。
 */
#ifndef TIMING_H
#define TIMING_H

#include <stdio.h>
#include <time.h>

typedef struct {
    clock_t start;
    const char *label;
} Timer;

static inline Timer timer_start(const char *label)
{
    Timer t;
    t.label = label;
    t.start = clock();
    return t;
}

static inline double timer_stop(Timer t)
{
    clock_t end = clock();
    double ms = (double)(end - t.start) * 1000.0 / (double)CLOCKS_PER_SEC;
    return ms;
}

/* 跑一次，打印 "label ... 12.345 ms"
 * volatile sink 用来阻止编译器把整个计算优化掉。 */
#define TIME_IT(label, stmt)                                    \
    do {                                                        \
        Timer _t = timer_start(label);                          \
        stmt;                                                   \
        printf("  %-34s %10.3f ms\n", label, timer_stop(_t));   \
    } while (0)

/* 防止编译器消除纯计算：把结果"用掉" */
extern volatile long g_bench_sink;
#define KEEP(x) do { g_bench_sink += (long)(x); } while (0)

#endif /* TIMING_H */
