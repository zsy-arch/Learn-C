/* guard_check —— 单独一个进程，用来验证 btree_create 对非法 t 的守卫还在。
 *
 * 为什么不放在 tests.c 里：守卫的做法是打一行 stderr 然后 exit(1)，同进程内
 * 测不了，只能 fork 子进程看退出码。但 macOS 的 `leaks --atExit` 撞上 fork 会
 * 永久挂死（实测 90 秒仍未返回），而本平台 LeakSanitizer 不支持 arm64，
 * `leaks` 是唯一能用的泄漏检测手段。为了不把 tests.c 变成漏检死角，这个用例
 * 搬成独立可执行文件，由 Makefile 的 `guard` 目标驱动。
 *
 * 语义刻意反着写：
 *   exit(0) —— btree_create 放行了非法的 t，**守卫失效**；
 *   exit(1) —— 守卫自己打印诊断并退出，**符合预期**。
 * 所以 Makefile 里的断言是 "这条命令必须失败"。
 *
 * 为什么这个守卫值得单独守：t <= 0 时 node_create 里的 `2 * t - 1` 是负数，
 * 强转 size_t 之后变成接近 SIZE_MAX 的巨大值；真正的错误（传了非法的 t）和
 * 最终暴露出来的现象（malloc 失败或写越界）隔着好几层。
 *
 * 用法：./guard_check <t>
 */
#include <stdio.h>
#include <stdlib.h>

#include "btree.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "用法: %s <t>\n", argv[0]);
        return 2;
    }
    char *end = NULL;
    long t = strtol(argv[1], &end, 10);
    if (end == argv[1] || *end != '\0' || t > 1000000L || t < -1000000L) {
        fprintf(stderr, "%s: 无法解析的 t: %s\n", argv[0], argv[1]);
        return 2;
    }

    BTree tree = btree_create((int)t);

    /* 走到这里说明 btree_create 放行了。t >= 2 时这是对的；t < 2 时守卫失效。 */
    if (t < 2) {
        printf("guard_check: btree_create(%ld) 返回了（root=%p）——守卫失效\n",
               t, (void *)tree.root);
    } else {
        printf("guard_check: btree_create(%ld) 正常返回（t >= 2，本就该放行）\n", t);
    }
    btree_destroy(&tree);
    return 0;
}
