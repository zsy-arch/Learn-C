/* m01_regions.c —— 一个进程的内存分区：代码、常量、全局、堆、栈 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int         g_initialized   = 1;    /* .data   已初始化全局 */
int         g_uninitialized;        /* .bss    未初始化全局（运行时清零） */
const int   g_const         = 42;   /* 只读数据段 */
static int  g_static        = 7;

static void dummy(void) { }

static void print_row(const char *what, const void *addr, const char *seg)
{
    printf("  %-26s %p   %s\n", what, addr, seg);
}

static void recurse(int depth, const void *top)
{
    volatile char frame[256];       /* 每层吃掉一点栈 */
    frame[0] = (char)depth;
    if (depth == 0) {
        printf("  递归 0 层时栈顶约 %p\n", (const void *)frame);
    }
    if (depth < 20) {
        recurse(depth + 1, top);
    } else {
        printf("  递归 20 层时栈顶约 %p，共下探约 %td 字节\n",
               (const void *)frame, (const char *)top - (const char *)frame);
    }
}

int main(void)
{
    int    stack_var   = 0;
    char  *heap_small  = malloc(16);
    char  *heap_big    = malloc(1024 * 1024);
    const char *literal = "I live in read-only memory";

    puts("== 1. 各个区的典型地址（地址从小到大排列）==");
    print_row("函数代码 main",        (const void *)(uintptr_t)main,  "__TEXT  代码段(可执行，只读)");
    print_row("函数代码 dummy",       (const void *)(uintptr_t)dummy, "__TEXT");
    print_row("字符串字面量",          (const void *)literal,   "__TEXT/__cstring 只读");
    print_row("const 全局 g_const",   (const void *)&g_const,  "__DATA_CONST 只读");
    print_row("全局 g_initialized",   (void *)&g_initialized,  "__DATA  已初始化");
    print_row("static g_static",      (void *)&g_static,       "__DATA");
    print_row("全局 g_uninitialized", (void *)&g_uninitialized,"__DATA/__bss 运行时清零");
    print_row("堆 malloc(16)",        (void *)heap_small,      "heap    向高地址增长");
    print_row("堆 malloc(1MB)",       (void *)heap_big,        "heap    大块可能单独 mmap");
    print_row("栈 局部变量",           (void *)&stack_var,      "stack   向低地址增长");

    puts("\n== 2. 栈的方向 ==");
    volatile char here = 0;
    recurse(0, (const void *)&here);

    puts("\n== 3. 栈 vs 堆 的对比 ==");
    puts("  ┌────────┬──────────────────┬──────────────────────┐");
    puts("  │        │ 栈 (stack)       │ 堆 (heap)            │");
    puts("  ├────────┼──────────────────┼──────────────────────┤");
    puts("  │ 分配   │ 移动栈指针，极快  │ malloc 要找空闲块，慢 │");
    puts("  │ 释放   │ 函数返回自动回收  │ 必须手动 free        │");
    puts("  │ 大小   │ 有限(通常 8MB)   │ 受物理内存/地址空间限 │");
    puts("  │ 生命期 │ 到函数返回为止    │ 到你 free 为止        │");
    puts("  │ 越界   │ 破坏别的栈帧      │ 破坏堆元数据         │");
    puts("  └────────┴──────────────────┴──────────────────────┘");

    puts("\n== 4. 栈大小是有限的 ==");
    printf("  可以用 ulimit -s 查看（macOS 默认主线程 8192 KB）\n");
    printf("  int big[4*1024*1024]; 作为局部变量会直接栈溢出崩溃\n");
    printf("  大数组请用 malloc 放到堆上，或者声明成 static\n");

    puts("\n== 5. 生命周期（lifetime）三种 ==");
    puts("  自动存储期 automatic : 局部变量，进入块时创建，离开块销毁");
    puts("  静态存储期 static    : 全局/static 变量，程序全程存在");
    puts("  分配存储期 allocated : malloc 得到的，从 malloc 到 free");

    free(heap_big);
    free(heap_small);
    return 0;
}
