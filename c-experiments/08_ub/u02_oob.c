/* u02_oob.c —— UB #2：数组越界（栈上 + 堆上）
 * 这是 C 里最常见、后果最严重的一类 bug。
 * 用 ASan 编译运行会精确指出越界位置。 */
#include <stdio.h>
#include <stdlib.h>

static void stack_oob(void)
{
    int arr[5] = {0, 1, 2, 3, 4};
    int idx = 5;                      /* 用变量绕过编译期检查 */
    printf("  arr[%d] = %d   <-- 越界读，UB\n", idx, arr[idx]);
}

static void heap_oob(void)
{
    int *p = malloc(5 * sizeof *p);
    if (p == NULL) { return; }
    for (int i = 0; i < 5; i++) { p[i] = i; }
    int idx = 5;
    p[idx] = 99;                      /* 越界写，UB：破坏堆元数据 */
    printf("  p[%d] = %d   <-- 越界写，UB\n", idx, p[idx]);
    free(p);
}

static void off_by_one(void)
{
    int arr[5] = {0};
    int n = 5;
    int sum = 0;
    for (int i = 0; i <= n; i++) {    /* 经典 off-by-one：应该是 i < n */
        sum += arr[i];
    }
    printf("  off-by-one 求和 = %d\n", sum);
}

int main(int argc, char **argv)
{
    const char *which = (argc > 1) ? argv[1] : "stack";

    printf("运行模式: %s  (可选: stack | heap | offbyone)\n", which);

    if (which[0] == 's') {
        puts("== 栈上数组越界 ==");
        stack_oob();
    } else if (which[0] == 'h') {
        puts("== 堆上数组越界 ==");
        heap_oob();
    } else {
        puts("== off-by-one ==");
        off_by_one();
    }

    puts("如果你看到这一行，说明这次越界「碰巧」没炸 —— 这才是最危险的情况");
    return 0;
}
