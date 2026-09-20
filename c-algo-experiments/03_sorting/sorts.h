/* sorts.h —— 六种排序算法的统一实现
 *
 * 统一接口：void sort_xxx(int *a, size_t n)
 * 每个函数都额外统计「比较次数」和「交换次数」，用于性能对比。
 *
 * 所有函数都保证：
 *   - 输入 nullptr 或 n<2 时安全返回
 *   - 原地排序（除归并排序需要 O(n) 临时空间）
 */
#ifndef SORTS_H
#define SORTS_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* 统计计数器（全局，方便演示；生产代码应该传结构体） */
typedef struct {
    unsigned long long cmp;      /* 比较次数 */
    unsigned long long swap;     /* 交换/移动次数 */
    unsigned long long passes;   /* 循环轮数 */
} Stats;

extern Stats g_stats;

static inline void stats_reset(void)
{
    g_stats.cmp = 0;
    g_stats.swap = 0;
    g_stats.passes = 0;
}

#define CMP(a, b) (++g_stats.cmp, ((a) > (b)) - ((a) < (b)))

static inline void swap_int(int *a, int *b)
{
    g_stats.swap++;
    int t = *a;
    *a = *b;
    *b = t;
}

/* ---------------------------------------------------------------- */
/* 1. 冒泡排序 —— 稳定、原地、O(n^2)                                   */
/* ---------------------------------------------------------------- */
static inline void sort_bubble(int *a, size_t n)
{
    if (a == NULL || n < 2) { return; }
    for (size_t i = 0; i + 1 < n; i++) {
        g_stats.passes++;
        int swapped = 0;
        /* 优化：每轮结束后，最大的元素已经"冒"到末尾，可以减少比较范围 */
        for (size_t j = 0; j + 1 < n - i; j++) {
            if (CMP(a[j], a[j + 1]) > 0) {
                swap_int(&a[j], &a[j + 1]);
                swapped = 1;
            }
        }
        if (!swapped) { break; }    /* 提前退出：已经有序 */
    }
}

/* ---------------------------------------------------------------- */
/* 2. 选择排序 —— 不稳定、原地、O(n^2)                                 */
/* ---------------------------------------------------------------- */
static inline void sort_selection(int *a, size_t n)
{
    if (a == NULL || n < 2) { return; }
    for (size_t i = 0; i + 1 < n; i++) {
        g_stats.passes++;
        size_t minidx = i;
        for (size_t j = i + 1; j < n; j++) {
            if (CMP(a[j], a[minidx]) < 0) { minidx = j; }
        }
        if (minidx != i) { swap_int(&a[i], &a[minidx]); }
    }
}

/* ---------------------------------------------------------------- */
/* 3. 插入排序 —— 稳定、原地、O(n^2)，但几乎有序时接近 O(n)               */
/* ---------------------------------------------------------------- */
static inline void sort_insertion(int *a, size_t n)
{
    if (a == NULL || n < 2) { return; }
    for (size_t i = 1; i < n; i++) {
        g_stats.passes++;
        int key = a[i];
        size_t j = i;
        /* 注意 j > 0 必须先判断！size_t 是无符号的，
         * 写成 j >= 0 是死循环（见 C 语言语法文档 03_loop） */
        while (j > 0 && CMP(a[j - 1], key) > 0) {
            a[j] = a[j - 1];
            g_stats.swap++;
            j--;
        }
        a[j] = key;
    }
}

/* ---------------------------------------------------------------- */
/* 4. 希尔排序 —— 不稳定、原地、取决于增量序列                          */
/* ---------------------------------------------------------------- */
/*
 * 插入排序对"几乎有序"的数组很快。希尔排序先按大步长做插入排序，
 * 让数组变得"大致有序"，最后再做一次步长为 1 的插入排序。
 *
 * 增量序列用 Knuth 的 1, 4, 13, 40, ... (h = 3h+1)
 * 最坏复杂度约 O(n^(3/2))
 */
