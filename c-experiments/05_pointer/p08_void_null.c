/* p08_void_null.c —— void*、NULL、野指针、悬垂指针 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* void* 是「类型被擦除」的通用指针，标准库靠它做泛型 */
static void print_bytes(const char *tag, const void *obj, size_t size)
{
    const unsigned char *p = (const unsigned char *)obj;
    printf("  %-10s (%zu 字节): ", tag, size);
    for (size_t i = 0; i < size; i++) {
        printf("%02X ", p[i]);
    }
    putchar('\n');
}

/* 泛型 swap：只知道字节数，不知道类型 */
static void generic_swap(void *a, void *b, size_t size)
{
    unsigned char *pa = (unsigned char *)a;
    unsigned char *pb = (unsigned char *)b;
    for (size_t i = 0; i < size; i++) {
        unsigned char t = pa[i];
        pa[i] = pb[i];
        pb[i] = t;
    }
}

int main(void)
{
    puts("== 1. void* 可以接住任何对象指针 ==");
    int    i = 0x01020304;
    double d = 1.5;
    char   s[] = "Hi";

    print_bytes("int",    &i, sizeof(i));
    print_bytes("double", &d, sizeof(d));
    print_bytes("char[3]", s, sizeof(s));

    void *vp = &i;
    printf("  void *vp = &i;  vp = %p\n", vp);
    printf("  *(int*)vp = %d   <-- 用之前必须转回正确的类型\n", *(int *)vp);
    printf("  *vp 直接解引用是编译错误：void 没有大小\n");
    printf("  sizeof(void) 在 ISO C 里是非法的（GCC/Clang 扩展当成 1）\n");

    puts("\n== 2. 泛型 swap ==");
    int x = 1, y = 2;
    generic_swap(&x, &y, sizeof(int));
    printf("  swap int:    x=%d y=%d\n", x, y);
    double p = 1.0, q = 2.0;
    generic_swap(&p, &q, sizeof(double));
    printf("  swap double: p=%g q=%g\n", p, q);

    puts("\n== 3. 空指针 NULL ==");
    int *np = NULL;
    printf("  NULL 打印出来是 %p\n", (void *)np);
    printf("  if (np) 为 %s；空指针在布尔上下文里是假\n", np ? "真" : "假");
    printf("  解引用 NULL 是 UB（见 08_ub/u05_nullderef.c）\n");
    printf("  malloc 失败返回 NULL，所以每次 malloc 都要检查\n");

    puts("\n== 4. 野指针（wild pointer）：未初始化的指针 ==");
    puts("  int *wild;          // 它的值是垃圾，指向哪里没人知道");
    puts("  *wild = 1;          // UB，可能崩溃，也可能悄悄破坏别的数据");
    puts("  修复：int *wild = NULL;  —— 定义即初始化");

    puts("\n== 5. 悬垂指针（dangling pointer）：指向已失效对象 ==");
    char *heap = malloc(16);
    if (heap != NULL) {
        strcpy(heap, "alive");
        printf("  free 之前: heap=%p 内容=\"%s\"\n", (void *)heap, heap);
        free(heap);
        printf("  free 之后: heap 仍然是 %p，但这块内存已经不属于你了\n", (void *)heap);
        heap = NULL;                       /* 关键的一行 */
        printf("  置 NULL 之后: heap=%p，再误用会立刻暴露而不是静默出错\n", (void *)heap);
    }

    puts("\n== 6. 最佳实践 ==");
    puts("  1) 指针定义时就初始化（有值就赋值，没值就 = NULL）");
    puts("  2) free 之后立刻置 NULL");
    puts("  3) 函数入口检查指针参数是否为 NULL");
    puts("  4) void* 转回去时类型必须和当初存进来的一致");

    return 0;
}
