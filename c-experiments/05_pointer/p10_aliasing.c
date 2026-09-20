/* p10_aliasing.c —— 严格别名规则（strict aliasing）入门
 *
 * 规则：除了 char/unsigned char（以及 C17 起的兼容类型、union 成员等少数例外），
 * 不允许通过「和对象实际类型不兼容的左值」去访问该对象。
 * 编译器据此假设 int* 和 float* 不会指向同一块内存，从而做激进优化。
 *
 * 这个文件分别用 -O0 和 -O2 编译，观察输出是否不同。
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* 反例：通过 float* 去读写一个 int 对象 —— 违反严格别名 */
static int bad_punning(int *pi, float *pf)
{
    *pi = 1;              /* 写 int */
    *pf = 2.0f;           /* 写 float —— 编译器认为它不可能影响 *pi */
    return *pi;           /* 可能被优化成「直接返回 1」 */
}

/* 正确做法 1：memcpy，编译器会优化掉，零成本 */
static uint32_t float_bits_memcpy(float f)
{
    uint32_t bits;
    memcpy(&bits, &f, sizeof(bits));
    return bits;
}

/* 正确做法 2：union 类型双关，C（不同于 C++）明确允许读非活跃成员 */
static uint32_t float_bits_union(float f)
{
    union { float f; uint32_t u; } u;
    u.f = f;
    return u.u;
}

/* 正确做法 3：用 unsigned char* 逐字节访问，永远合法 */
static uint32_t float_bits_bytes(float f)
{
    const unsigned char *p = (const unsigned char *)&f;
    uint32_t bits = 0;
    for (size_t i = 0; i < sizeof(float); i++) {
        bits |= (uint32_t)p[i] << (8 * i);   /* 假定 little-endian */
    }
    return bits;
}

int main(void)
{
    puts("== 1. 违反严格别名的后果依赖优化级别 ==");
    int storage = 0;
    int r = bad_punning(&storage, (float *)&storage);
    printf("  bad_punning 返回 %d\n", r);
    printf("  storage 的字节 = 0x%08X\n", (unsigned)storage);
    printf("  如果 -O0 和 -O2 结果不同，说明这段代码依赖了 UB\n");

    puts("\n== 2. 三种合法的 type punning ==");
    float f = 1.0f;
    printf("  f = %g\n", (double)f);
    printf("  memcpy 版本 : 0x%08X\n", float_bits_memcpy(f));
    printf("  union  版本 : 0x%08X\n", float_bits_union(f));
    printf("  字节   版本 : 0x%08X\n", float_bits_bytes(f));
    printf("  IEEE-754 单精度 1.0 应为 0x3F800000\n");

    puts("\n== 3. 最佳实践 ==");
    puts("  - 需要按位解释一个对象时，用 memcpy（首选）或 union");
    puts("  - 永远不要写 *(float*)&some_int");
    puts("  - 遍历对象表示只用 unsigned char*");
    puts("  - 编译时加 -fno-strict-aliasing 只能救老代码，不能当成解决方案");

    return 0;
}