static inline void sort_shell(int *a, size_t n)
{
    if (a == NULL || n < 2) { return; }

    size_t h = 1;
    while (h < n / 3) { h = 3 * h + 1; }     /* 找到最大的 h */

    /* 注意：不能写 for (; h >= 1; h /= 3)！
     * h 是 size_t（无符号），h >= 1 恒真；而且 1/3 == 0，
     * 下一轮 h=0 时内层 while (j >= h) 会读到 a[j-0]，逻辑全乱。
     * 这里用显式 break 收尾。 */
    for (;;) {
        g_stats.passes++;
        for (size_t i = h; i < n; i++) {
            int key = a[i];
            size_t j = i;
            while (j >= h && CMP(a[j - h], key) > 0) {
                a[j] = a[j - h];
                g_stats.swap++;
                j -= h;
            }
            a[j] = key;
        }
        if (h == 1) { break; }
        h /= 3;
    }
}

/* ---------------------------------------------------------------- */
/* 5. 快速排序 —— 不稳定、原地、平均 O(n log n)、最坏 O(n^2)             */
/* ---------------------------------------------------------------- */

/* Lomuto 分区：把 <= pivot 的放左边，> pivot 的放右边，返回 pivot 最终位置 */
static inline size_t partition_lomuto(int *a, size_t lo, size_t hi)
{
    /* 三数取中：取 a[lo]、a[mid]、a[hi] 的中位数当 pivot，
     * 避免对已排序/逆序数组退化成 O(n^2)（那种情况下固定取 a[hi] 是最坏情形） */
    size_t mid = lo + (hi - lo) / 2;
    if (CMP(a[mid], a[lo]) < 0) { swap_int(&a[mid], &a[lo]); }   /* a[lo] = min(lo,mid) */
    if (CMP(a[hi],  a[lo]) < 0) { swap_int(&a[hi],  &a[lo]); }   /* a[lo] = 三者最小 */
    if (CMP(a[hi],  a[mid]) < 0) { swap_int(&a[hi], &a[mid]); }  /* a[mid] = 三者中位数 */

    swap_int(&a[mid], &a[hi]);         /* 中位数挪到最右当 pivot */
    int pivot = a[hi];
    size_t i = lo;                     /* i 指向"已确认小于 pivot"区域的下一个 */

    for (size_t j = lo; j < hi; j++) {
        if (CMP(a[j], pivot) < 0) {
            swap_int(&a[i], &a[j]);
            i++;
        }
    }
    swap_int(&a[i], &a[hi]);           /* pivot 就位 */
    return i;
}

static inline void quicksort_rec(int *a, size_t lo, size_t hi)
{
    if (lo >= hi) { return; }          /* 0 或 1 个元素，已有序 */

    g_stats.passes++;
    size_t p = partition_lomuto(a, lo, hi);

    if (p > lo)     { quicksort_rec(a, lo, p - 1); }
    if (p + 1 < hi) { quicksort_rec(a, p + 1, hi); }
}

static inline void sort_quick(int *a, size_t n)
{
    if (a == NULL || n < 2) { return; }
    quicksort_rec(a, 0, n - 1);
}

/* ---------------------------------------------------------------- */
/* 6. 归并排序 —— 稳定、需要 O(n) 额外空间、稳定 O(n log n)               */
/* ---------------------------------------------------------------- */
static inline void merge(int *a, int *tmp, size_t lo, size_t mid, size_t hi)
{
    size_t i = lo, j = mid + 1, k = lo;

    while (i <= mid && j <= hi) {
        /* <= 保证稳定性：相等时先取左边的 */
        if (CMP(a[i], a[j]) <= 0) { tmp[k++] = a[i++]; }
        else                      { tmp[k++] = a[j++]; }
        g_stats.swap++;
    }
    while (i <= mid) { tmp[k++] = a[i++]; g_stats.swap++; }
    while (j <= hi)  { tmp[k++] = a[j++]; g_stats.swap++; }

    memcpy(a + lo, tmp + lo, (hi - lo + 1) * sizeof *a);
}

static inline void mergesort_rec(int *a, int *tmp, size_t lo, size_t hi)
{
    if (lo >= hi) { return; }
    g_stats.passes++;
    size_t mid = lo + (hi - lo) / 2;      /* 不写 (lo+hi)/2，避免溢出 */
    mergesort_rec(a, tmp, lo, mid);
    mergesort_rec(a, tmp, mid + 1, hi);
    merge(a, tmp, lo, mid, hi);
}

static inline void sort_merge(int *a, size_t n)
{
    if (a == NULL || n < 2) { return; }
    int *tmp = malloc(n * sizeof *tmp);
    if (tmp == NULL) { return; }          /* 分配失败就放弃排序 */
    mergesort_rec(a, tmp, 0, n - 1);
    free(tmp);
}

#endif /* SORTS_H */
