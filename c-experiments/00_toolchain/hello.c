/* hello.c —— 用来观察 C 程序从源码到可执行文件的四个阶段 */
#include <stdio.h>

#define GREETING "Hello, C17!"
#define SQUARE(x) ((x) * (x))

int main(void)
{
    puts(GREETING);
    printf("SQUARE(3) = %d\n", SQUARE(3));
    printf("__STDC_VERSION__ = %ldL\n", __STDC_VERSION__);
    return 0;
}
