/* unsigned_loop.c —— 反例：用无符号类型倒着数，条件 i >= 0 永远成立。
 * 加了安全计数器，否则这是死循环。故意不加 -Werror 编译以观察警告。 */
#include <stdio.h>

int main(void)
{
    int arr[] = {10, 20, 30};
    size_t n = sizeof(arr) / sizeof(arr[0]);

    puts("== 错误写法：size_t i 倒着数 ==");
    int guard = 0;
    for (size_t i = n - 1; i >= 0; i--) {     /* i 是无符号，i>=0 恒真 */
        if (guard++ >= 6) {
            printf("  ...安全阀触发，这是死循环！i 现在 = %zu\n", i);
            break;
        }
        printf("  i=%zu\n", i);
    }

    puts("\n== 正确写法 1：下标 +1 偏移 ==");
    for (size_t i = n; i-- > 0; ) {
        printf("  arr[%zu]=%d\n", i, arr[i]);
    }

    puts("\n== 正确写法 2：用有符号类型 ==");
    for (int i = (int)n - 1; i >= 0; i--) {
        printf("  arr[%d]=%d\n", i, arr[i]);
    }
    return 0;
}
