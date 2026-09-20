/* dangling_return.c —— 反例：返回指向局部变量的指针。
 * 局部变量的生命周期在函数返回时结束，返回它的地址 = 悬垂指针 = UB。
 * 用 ASan 编译运行可以看到 stack-use-after-return。 */
#include <stdio.h>

static int *bad_make_int(void)
{
    int local = 12345;      /* 自动存储期：函数返回即失效 */
    return &local;          /* 危险！编译器会警告 -Wreturn-stack-address */
}

static char *bad_make_string(void)
{
    char buf[16] = "hello";
    return buf;             /* 同样危险 */
}

int main(void)
{
    int *p = bad_make_int();
    printf("读取悬垂指针 *p = %d\n", *p);     /* UB */

    char *s = bad_make_string();
    printf("读取悬垂指针 s  = %s\n", s);      /* UB */
    return 0;
}
