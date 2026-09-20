/* a01_arrays.c —— 数组：初始化、长度、多维、VLA */
#include <stdio.h>
#include <string.h>

#define LEN(a) (sizeof(a) / sizeof((a)[0]))   /* 只能对「真数组」用，不能对指针用 */

static void print_row(const int *row, size_t n)
{
    printf("[");
    for (size_t i = 0; i < n; i++) { printf("%s%2d", i ? ", " : "", row[i]); }
    printf("]");
}

/* 二维数组作参数：除了第一维，其他维度必须写明 */
static int sum2d(const int m[][4], size_t rows)
{
    int s = 0;
    for (size_t i = 0; i < rows; i++) {
        for (size_t j = 0; j < 4; j++) { s += m[i][j]; }
    }
    return s;
}

int main(void)
{
    puts("== 1. 初始化的几种写法 ==");
    int a1[5] = {1, 2, 3, 4, 5};
    int a2[5] = {1, 2};          /* 剩下的自动补 0 */
    int a3[5] = {0};             /* 全部清零的惯用法 */
    int a4[]  = {1, 2, 3};       /* 长度由初始化器推断 */
    int a5[5] = {[4] = 9, [0] = 1};   /* 指定初始化器（C99） */

    printf("  int a1[5]={1,2,3,4,5} -> "); print_row(a1, LEN(a1)); putchar('\n');
    printf("  int a2[5]={1,2}       -> "); print_row(a2, LEN(a2)); puts("   <-- 后面补 0");
    printf("  int a3[5]={0}         -> "); print_row(a3, LEN(a3)); putchar('\n');
    printf("  int a4[]={1,2,3}      -> "); print_row(a4, LEN(a4));
    printf("   长度 = %zu（编译器数出来的）\n", LEN(a4));
    printf("  int a5[5]={[4]=9,[0]=1} -> "); print_row(a5, LEN(a5)); putchar('\n');

    puts("\n== 2. 数组不能整体赋值，也不能整体比较 ==");
    int b[5];
    /* b = a1;            <-- 编译错误 */
    memcpy(b, a1, sizeof a1);    /* 要用 memcpy */
    printf("  memcpy(b, a1, sizeof a1) -> "); print_row(b, LEN(b)); putchar('\n');
    printf("  memcmp(b, a1, sizeof a1) = %d (0 表示逐字节相同)\n",
           memcmp(b, a1, sizeof a1));
    printf("  但结构体可以整体赋值，数组不行 —— 这是 C 的历史包袱\n");

    puts("\n== 3. 二维数组是「数组的数组」，内存里是行优先连续的 ==");
    int m[3][4] = {
        {1,  2,  3,  4},
        {5,  6,  7,  8},
        {9, 10, 11, 12},
    };
    printf("  sizeof(m)    = %zu  (3*4*4)\n", sizeof m);
    printf("  sizeof(m[0]) = %zu  (一行 4 个 int)\n", sizeof m[0]);
    printf("  sizeof(m[0][0]) = %zu\n", sizeof m[0][0]);
    printf("  行数 = %zu, 列数 = %zu\n", sizeof m / sizeof m[0],
           sizeof m[0] / sizeof m[0][0]);

    printf("  内存中的实际顺序: ");
    const int *flat = &m[0][0];
    for (size_t i = 0; i < 12; i++) { printf("%d ", flat[i]); }
    puts("  <-- 行优先(row-major)");

    printf("  &m[0][0]=%p\n", (const void *)&m[0][0]);
    printf("  &m[1][0]=%p  差 %td 字节 = 一整行\n",
           (const void *)&m[1][0],
           (const char *)&m[1][0] - (const char *)&m[0][0]);
    printf("  sum2d(m, 3) = %d\n", sum2d(m, 3));

    puts("\n== 4. m[i][j] 的地址计算 ==");
    puts("  m[i][j] 等价于 *(*(m + i) + j)");
    puts("  地址 = (char*)m + (i * 列数 + j) * sizeof(元素)");
    size_t i = 2, j = 1;
    printf("  m[2][1] = %d，手算地址 = %p，实际地址 = %p\n",
           m[i][j],
           (const void *)((const char *)m + (i * 4 + j) * sizeof(int)),
           (const void *)&m[i][j]);

    puts("\n== 5. VLA（变长数组，C99 引入，C11 起为可选特性）==");
    int n = 4;
    int vla[n];                   /* 长度在运行时确定 */
    for (int k = 0; k < n; k++) { vla[k] = k * k; }
    printf("  int vla[n] (n=%d): ", n);
    print_row(vla, (size_t)n);
    printf("   sizeof(vla) = %zu（运行时计算）\n", sizeof vla);
    puts("  注意: VLA 在栈上分配，n 很大时会栈溢出；");
    puts("        MSVC 不支持；C11 起是可选特性（__STDC_NO_VLA__）。");
    puts("        生产代码里建议用 malloc 代替 VLA。");

    puts("\n== 6. LEN 宏的陷阱 ==");
    printf("  在 main 里 LEN(a1) = %zu  正确\n", LEN(a1));
    puts("  一旦把 a1 传给函数，函数里 LEN(参数) = 8/4 = 2，完全错误！");
    puts("  这就是 05_pointer/p02_decay 讲的数组退化。");

    return 0;
}
