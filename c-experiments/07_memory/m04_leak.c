/* m04_leak.c —— 内存泄漏：分配了却没有释放。
 * macOS 上 LeakSanitizer 不可用，这里用系统自带的 leaks 工具检测。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 泄漏点 1：忘记 free */
static void leak_forget(void)
{
    char *p = malloc(128);
    if (p != NULL) {
        strcpy(p, "leaked 128 bytes");
    }
    /* 没有 free(p); —— 函数返回后，p 这个唯一的线索也没了 */
}

/* 泄漏点 2：提前 return 绕过了 free */
static int leak_early_return(int fail)
{
    char *buf = malloc(256);
    if (buf == NULL) { return -1; }

    if (fail) {
        return -2;          /* 忘了 free(buf) */
    }

    free(buf);
    return 0;
}

/* 泄漏点 3：指针被覆盖 */
static void leak_overwrite(void)
{
    char *p = malloc(64);
    p = malloc(64);         /* 第一块的地址被冲掉了 */
    free(p);                /* 只释放了第二块 */
}

/* 正确版本：单一出口 + goto cleanup */
static int no_leak(int fail)
{
    int   rc  = -1;
    char *buf = malloc(256);
    if (buf == NULL) { goto cleanup; }

    if (fail) { rc = -2; goto cleanup; }

    rc = 0;
cleanup:
    free(buf);
    return rc;
}

int main(void)
{
    puts("故意制造 3 处内存泄漏，共 128 + 256 + 64 = 448 字节");
    leak_forget();
    (void)leak_early_return(1);
    leak_overwrite();

    printf("对照：no_leak(1) 返回 %d，没有泄漏\n", no_leak(1));
    printf("对照：no_leak(0) 返回 %d，没有泄漏\n", no_leak(0));

    puts("程序正常退出（泄漏不会让程序崩溃，这正是它危险的地方）");
    return 0;
}
