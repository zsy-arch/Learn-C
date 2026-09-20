/* demo.c —— 函数：值传递、栈帧、递归、错误处理 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* static 函数：内部链接，只有本文件能调用，等价于「模块私有」 */
static void try_to_modify(int x)
{
    printf("    进入函数: 形参 x = %d, &x = %p\n", x, (void *)&x);
    x = 999;
    printf("    函数内改成 x = %d（改的是副本）\n", x);
}

static void modify_via_pointer(int *px)
{
    printf("    进入函数: px = %p, *px = %d\n", (void *)px, *px);
    *px = 999;
}

/* 递归：每层都有自己独立的栈帧和局部变量 */
static void show_stack_depth(int depth, const void *prev_frame)
{
    int local_marker = depth;          /* 每层一个局部变量 */
    const void *here = (const void *)&local_marker;

    if (prev_frame == NULL) {
        printf("    depth=%d  &local=%p\n", depth, here);
    } else {
        long delta = (long)((const char *)prev_frame - (const char *)here);
        printf("    depth=%d  &local=%p   与上一层相差 %ld 字节\n",
               depth, here, delta);
    }

    if (depth < 4) {
        show_stack_depth(depth + 1, here);
    }
    /* 函数返回时，local_marker 所在的栈空间立刻失效 */
}

static unsigned long long factorial(unsigned n)
{
    if (n <= 1) { return 1; }          /* base case 必须有，否则栈溢出 */
    return n * factorial(n - 1);
}

/* ---- 错误处理风格 1：返回状态码，结果通过出参带出 ---- */
typedef enum {
    OK = 0,
    ERR_NULL_ARG = 1,
    ERR_DIV_ZERO = 2
} Status;

static Status safe_div(int a, int b, int *out)
{
    if (out == NULL) { return ERR_NULL_ARG; }
    if (b == 0)      { return ERR_DIV_ZERO; }
    *out = a / b;
    return OK;
}

static const char *status_str(Status s)
{
    switch (s) {
    case OK:           return "OK";
    case ERR_NULL_ARG: return "ERR_NULL_ARG";
    case ERR_DIV_ZERO: return "ERR_DIV_ZERO";
    }
    return "UNKNOWN";
}

/* ---- 错误处理风格 2：goto cleanup 统一释放资源 ---- */
static int build_report(size_t n, char **out)
{
    int   rc  = -1;
    char *buf = NULL;
    char *tmp = NULL;

    if (out == NULL) { goto cleanup; }
    *out = NULL;

    buf = malloc(n);
    if (buf == NULL) { goto cleanup; }

    tmp = malloc(n);
    if (tmp == NULL) { goto cleanup; }

    snprintf(tmp, n, "report-%zu", n);
    memcpy(buf, tmp, strlen(tmp) + 1);

    *out = buf;
    buf  = NULL;      /* 所有权移交给调用者，防止下面被 free 掉 */
    rc   = 0;

cleanup:
    free(tmp);        /* 无论成功失败都要释放的中间资源 */
    free(buf);        /* 只有失败时 buf 非 NULL */
    return rc;
}

int main(void)
{
    puts("== 1. C 只有值传递（pass by value）==");
    int a = 1;
    printf("  调用前: a = %d, &a = %p\n", a, (void *)&a);
    try_to_modify(a);
    printf("  调用后: a = %d  <-- 没变！形参是实参的一份拷贝\n", a);

    puts("\n== 2. 想改调用者的变量，就把「地址」按值传进去 ==");
    printf("  调用前: a = %d\n", a);
    modify_via_pointer(&a);
    printf("  调用后: a = %d  <-- 变了。传的是 a 的地址的副本\n", a);

    puts("\n== 3. 栈帧：递归时每层局部变量的地址 ==");
    show_stack_depth(0, NULL);
    printf("  地址递减 => 本平台栈向低地址方向生长\n");

    puts("\n== 4. 递归 ==");
    for (unsigned n = 0; n <= 10; n += 5) {
        printf("  factorial(%u) = %llu\n", n, factorial(n));
    }
    printf("  注意：factorial(21) 就会让 unsigned long long 溢出\n");

    puts("\n== 5. 错误处理：返回状态码 + 出参 ==");
    int result = 0;
    Status st;
    st = safe_div(10, 3, &result);
    printf("  safe_div(10,3) -> %-12s result=%d\n", status_str(st), result);
    st = safe_div(10, 0, &result);
    printf("  safe_div(10,0) -> %-12s result 未被修改，仍是 %d\n", status_str(st), result);
    st = safe_div(10, 2, NULL);
    printf("  safe_div(10,2,NULL) -> %s\n", status_str(st));

    puts("\n== 6. goto cleanup 与所有权转移 ==");
    char *report = NULL;
    int rc = build_report(32, &report);
    printf("  build_report 返回 %d, report = \"%s\"\n", rc, report ? report : "(null)");
    free(report);          /* 所有权在调用者手里，由调用者释放 */
    report = NULL;

    puts("\n== 7. 函数声明的重要性 ==");
    printf("  C17 里调用没声明过的函数是硬错误（C89 会隐式声明为返回 int）\n");
    printf("  所以：先声明（头文件），后使用\n");

    return 0;
}
