/* p06_funcptr.c —— 函数指针与回调 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int add(int a, int b) { return a + b; }
static int sub(int a, int b) { return a - b; }
static int mul(int a, int b) { return a * b; }

/* 回调：把「怎么做」交给调用者决定 */
static void map_int(int *arr, size_t n, int (*fn)(int))
{
    for (size_t i = 0; i < n; i++) {
        arr[i] = fn(arr[i]);
    }
}

static int square(int x) { return x * x; }
static int negate(int x) { return -x; }

/* qsort 的比较回调必须是 int (*)(const void *, const void *) */
static int cmp_int_asc(const void *pa, const void *pb)
{
    int a = *(const int *)pa;
    int b = *(const int *)pb;
    return (a > b) - (a < b);      /* 避免 a-b 溢出 */
}

static int cmp_int_desc(const void *pa, const void *pb)
{
    return cmp_int_asc(pb, pa);
}

static int cmp_str(const void *pa, const void *pb)
{
    const char *const *a = (const char *const *)pa;
    const char *const *b = (const char *const *)pb;
    return strcmp(*a, *b);
}

/* 用函数指针表代替 switch —— 分发表（dispatch table） */
typedef struct {
    const char *name;
    int (*op)(int, int);
} OpEntry;

int main(void)
{
    puts("== 1. 声明的读法 ==");
    puts("  int (*fp)(int, int);");
    puts("    fp 是一个指针 -> 指向函数 -> 该函数接收 (int,int) -> 返回 int");
    puts("  int *fp(int, int);");
    puts("    fp 是一个函数 -> 接收 (int,int) -> 返回 int*   （少了括号，完全不同！）");

    puts("\n== 2. 基本用法 ==");
    int (*fp)(int, int) = add;           /* 函数名会退化成函数指针，不用写 & */
    printf("  fp = add;  fp(3,4)   = %d\n", fp(3, 4));
    printf("  (*fp)(3,4)           = %d   <-- 显式解引用，效果相同\n", (*fp)(3, 4));
    fp = &sub;                            /* 写 & 也合法 */
    printf("  fp = &sub; fp(3,4)   = %d\n", fp(3, 4));
    printf("  fp 的大小 = %zu 字节, 值 = %p\n",
           sizeof(fp), (void *)(uintptr_t)fp);

    puts("\n== 3. 函数指针数组 / 分发表 ==");
    const OpEntry table[] = {
        {"add", add},
        {"sub", sub},
        {"mul", mul},
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        printf("  %s(6, 3) = %d\n", table[i].name, table[i].op(6, 3));
    }

    puts("\n== 4. 回调：把策略传进算法 ==");
    int data[5] = {1, 2, 3, 4, 5};
    map_int(data, 5, square);
    printf("  map(square): ");
    for (int i = 0; i < 5; i++) { printf("%d ", data[i]); }
    putchar('\n');
    map_int(data, 5, negate);
    printf("  map(negate): ");
    for (int i = 0; i < 5; i++) { printf("%d ", data[i]); }
    putchar('\n');

    puts("\n== 5. 标准库 qsort 就是回调 ==");
    int nums[] = {42, 7, 19, 3, 88, 1};
    size_t n = sizeof(nums) / sizeof(nums[0]);

    qsort(nums, n, sizeof(nums[0]), cmp_int_asc);
    printf("  升序: ");
    for (size_t i = 0; i < n; i++) { printf("%d ", nums[i]); }
    putchar('\n');

    qsort(nums, n, sizeof(nums[0]), cmp_int_desc);
    printf("  降序: ");
    for (size_t i = 0; i < n; i++) { printf("%d ", nums[i]); }
    putchar('\n');

    const char *words[] = {"pear", "apple", "orange", "banana"};
    size_t wn = sizeof(words) / sizeof(words[0]);
    qsort(words, wn, sizeof(words[0]), cmp_str);
    printf("  字符串升序:");
    for (size_t i = 0; i < wn; i++) { printf(" %s", words[i]); }
    putchar('\n');

    puts("\n== 6. typedef 让声明可读 ==");
    puts("  typedef int (*BinOp)(int, int);");
    puts("  BinOp fp = add;   // 比 int (*fp)(int,int) 好读得多");

    return 0;
}
