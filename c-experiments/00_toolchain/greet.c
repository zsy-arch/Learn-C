/* greet.c —— 定义(definition)：真正分配了代码/存储 */
#include <stdio.h>
#include "greet.h"

int g_greet_count = 0;   /* 定义，分配存储 */

void greet(const char *who)
{
    g_greet_count++;
    printf("Hello, %s! (第 %d 次问候)\n", who, g_greet_count);
}
