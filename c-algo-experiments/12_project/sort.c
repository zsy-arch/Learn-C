/* sort.c —— 排序与搜索的实现 */
#include "algolib.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---------------------------------------------------------------- */
/* 交换                                                              */
/* ---------------------------------------------------------------- */
static void swap_int(int *a, int *b)
{
    int t = *a;
    *a = *b;
    *b = t;
}

/* ---------------------------------------------------------------- */
/* 冒泡排序 —— 稳定，O(n^2)                                            */
/* ---------------------------------------------------------------- */
void al_sort_bubble(int *a, size_t n)
{
    if (a == NULL || n < 2) { return; }
    for (size_t i = 0; i + 1 < n; i++) {
        bool swapped = false;
        for (size_t j = 0; j + 1 < n - i; j++) {
            if (a[j] > a[j + 1]) {
                swap_int(&a[j], &a[j + 1]);
                swapped = true;
            }
        }
        if (!swapped) { break; }          /* 提前退出 */
    }
}

/* ---------------------------------------------------------------- */
/* 插入排序 —— 稳定，几乎有序时接近 O(n)                                */
/* ---------------------------------------------------------------- */
void al_sort_insertion(int *a, size_t n)
{
    if (a == NULL || n < 2) { return; }
    for (size_t i = 1; i < n; i++) {
        int key = a[i];
        size_t j = i;
        /* j > 0 必须先判断：size_t 是无符号的，j >= 0 恒真 */
        while (j > 0 && a[j - 1] > key) {
            a[j] = a[j - 1];
            j--;
        }
        a[j] = key;
    }
}

/* ---------------------------------------------------------------- */
/* 选择排序 —— 不稳定，交换次数最少（最多 n-1 次）                       */
/* ---------------------------------------------------------------- */
void al_sort_selection(int *a, size_t n)
{
    if (a == NULL || n < 2) { return; }
    for (size_t i = 0; i + 1 < n; i++) {
        size_t minidx = i;
        for (size_t j = i + 1; j < n; j++) {
            if (a[j] < a[minidx]) { minidx = j; }
        }
        if (minidx != i) { swap_int(&a[i], &a[minidx]); }
    }
}

/* ---------------------------------------------------------------- */
/* 希尔排序 —— 不稳定，O(n^(3/2)) 左右                                 */
/* ---------------------------------------------------------------- */
void al_sort_shell(int *a, size_t n)
{
    if (a == NULL || n < 2) { return; }

    size_t h = 1;
    while (h < n / 3) { h = 3 * h + 1; }

    /* 不能写 for (; h >= 1; h /= 3)：h 是无符号的，恒真；
     * 而且 1/3 == 0，下一轮 h=0 会导致内层 while 逻辑崩溃。 */
    for (;;) {
        for (size_t i = h; i < n; i++) {
            int key = a[i];
            size_t j = i;
            while (j >= h && a[j - h] > key) {
                a[j] = a[j - h];
                j -= h;
            }
            a[j] = key;
        }
        if (h == 1) { break; }
        h /= 3;
    }
}

/* ---------------------------------------------------------------- */
/* 快速排序 —— 不稳定，平均 O(n log n)，最坏 O(n^2)                     */
/* ---------------------------------------------------------------- */
static size_t partition_lomuto(int *a, size_t lo, size_t hi)
{
    /* 三数取中，避免对已排序数组退化 */
    size_t mid = lo + (hi - lo) / 2;
    if (a[mid] < a[lo]) { swap_int(&a[mid], &a[lo]); }
    if (a[hi]  < a[lo]) { swap_int(&a[hi],  &a[lo]); }
    if (a[hi]  < a[mid]) { swap_int(&a[hi], &a[mid]); }

    swap_int(&a[mid], &a[hi]);
    int pivot = a[hi];
    size_t i = lo;
    for (size_t j = lo; j < hi; j++) {
        if (a[j] < pivot) {
            swap_int(&a[i], &a[j]);
            i++;
        }
    }
    swap_int(&a[i], &a[hi]);
    return i;
}

static void quicksort_rec(int *a, size_t lo, size_t hi)
{
    if (lo >= hi) { return; }
    size_t p = partition_lomuto(a, lo, hi);
    if (p > lo)     { quicksort_rec(a, lo, p - 1); }
    if (p + 1 < hi) { quicksort_rec(a, p + 1, hi); }
}

void al_sort_quick(int *a, size_t n)
{
    if (a == NULL || n < 2) { return; }
    quicksort_rec(a, 0, n - 1);
}

/* ---------------------------------------------------------------- */
/* 归并排序 —— 稳定，稳定 O(n log n)，需要 O(n) 额外空间                 */
/* ---------------------------------------------------------------- */
static void merge_range(int *a, int *tmp, size_t lo, size_t mid, size_t hi)
{
    size_t i = lo, j = mid + 1, k = lo;
    while (i <= mid && j <= hi) {
        /* <= 保证稳定：相等时取左半边的 */
        if (a[i] <= a[j]) { tmp[k++] = a[i++]; }
        else              { tmp[k++] = a[j++]; }
    }
    while (i <= mid) { tmp[k++] = a[i++]; }
    while (j <= hi)  { tmp[k++] = a[j++]; }
    memcpy(a + lo, tmp + lo, (hi - lo + 1) * sizeof *a);
}

static void mergesort_rec(int *a, int *tmp, size_t lo, size_t hi)
{
    if (lo >= hi) { return; }
    size_t mid = lo + (hi - lo) / 2;
    mergesort_rec(a, tmp, lo, mid);
    mergesort_rec(a, tmp, mid + 1, hi);
    merge_range(a, tmp, lo, mid, hi);
}

void al_sort_merge(int *a, size_t n)
{
    if (a == NULL || n < 2) { return; }
    int *tmp = malloc(n * sizeof *tmp);
    if (tmp == NULL) { return; }          /* 分配失败就放弃排序 */
    mergesort_rec(a, tmp, 0, n - 1);
    free(tmp);
}

/* ---------------------------------------------------------------- */
/* 搜索                                                              */
/* ---------------------------------------------------------------- */
long al_linear_search(const int *a, size_t n, int target)
{
    if (a == NULL) { return -1; }
    for (size_t i = 0; i < n; i++) {
        if (a[i] == target) { return (long)i; }
    }
    return -1;
}

long al_binary_search(const int *a, size_t n, int target)
{
    if (a == NULL || n == 0) { return -1; }
    size_t lo = 0, hi = n - 1;
    while (lo <= hi) {
        size_t mid = lo + (hi - lo) / 2;   /* 不写 (lo+hi)/2，避免溢出 */
        if (a[mid] == target) { return (long)mid; }
        if (a[mid] < target) {
            lo = mid + 1;
        } else {
            if (mid == 0) { break; }       /* 防 size_t 下溢 */
            hi = mid - 1;
        }
    }
    return -1;
}

size_t al_lower_bound(const int *a, size_t n, int target)
{
    if (a == NULL) { return 0; }
    size_t lo = 0, hi = n;                 /* 半开区间 [lo, hi) */
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (a[mid] < target) { lo = mid + 1; }
        else                 { hi = mid; }
    }
    return lo;
}

size_t al_upper_bound(const int *a, size_t n, int target)
{
    if (a == NULL) { return 0; }
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (a[mid] <= target) { lo = mid + 1; }
        else                  { hi = mid; }
    }
    return lo;
}
