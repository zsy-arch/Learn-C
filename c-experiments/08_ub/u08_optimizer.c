/* u08_optimizer.c —— UB 最可怕的一面：优化器会「相信」你不写 UB，
 * 从而删掉你以为存在的安全检查。
 *
 * 分别用 -O0 和 -O2 编译，对比反汇编和运行结果。 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

/* 案例 1：先解引用，后判空 —— 编译器推断 p 非空，直接删掉 if */
static int deref_then_check(int *p)
{
    int v = *p;                 /* 如果 p 可能是 NULL，这里就是 UB */
    if (p == NULL) {            /* 优化器：既然上面没崩，p 一定非空 -> 这个分支是死代码 */
        return -1;
    }
    return v;
}

/* 案例 2：靠「溢出后变负」来检查溢出 —— 有符号溢出是 UB，检查会被删掉 */
static int bad_overflow_check(int a, int b)
{
    int sum = a + b;            /* UB when it overflows */
    if (sum < a) {              /* 优化器：有符号加法不会溢出 -> b<0 才可能，条件被化简 */
        return -1;              /* 你以为这里能拦住溢出 */
    }
    return sum;
}

/* 正确的溢出检查：用编译器内建函数，或者事前判断 */
static int good_overflow_check(int a, int b, int *out)
{
    if (__builtin_add_overflow(a, b, out)) {
        return -1;
    }
    return 0;
}

/* 正确的溢出检查（不依赖扩展）：加法前先判断 */
static int portable_overflow_check(int a, int b, int *out)
{
    if (b > 0 && a > INT_MAX - b) { return -1; }
    if (b < 0 && a < INT_MIN - b) { return -1; }
    *out = a + b;
    return 0;
}

int main(int argc, char **argv)
{
    /* 从命令行取值，防止编译器在编译期把整个调用常量折叠掉 */
    int a = (argc > 1) ? atoi(argv[1]) : 2000000000;
    int b = (argc > 2) ? atoi(argv[2]) : 2000000000;

    puts("== 案例 1：先解引用后判空 ==");
    int v = 42;
    printf("  deref_then_check(&v) = %d\n", deref_then_check(&v));
    puts("  -O0 的汇编里有 cbnz（判空分支）；-O2 里整个函数只剩 ldr + ret。");
    puts("  也就是说：if (p == NULL) 被优化器整段删除了。");

    printf("\n== 案例 2：错误的溢出检查 a=%d b=%d ==\n", a, b);
    printf("  a + b 的数学真值 = %lld（已超出 int 范围）\n",
           (long long)a + (long long)b);
    printf("  bad_overflow_check(a, b) = %d\n", bad_overflow_check(a, b));
    puts("  期望：返回 -1 表示「检测到溢出」。");
    puts("  实际：-O2 下编译器把 (a+b < a) 化简成了 (b < 0)，检查失效。");

    puts("\n== 正确做法 ==");
    int out = 0;
    if (good_overflow_check(a, b, &out) != 0) {
        puts("  __builtin_add_overflow 正确报告了溢出");
    } else {
        printf("  结果 %d\n", out);
    }
    if (portable_overflow_check(a, b, &out) != 0) {
        puts("  可移植版本也正确报告了溢出");
    } else {
        printf("  结果 %d\n", out);
    }

    puts("\n== 结论 ==");
    puts("  UB 不是「结果不确定」，而是「整个程序的行为都不再有任何约束」。");
    puts("  优化器把「程序不含 UB」当成公理来推理，所以 UB 能让远处的代码消失。");
    puts("  所以：永远不要用 UB 来做检查，要在触发 UB 之前就拦住它。");
    return 0;
}
