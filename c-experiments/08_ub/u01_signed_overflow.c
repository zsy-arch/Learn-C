/* u01_signed_overflow.c —— UB #1：有符号整数溢出
 * 用 -fsanitize=undefined 运行可以在越界的瞬间被抓住。 */
#include <stdio.h>
#include <limits.h>

/* 用 volatile 阻止编译器在编译期就把结果算出来 */
static volatile int g_sink;

int main(void)
{
    printf("INT_MAX = %d\n", INT_MAX);

    volatile int a = INT_MAX;
    printf("即将计算 a + 1，其中 a = %d\n", a);

    int r = a + 1;           /* UB：有符号溢出 */
    g_sink = r;
    printf("a + 1 = %d   <-- 没有任何保证，UBSan 会在上一行报错\n", r);

    volatile int b = INT_MIN;
    printf("\nINT_MIN = %d\n", b);
    int neg = -b;            /* UB：-INT_MIN 溢出 */
    g_sink = neg;
    printf("-INT_MIN = %d   <-- 同样是 UB\n", neg);

    volatile int c = INT_MIN;
    volatile int d = -1;
    int q = c / d;           /* UB：INT_MIN / -1 溢出（在 x86 上会硬件异常） */
    g_sink = q;
    printf("INT_MIN / -1 = %d\n", q);

    return 0;
}
