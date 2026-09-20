/* u07_misc.c —— 其他几类常见 UB：移位、除零、序列点、对齐 */
#include <stdio.h>
#include <limits.h>

static volatile int g_sink;

int main(int argc, char **argv)
{
    const char *mode = (argc > 1) ? argv[1] : "shift";
    printf("模式: %s  (可选: shift | divzero | seq)\n", mode);

    if (mode[0] == 's' && mode[1] == 'h') {
        puts("== 移位越界 ==");
        volatile int x = 1;
        volatile int n = 32;                 /* int 只有 32 位 */
        printf("  1 << 32 是 UB（移位量必须 < 类型位宽）\n");
        int r = x << n;                      /* UB */
        g_sink = r;
        printf("  本次得到 %d\n", r);

        volatile int neg = -1;
        volatile int one = 1;
        printf("  -1 << 1 在 C17 里也是 UB（左移有符号负数）\n");
        int r2 = neg << one;                 /* UB */
        g_sink = r2;
        printf("  本次得到 %d\n", r2);
        printf("  正确做法：需要位操作时一律用 unsigned 类型\n");

    } else if (mode[0] == 'd') {
        puts("== 除以零 ==");
        volatile int a = 1;
        volatile int b = 0;
        printf("  即将计算 %d / %d\n", a, b);
        fflush(stdout);
        int r = a / b;                       /* UB */
        g_sink = r;
        printf("  结果 %d\n", r);

    } else {
        puts("== 同一表达式内多次修改同一对象（unsequenced）==");
        int i = 0;
        int arr[5] = {0, 1, 2, 3, 4};
        printf("  i = i++ + ++i;      // UB\n");
        printf("  arr[i] = i++;       // UB\n");
        printf("  printf(\"%%d %%d\", i++, i++);  // 参数求值顺序未指定\n");
        printf("  这类写法在不同编译器/优化级别下结果不同，永远不要写。\n");
        printf("  拆成两行就完全没问题：\n");
        i = 0;
        arr[i] = i;
        i++;
        printf("  arr[0]=%d, i=%d\n", arr[0], i);
    }

    puts("\n== 一份常见 UB 清单 ==");
    puts("   1. 有符号整数溢出");
    puts("   2. 数组/指针越界访问");
    puts("   3. 解引用空指针 / 野指针 / 悬垂指针");
    puts("   4. 读取未初始化的对象");
    puts("   5. free 两次 / free 非堆指针");
    puts("   6. 修改字符串字面量或 const 对象");
    puts("   7. 移位量 >= 位宽，或左移负数");
    puts("   8. 整数除以零 / 取模零");
    puts("   9. 同一表达式内无序地多次修改同一对象");
    puts("  10. 违反严格别名规则");
    puts("  11. memcpy 源和目的重叠（应该用 memmove）");
    puts("  12. 非 void 函数走到结尾却没 return");
    puts("  13. printf 格式串与实参类型不匹配");
    puts("  14. 未对齐的指针解引用");
    return 0;
}
