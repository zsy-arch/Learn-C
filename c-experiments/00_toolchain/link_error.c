/* link_error.c —— 反例：只有声明没有定义，编译能过，链接必失败 */
#include <stdio.h>

int missing_function(int x);   /* 声明了，但整个程序里没人定义它 */

int main(void)
{
    printf("%d\n", missing_function(1));
    return 0;
}
