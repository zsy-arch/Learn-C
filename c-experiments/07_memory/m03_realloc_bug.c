/* m03_realloc_bug.c —— 反例：realloc 的经典错误写法
 *
 *   p = realloc(p, newsize);
 *
 * 如果 realloc 失败返回 NULL，p 被覆盖成 NULL，
 * 原来那块内存就再也没人能 free —— 内存泄漏，而且数据全丢。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 反例 */
static int grow_bad(char **pp, size_t newsize)
{
    *pp = realloc(*pp, newsize);       /* 危险 */
    if (*pp == NULL) {
        return -1;                     /* 此时原内存已经泄漏 */
    }
    return 0;
}

/* 正确 */
static int grow_good(char **pp, size_t newsize)
{
    char *tmp = realloc(*pp, newsize); /* 先接到临时变量 */
    if (tmp == NULL) {
        return -1;                     /* *pp 仍然有效，调用者可以继续用/释放 */
    }
    *pp = tmp;
    return 0;
}

int main(void)
{
    puts("== 正确写法：realloc 结果先放临时变量 ==");
    char *p = malloc(8);
    if (p == NULL) { return 1; }
    strcpy(p, "abc");
    printf("  扩容前 p=%p 内容=\"%s\"\n", (void *)p, p);

    if (grow_good(&p, 64) == 0) {
        printf("  扩容后 p=%p 内容=\"%s\"  <-- 内容被保留\n", (void *)p, p);
    }

    puts("\n== 用一个「一定会失败」的超大尺寸来验证 ==");
    char *q = malloc(8);
    if (q == NULL) { free(p); return 1; }
    strcpy(q, "xyz");
    printf("  q=%p 内容=\"%s\"\n", (void *)q, q);

    size_t insane = (size_t)-1 / 2;    /* 一个必然分配失败的尺寸 */
    if (grow_good(&q, insane) != 0) {
        printf("  grow_good 失败，但 q 仍然是 %p，内容仍是 \"%s\"\n", (void *)q, q);
        printf("  -> 可以正常 free，没有泄漏\n");
    } else {
        puts("  （本次分配居然成功了，换个更大的尺寸再试）");
    }
    free(q);

    puts("\n== 反例演示：grow_bad ==");
    char *r = malloc(8);
    if (r == NULL) { free(p); return 1; }
    strcpy(r, "lost");
    printf("  调用前 r=%p 内容=\"%s\"\n", (void *)r, r);
    if (grow_bad(&r, insane) != 0) {
        printf("  调用后 r=%p  <-- 变成 NULL 了\n", (void *)r);
        puts("  原来那 8 字节的地址已经没人知道，永远无法 free —— 内存泄漏");
        puts("  而且里面的 \"lost\" 也彻底丢了");
    }

    puts("\n== 最佳实践 ==");
    puts("  1) 永远写 tmp = realloc(p, n); if (tmp) p = tmp;");
    puts("  2) 不要用 realloc(p, 0) 来释放，直接 free(p)");
    puts("  3) realloc 之后，所有指向旧块的指针/迭代器全部作废");
    puts("  4) 扩容策略用「翻倍」而不是「每次 +1」，否则是 O(n^2)");

    free(p);
    return 0;
}
