/* u05_nullderef.c —— UB #5：解引用空指针 */
#include <stdio.h>
#include <stdlib.h>

struct Node { int value; struct Node *next; };

static int bad_sum(const struct Node *n)
{
    /* 忘了判空 */
    return n->value + (n->next ? bad_sum(n->next) : 0);
}

static int good_sum(const struct Node *n)
{
    if (n == NULL) { return 0; }
    return n->value + good_sum(n->next);
}

int main(int argc, char **argv)
{
    const char *mode = (argc > 1) ? argv[1] : "safe";
    printf("模式: %s  (可选: safe | crash)\n", mode);

    struct Node b = {2, NULL};
    struct Node a = {1, &b};

    printf("good_sum(&a)  = %d\n", good_sum(&a));
    printf("good_sum(NULL) = %d   <-- 判空之后很安全\n", good_sum(NULL));

    if (mode[0] == 'c') {
        puts("即将调用 bad_sum(NULL) —— 解引用空指针");
        fflush(stdout);              /* 崩溃前把缓冲区刷出来，否则输出会丢 */
        printf("%d\n", bad_sum(NULL));
        puts("不会执行到这里");
    }

    puts("\n要点:");
    puts("  1) 解引用 NULL 在多数平台会 SIGSEGV，但标准只说是 UB —— 不保证崩溃");
    puts("  2) 优化器看到 n->value 就会「推断 n 一定非空」，从而删掉后面的判空分支");
    puts("  3) 所有可能返回 NULL 的 API（malloc/fopen/strchr...）都必须检查返回值");
    return 0;
}
