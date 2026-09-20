/* main.c —— 只 include 声明，链接期才和 greet.o 绑定 */
#include <stdio.h>
#include "greet.h"
#include "greet.h"   /* 故意重复 include：include guard 保证不会重复定义 */

int main(void)
{
    greet("Alice");
    greet("Bob");
    printf("g_greet_count = %d\n", g_greet_count);
    return 0;
}
