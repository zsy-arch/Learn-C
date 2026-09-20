/* p05_multilevel.c —— 多级指针：什么时候真的需要 int** */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 典型场景 1：由函数负责分配内存，把「新指针」带回调用者 */
static int alloc_buffer(size_t n, char **out)
{
    if (out == NULL) { return -1; }
    char *p = malloc(n);
    if (p == NULL) { return -1; }
    memset(p, 0, n);
    snprintf(p, n, "buffer of %zu bytes", n);
    *out = p;           /* 通过二级指针写回 */
    return 0;
}

/* 典型场景 2：释放并置空，避免悬垂指针 */
static void free_and_null(void **pp)
{
    if (pp != NULL && *pp != NULL) {
        free(*pp);
        *pp = NULL;
    }
}

/* 典型场景 3：命令行参数 char **argv */
static void print_args(int argc, char **argv)
{
    for (int i = 0; i < argc; i++) {
        printf("    argv[%d] = \"%s\"  (argv+%d = %p)\n",
               i, argv[i], i, (void *)(argv + i));
    }
}

int main(int argc, char **argv)
{
    puts("== 1. 一步步看清 int** ==");
    int    v   = 42;
    int   *p   = &v;
    int  **pp  = &p;
    int ***ppp = &pp;

    printf("  v    = %d        &v   = %p\n", v, (void *)&v);
    printf("  p    = %p  &p   = %p\n", (void *)p,  (void *)&p);
    printf("  pp   = %p  &pp  = %p\n", (void *)pp, (void *)&pp);
    printf("  ppp  = %p\n", (void *)ppp);
    printf("  *p   = %d   **pp = %d   ***ppp = %d   <-- 都是同一个 v\n",
           *p, **pp, ***ppp);

    puts("\n  内存示意:");
    puts("    ppp ---> pp ---> p ---> v(42)");
    puts("    每多一个 * ，就多跳一次地址");

    puts("\n== 2. 场景：函数分配内存并回传 ==");
    char *buf = NULL;
    printf("  调用前 buf = %p\n", (void *)buf);
    if (alloc_buffer(64, &buf) == 0) {
        printf("  调用后 buf = %p, 内容 = \"%s\"\n", (void *)buf, buf);
    }

    puts("\n== 3. 场景：free 并置空 ==");
    free_and_null((void **)&buf);
    printf("  free_and_null 后 buf = %p  <-- 已置空，不是悬垂指针\n", (void *)buf);

    puts("\n== 4. 场景：argv ==");
    printf("  argc = %d\n", argc);
    print_args(argc, argv);

    puts("\n== 5. 指针数组 vs 数组指针（读法练习）==");
    int a = 1, b = 2, c = 3;
    int *arr_of_ptr[3] = {&a, &b, &c};       /* 「指针的数组」：3 个 int* */
    int  real_arr[3]   = {7, 8, 9};
    int (*ptr_to_arr)[3] = &real_arr;        /* 「指向数组的指针」：1 个指针 */

    printf("  int *arr_of_ptr[3]  : sizeof = %zu (3 个指针)\n", sizeof(arr_of_ptr));
    printf("  int (*ptr_to_arr)[3]: sizeof = %zu (1 个指针)\n", sizeof(ptr_to_arr));
    printf("  *arr_of_ptr[1]      = %d\n", *arr_of_ptr[1]);
    printf("  (*ptr_to_arr)[1]    = %d\n", (*ptr_to_arr)[1]);
    puts("  读法: 从变量名出发，先看右边，再看左边，遇到括号先算括号。");

    return 0;
}
