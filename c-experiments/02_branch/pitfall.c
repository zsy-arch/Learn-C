/* pitfall.c —— 分支三大反例：悬空 else、意外 fallthrough、= 写成 ==
 * 故意不加 -Werror 编译，用来观察编译器警告。 */
#include <stdio.h>

static void dangling_else(int a, int b)
{
    /* 危险：else 到底属于哪个 if？缩进在骗人，else 绑定的是「最近的」if。 */
    if (a > 0)
        if (b > 0)
            printf("  a>0 且 b>0\n");
    else                                   /* 看起来配 if(a>0)，实际配 if(b>0) */
        printf("  <-- 这行真正的含义是: a>0 但 b<=0\n");
}

static void accidental_fallthrough(int cmd)
{
    switch (cmd) {
    case 1:
        printf("  case 1 执行\n");
        /* 忘记 break！ */
    case 2:
        printf("  case 2 也被执行了（fallthrough）\n");
        break;
    default:
        printf("  default\n");
        break;
    }
}

int main(void)
{
    puts("== 悬空 else ==");
    printf(" dangling_else(1, -1):\n");
    dangling_else(1, -1);
    printf(" dangling_else(-1, 1): (什么都不打印)\n");
    dangling_else(-1, 1);

    puts("\n== 意外 fallthrough ==");
    printf(" accidental_fallthrough(1):\n");
    accidental_fallthrough(1);

    puts("\n== 把 == 写成 = ==");
    int x = 0;
    if ((x = 5)) {              /* 加一层括号是「我故意的」的惯用写法 */
        printf("  if (x = 5) 恒为真，而且把 x 改成了 %d\n", x);
    }
    return 0;
}
