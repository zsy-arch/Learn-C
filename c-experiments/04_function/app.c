/* app.c —— 使用 mathutil 模块 */
#include <stdio.h>
#include "mathutil.h"

int main(void)
{
    printf("mu_add(3, 4)            = %d\n", mu_add(3, 4));
    printf("mu_clamp(15, 0, 10)     = %d\n", mu_clamp(15, 0, 10));
    printf("mu_clamp(-5, 0, 10)     = %d\n", mu_clamp(-5, 0, 10));

    printf("2..20 里的素数:");
    for (unsigned n = 2; n <= 20; n++) {
        if (mu_is_prime(n)) { printf(" %u", n); }
    }
    putchar('\n');

    int data[] = {1, 2, 3, 4, 5};
    size_t n = sizeof(data) / sizeof(data[0]);   /* 只能在「看得见数组」的地方算 */
    printf("mu_sum(data, %zu)        = %ld\n", n, mu_sum(data, n));
    return 0;
}
