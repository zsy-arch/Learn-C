/* u04_uninit.c —— UB #4：读取未初始化的变量
 * macOS/arm64 上没有 MemorySanitizer，这里用编译器警告 + 多次运行观察。 */
#include <stdio.h>

static int leak_stack_garbage(void)
{
    int arr[8];
    int sum = 0;
    for (int i = 0; i < 8; i++) {
        sum += arr[i];      /* 读未初始化的栈内存，UB */
    }
    return sum;
}

/* 先往栈上写点东西，再看下一个函数能不能"捡到" */
static void dirty_the_stack(void)
{
    volatile int junk[8];
    for (int i = 0; i < 8; i++) { junk[i] = 0x11111111 * (i + 1); }
}

int main(void)
{
    puts("== 1. 未初始化的局部变量 ==");
    int x;                      /* 没有初始化 */
    printf("  未初始化的 x = %d   <-- 值不确定，读它就是 UB\n", x);

    puts("\n== 2. 未初始化的指针（野指针）==");
    int *p;
    printf("  未初始化的 p = %p   <-- 解引用它会发生什么，没人能保证\n", (void *)p);

    puts("\n== 3. 栈上的「垃圾」其实是上一个函数留下的数据 ==");
    printf("  干净调用: leak_stack_garbage() = %d\n", leak_stack_garbage());
    dirty_the_stack();
    printf("  脏栈之后: leak_stack_garbage() = %d\n", leak_stack_garbage());
    printf("  如果两次结果不同，说明你读到的是别的函数的残留数据\n");
    printf("  在真实程序里，这可能是密码、密钥、指针……这就是信息泄漏漏洞\n");

    puts("\n== 4. 正确写法 ==");
    int y = 0;
    int *q = NULL;
    int arr[8] = {0};
    printf("  int y = 0;  int *q = NULL;  int arr[8] = {0};\n");
    printf("  y=%d q=%p arr[0]=%d —— 定义即初始化，零成本换确定性\n",
           y, (void *)q, arr[0]);

    return 0;
}
