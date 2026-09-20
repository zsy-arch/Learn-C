/* demo.c —— 循环：for / while / do-while / break / continue / goto cleanup */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 演示 goto cleanup：C 没有析构函数，goto 是最干净的资源回收写法 */
static int load_two_buffers(size_t n1, size_t n2)
{
    int   rc = -1;
    char *a  = NULL;
    char *b  = NULL;

    a = malloc(n1);
    if (a == NULL) {
        goto cleanup;                  /* 一处失败，统一出口 */
    }
    memset(a, 'A', n1);

    b = malloc(n2);
    if (b == NULL) {
        goto cleanup;
    }
    memset(b, 'B', n2);

    printf("    两块缓冲区都分配成功: a[0]=%c b[0]=%c\n", a[0], b[0]);
    rc = 0;

cleanup:
    free(b);                           /* free(NULL) 是安全的空操作 */
    free(a);
    return rc;
}

int main(void)
{
    puts("== 1. for：三个部分都可以省略 ==");
    for (int i = 0; i < 5; i++) {
        printf("  i=%d", i);
    }
    putchar('\n');

    puts("\n== 2. while：条件先判断，可能一次都不执行 ==");
    int n = 0;
    while (n > 0) {
        puts("  不会打印");
        n--;
    }
    printf("  while(0>0) 循环体执行了 0 次\n");

    puts("\n== 3. do-while：至少执行一次 ==");
    int m = 0;
    do {
        printf("  do-while 体执行了一次，m=%d\n", m);
        m--;
    } while (m > 0);

    puts("\n== 4. break / continue ==");
    printf("  1..10 中跳过偶数，遇到 7 停止: ");
    for (int i = 1; i <= 10; i++) {
        if (i % 2 == 0) {
            continue;        /* 跳过本次剩余部分，继续下一次 */
        }
        if (i == 7) {
            break;           /* 直接跳出整个循环 */
        }
        printf("%d ", i);
    }
    putchar('\n');

    puts("\n== 5. 循环不变式（loop invariant）：二分查找 ==");
    int  arr[] = {1, 3, 5, 7, 9, 11, 13};
    int  target = 11;
    size_t len = sizeof(arr) / sizeof(arr[0]);
    size_t lo = 0, hi = len;          /* 不变式：答案若存在，一定在 [lo, hi) 里 */
    int found = -1;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;   /* 不写 (lo+hi)/2，避免溢出 */
        printf("    lo=%zu hi=%zu mid=%zu arr[mid]=%d\n", lo, hi, mid, arr[mid]);
        if (arr[mid] == target) { found = (int)mid; break; }
        if (arr[mid] <  target) { lo = mid + 1; }
        else                    { hi = mid; }
    }
    printf("  找到 %d 在下标 %d\n", target, found);

    puts("\n== 6. 半开区间 [0, n) 是 C 的默认约定 ==");
    printf("  len = %zu，合法下标是 0..%zu，arr[%zu] 就越界了\n", len, len - 1, len);
    printf("  写 i <= len 而不是 i < len 就是经典 off-by-one\n");

    puts("\n== 7. 嵌套循环 + 标签式跳出 ==");
    int matrix[3][4] = {{1,2,3,4},{5,6,7,8},{9,10,11,12}};
    int want = 7;
    int fi = -1, fj = -1;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            if (matrix[i][j] == want) {
                fi = i; fj = j;
                goto found_label;     /* break 只能跳一层，goto 一次跳出两层 */
            }
        }
    }
found_label:
    printf("  %d 位于 matrix[%d][%d]\n", want, fi, fj);

    puts("\n== 8. goto cleanup 资源回收惯用法 ==");
    printf("  load_two_buffers(16, 32) 返回 %d\n", load_two_buffers(16, 32));

    puts("\n== 9. 浮点数不能用来控制循环步进 ==");
    int steps = 0;
    for (double d = 0.0; d < 1.0; d += 0.1) {
        steps++;
    }
    printf("  for(d=0.0; d<1.0; d+=0.1) 实际跑了 %d 次（你以为是 10 次）\n", steps);
    double acc = 0.0;
    for (int i = 0; i < 10; i++) { acc += 0.1; }
    printf("  累加十次 0.1 = %.20f\n", acc);
    printf("  正确做法：用整数计数，需要时再换算成浮点\n");

    return 0;
}
