/* mathutil.c —— 实现文件 */
#include "mathutil.h"   /* 先 include 自己的头文件，可以让编译器帮你检查签名是否一致 */

/* 内部辅助函数用 static，不暴露给其他翻译单元 */
static int mu_min(int a, int b) { return (a < b) ? a : b; }
static int mu_max(int a, int b) { return (a > b) ? a : b; }

int mu_add(int a, int b)
{
    return a + b;
}

int mu_clamp(int v, int lo, int hi)
{
    return mu_max(lo, mu_min(v, hi));
}

bool mu_is_prime(unsigned n)
{
    if (n < 2U)      { return false; }
    if (n % 2U == 0) { return n == 2U; }
    for (unsigned d = 3U; d * d <= n; d += 2U) {
        if (n % d == 0U) { return false; }
    }
    return true;
}

long mu_sum(const int *arr, size_t n)
{
    if (arr == NULL) { return 0; }
    long s = 0;
    for (size_t i = 0; i < n; i++) {
        s += arr[i];
    }
    return s;
}
