/* p04_swap.c —— 为什么 swap 不生效？C 只有值传递 */
#include <stdio.h>

/* 反例：交换的是形参副本 */
static void swap_broken(int a, int b)
{
    printf("    [broken] 入口 &a=%p &b=%p  a=%d b=%d\n",
           (void *)&a, (void *)&b, a, b);
    int t = a;
    a = b;
    b = t;
    printf("    [broken] 出口              a=%d b=%d  (副本换好了，没用)\n", a, b);
}

/* 正确：把地址按值传进来，再通过地址改原对象 */
static void swap_ok(int *pa, int *pb)
{
    printf("    [ok]     入口 pa=%p pb=%p\n", (void *)pa, (void *)pb);
    if (pa == NULL || pb == NULL || pa == pb) { return; }
    int t = *pa;
    *pa = *pb;
    *pb = t;
}

/* 指针本身也是值传递：想改「调用者的指针」，得传指针的指针 */
static void retarget_broken(int *p, int *newtarget)
{
    p = newtarget;       /* 只改了形参 p */
    (void)p;
}

static void retarget_ok(int **pp, int *newtarget)
{
    *pp = newtarget;     /* 改的是调用者那个指针变量 */
}

int main(void)
{
    int x = 1, y = 2;

    puts("== 1. 反例：swap_broken ==");
    printf("  调用前 x=%d y=%d   &x=%p &y=%p\n", x, y, (void *)&x, (void *)&y);
    swap_broken(x, y);
    printf("  调用后 x=%d y=%d   <-- 没换！\n", x, y);

    puts("\n== 2. 正确：swap_ok ==");
    printf("  调用前 x=%d y=%d\n", x, y);
    swap_ok(&x, &y);
    printf("  调用后 x=%d y=%d   <-- 换成功了\n", x, y);

    puts("\n== 3. 指针也是值：改指针本身也需要多一层 ==");
    int a = 10, b = 20;
    int *p = &a;
    printf("  初始 p -> a(%d)\n", *p);
    retarget_broken(p, &b);
    printf("  retarget_broken 后 p -> %d   <-- 没改\n", *p);
    retarget_ok(&p, &b);
    printf("  retarget_ok     后 p -> %d   <-- 改了\n", *p);

    puts("\n== 4. 口诀 ==");
    puts("  想在函数里修改 T 类型的东西，就传 T* 。");
    puts("  想修改 int，传 int*；想修改 int*，传 int** 。");

    return 0;
}
