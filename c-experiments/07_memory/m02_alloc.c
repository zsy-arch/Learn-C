/* m02_alloc.c —— malloc / calloc / realloc / free 的正确用法 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static void dump_ints(const char *tag, const int *p, size_t n)
{
    printf("  %-22s [", tag);
    for (size_t i = 0; i < n; i++) {
        printf("%s%d", (i ? ", " : ""), p[i]);
    }
    puts("]");
}

int main(void)
{
    puts("== 1. malloc：分配但不初始化 ==");
    size_t n = 5;
    int *a = malloc(n * sizeof *a);       /* sizeof *a：不用重复写类型名 */
    if (a == NULL) {                       /* 每一次 malloc 都要检查 */
        perror("malloc");
        return 1;
    }
    printf("  malloc(%zu * %zu) = %p\n", n, sizeof *a, (void *)a);
    printf("  内容是「不确定的」，直接读就是 UB。本次实际读到（仅供观察）：\n");
    dump_ints("malloc 后未初始化", a, n);
    for (size_t i = 0; i < n; i++) { a[i] = (int)i * 10; }
    dump_ints("手动初始化后", a, n);

    puts("\n== 2. calloc：分配 + 清零，还能检查乘法溢出 ==");
    int *b = calloc(n, sizeof *b);
    if (b == NULL) { free(a); return 1; }
    dump_ints("calloc 后", b, n);
    printf("  calloc(nmemb, size) 内部会检查 nmemb*size 是否溢出，比 malloc(n*size) 安全\n");

    puts("\n== 3. realloc：扩容 / 缩容 ==");
    printf("  扩容前 a = %p\n", (void *)a);
    size_t newn = 16;
    int *tmp = realloc(a, newn * sizeof *a);   /* 关键：先接到临时变量 */
    if (tmp == NULL) {
        /* realloc 失败时原指针 a 依然有效，不能丢 */
        free(b);
        free(a);
        fputs("realloc failed\n", stderr);
        return 1;
    }
    a = tmp;
    printf("  扩容后 a = %p\n", (void *)a);
    printf("  前 %zu 个元素被保留：", n);
    for (size_t i = 0; i < n; i++) { printf("%d ", a[i]); }
    printf("\n  新增部分的内容是不确定的，必须自己初始化\n");
    for (size_t i = n; i < newn; i++) { a[i] = -1; }
    dump_ints("补齐之后", a, newn);

    puts("\n== 4. realloc 的三个特殊约定 ==");
    puts("  realloc(NULL, size)  等价于 malloc(size)");
    puts("  realloc(p, 0)        C17 里是 implementation-defined，别用；要释放就写 free(p)");
    puts("  realloc 可能「原地扩容」也可能「搬家」，搬家后旧指针立刻失效");

    puts("\n== 5. 演示搬家：所有指向旧内存的指针都会悬垂 ==");
    int *arr = malloc(4 * sizeof *arr);
    if (arr == NULL) { free(a); free(b); return 1; }
    for (int i = 0; i < 4; i++) { arr[i] = i; }
    /* 记录旧地址的「数值」。realloc 之后旧指针变量本身就不该再被读取了，
     * 所以这里存成 uintptr_t 只用于打印和比较，不再当指针使用。 */
    uintptr_t old_addr  = (uintptr_t)arr;
    uintptr_t old_alias = (uintptr_t)&arr[2];
    printf("  扩容前: arr=0x%llX  &arr[2]=0x%llX  arr[2]=%d\n",
           (unsigned long long)old_addr, (unsigned long long)old_alias, arr[2]);
    int *arr2 = realloc(arr, 1024 * sizeof *arr);
    if (arr2 != NULL) {
        arr = arr2;
        printf("  扩容后: arr=%p  %s\n", (void *)arr,
               ((uintptr_t)arr == old_addr) ? "（原地扩容）"
                                            : "（搬家了！旧地址上的别名全部作废）");
        printf("  正确做法：realloc 之后重新计算偏移，&arr[2] = %p，arr[2]=%d\n",
               (void *)&arr[2], arr[2]);
    }
    free(arr);

    puts("\n== 6. free 的规则 ==");
    puts("  free(NULL)            合法，什么都不做");
    puts("  free 同一块两次        UB (double free)");
    puts("  free 非 malloc 的指针  UB");
    puts("  free 之后再用          UB (use after free)");
    puts("  malloc/free 必须一一配对");

    puts("\n== 7. 分配大小的整数溢出 ==");
    size_t huge = SIZE_MAX / 2 + 1;
    printf("  malloc(%zu * 4) 的乘法会回绕成 %zu —— 会分配一个很小的块！\n",
           huge, huge * 4);
    printf("  防御写法: if (n > SIZE_MAX / sizeof *p) { /* 拒绝 */ }\n");
    if (huge > SIZE_MAX / sizeof(int)) {
        puts("  检查生效，拒绝这次分配");
    }

    free(b);
    free(a);
    puts("\n  全部释放完毕");
    return 0;
}
