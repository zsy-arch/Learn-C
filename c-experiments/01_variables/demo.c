/* demo.c —— 变量、类型、存储期、限定符 */
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <float.h>
#include <stdbool.h>

/* ---- 不同存储期 / 链接属性的变量 ---- */
static int g_internal = 42;   /* 静态存储期 + 内部链接（本文件可见） */
int        g_external = 7;    /* 静态存储期 + 外部链接（整个程序可见） */
static int g_zero_init;       /* 未显式初始化的静态变量 = 0，这是标准保证的 */

static void counter(void)
{
    static int calls = 0;     /* 块作用域 + 静态存储期：函数返回后依然活着 */
    int automatic = 0;        /* 块作用域 + 自动存储期：每次进函数都重来 */
    calls++;
    automatic++;
    printf("    counter(): static calls=%d, automatic=%d\n", calls, automatic);
}

int main(void)
{
    puts("== 1. sizeof 基本类型（单位：字节）==");
    printf("  char=%zu  short=%zu  int=%zu  long=%zu  long long=%zu\n",
           sizeof(char), sizeof(short), sizeof(int),
           sizeof(long), sizeof(long long));
    printf("  float=%zu double=%zu long double=%zu _Bool=%zu\n",
           sizeof(float), sizeof(double), sizeof(long double), sizeof(_Bool));
    printf("  void*=%zu  size_t=%zu  ptrdiff_t=%zu  intptr_t=%zu\n",
           sizeof(void *), sizeof(size_t), sizeof(ptrdiff_t), sizeof(intptr_t));
    printf("  CHAR_BIT = %d  (一个 char 有几个 bit)\n", CHAR_BIT);

    puts("\n== 2. 取值范围 ==");
    printf("  INT_MIN=%d  INT_MAX=%d\n", INT_MIN, INT_MAX);
    printf("  UINT_MAX=%u\n", UINT_MAX);
    printf("  LONG_MIN=%ld LONG_MAX=%ld\n", LONG_MIN, LONG_MAX);
    printf("  CHAR_MIN=%d CHAR_MAX=%d  -> 本平台 char 是%s的\n",
           CHAR_MIN, CHAR_MAX, (CHAR_MIN < 0) ? "有符号" : "无符号");
    printf("  DBL_DIG=%d FLT_DIG=%d\n", DBL_DIG, FLT_DIG);

    puts("\n== 3. 字面量的类型与进制 ==");
    printf("  10=%d  010(八进制)=%d  0x10(十六进制)=%d  0b? C17 无二进制字面量\n",
           10, 010, 0x10);
    printf("  sizeof('A') = %zu  <-- C 里字符字面量是 int，不是 char！\n", sizeof('A'));
    printf("  sizeof(\"abc\") = %zu <-- 3 个字符 + 1 个 '\\0'\n", sizeof("abc"));
    printf("  1/2 = %d   (整数除法，截断)\n", 1 / 2);
    printf("  1.0/2 = %g (有一个操作数是 double，整体升成 double)\n", 1.0 / 2);
    printf("  7 %% 3 = %d   -7 %% 3 = %d (C99 起商向零截断)\n", 7 % 3, -7 % 3);

    puts("\n== 4. 无符号回绕是「有定义」的，有符号溢出是 UB ==");
    unsigned int u = UINT_MAX;
    printf("  UINT_MAX + 1 = %u   <-- 模 2^32 回绕，标准明确规定\n", u + 1u);
    unsigned char uc = 250;
    uc = (unsigned char)(uc + 10);
    printf("  (unsigned char)250 + 10 = %u  <-- 模 2^8 回绕\n", uc);
    printf("  有符号溢出（INT_MAX+1）是 undefined behavior，见 08_ub 实验\n");

    puts("\n== 5. 整数提升与「有符号 vs 无符号」陷阱 ==");
    int      i = -1;
    unsigned v = 1u;
    printf("  int i = -1;  unsigned v = 1u;\n");
    printf("  (unsigned)i = %u  <-- -1 的补码被当成巨大的正数\n", (unsigned)i);
    printf("  i < v 的真实结果 = %s  <-- 直觉上 -1 < 1 应该为真！\n",
           ((unsigned)i < v) ? "true" : "false");
    printf("  原因：usual arithmetic conversions 把 int 转成了 unsigned int\n");

    puts("\n== 6. 作用域与存储期 ==");
    int shadow = 1;
    printf("  外层 shadow = %d\n", shadow);
    {
        int shadow_inner = 2;   /* 换个名字避免 -Wshadow 争议，但演示嵌套作用域 */
        printf("  内层块里可以再定义变量 shadow_inner = %d\n", shadow_inner);
    }
    printf("  离开内层块后 shadow 仍是 %d\n", shadow);
    counter();
    counter();
    counter();

    puts("\n== 7. 未初始化 vs 已初始化 ==");
    printf("  静态变量 g_zero_init = %d  (标准保证清零)\n", g_zero_init);
    int initialized = 0;
    printf("  局部变量必须自己初始化，否则读取它就是 UB（见 08_ub）\n");
    printf("  initialized = %d\n", initialized);

    puts("\n== 8. const / static / extern ==");
    const int  ci = 10;
    printf("  const int ci = %d  <-- 只读对象，改它是 UB\n", ci);
    printf("  g_internal(static, 内部链接) = %d\n", g_internal);
    printf("  g_external(外部链接)         = %d\n", g_external);

    puts("\n== 9. bool 与真值 ==");
    bool b = (0.1 + 0.2 == 0.3);
    printf("  0.1 + 0.2 == 0.3 ? %s   <-- 浮点不能用 == 比较\n", b ? "true" : "false");
    printf("  0.1 + 0.2 = %.20f\n", 0.1 + 0.2);
    printf("  fabs 差值判断才对：|(0.1+0.2)-0.3| = %.20f\n",
           (0.1 + 0.2) - 0.3 < 0 ? 0.3 - (0.1 + 0.2) : (0.1 + 0.2) - 0.3);

    return 0;
}
