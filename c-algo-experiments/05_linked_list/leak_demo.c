/* leak_demo.c —— ⚠️ 错误示例：链表中三种典型的内存错误
 *
 * 这个文件【故意】写出泄漏和悬垂指针，用来演示工具能抓到什么。
 * 绝对不要照抄进真实代码。
 *
 * 三种错误：
 *   1. free_list 里忘了先存 next        -> use-after-free
 *   2. 删除节点后没有 free              -> 内存泄漏
 *   3. 删除节点后继续用那个指针          -> 悬垂指针
 *
 * 用参数选择要演示哪一个：
 *   ./leak_demo leak      内存泄漏
 *   ./leak_demo uaf       free 之后继续用
 *   ./leak_demo dangling  删除后继续读
 *   ./leak_demo safe      正确写法（对照组）
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node {
    int          value;
    struct Node *next;
} Node;

static Node *push(Node *head, int v)
{
    Node *n = malloc(sizeof *n);
    if (n == NULL) { return head; }
    n->value = v;
    n->next = head;
    return n;
}

/* ❌ 错误 1：free 之前没保存 next */
static void free_list_broken(Node *head)
{
    Node *cur = head;
    while (cur != NULL) {
        free(cur);
        cur = cur->next;      /* ← use-after-free！cur 已经在读被释放的节点 */
    }
}

/* ✅ 正确：先存 next */
static void free_list_ok(Node *head)
{
    Node *cur = head;
    while (cur != NULL) {
        Node *next = cur->next;   /* 先保存 */
        free(cur);
        cur = next;
    }
}

/* ❌ 错误 2：删除节点但忘记 free */
static Node *remove_leak(Node *head, int v)
{
    if (head == NULL) { return NULL; }
    if (head->value == v) {
        Node *n = head->next;
        /* 忘了 free(head); —— 泄漏 */
        return n;
    }
    for (Node *c = head; c->next != NULL; c = c->next) {
        if (c->next->value == v) {
            c->next = c->next->next;
            /* 忘了 free —— 泄漏 */
            return head;
        }
    }
    return head;
}

/* ❌ 错误 3：删除后继续用被删的指针 */
static Node *dangling_demo(Node *head)
{
    Node *victim = head;          /* 假设要删第一个 */
    head = victim->next;
    free(victim);
    /* 从这里开始 victim 是悬垂指针 */
    printf("  ⚠️  读取已释放节点的 value = %d （UB，可能看起来完全正常）\n",
           victim->value);
    return head;
}

int main(int argc, char **argv)
{
    const char *mode = (argc > 1) ? argv[1] : "safe";
    printf("模式: %s\n\n", mode);

    if (strcmp(mode, "leak") == 0) {
        puts("== 场景 1：删除节点时忘记 free ==");
        Node *h = NULL;
        for (int i = 1; i <= 5; i++) { h = push(h, i); }
        printf("  建了 5 个节点，每个 %zu 字节 = %zu 字节\n",
               sizeof(Node), 5 * sizeof(Node));
        h = remove_leak(h, 3);
        printf("  删除了值为 3 的节点（但没 free）\n");
        free_list_ok(h);          /* 这里只释放剩下的 4 个 */
        puts("  -> 有 1 个节点永远不会被释放。用 leaks 工具能抓到。");
        return 0;
    }

    if (strcmp(mode, "uaf") == 0) {
        puts("== 场景 2：free 之后继续读 next ==");
        Node *h = NULL;
        for (int i = 1; i <= 4; i++) { h = push(h, i); }
        printf("  释放整条链表（错误写法）...\n");
        fflush(stdout);
        free_list_broken(h);
        puts("  -> 竟然没崩？因为小块内存释放后还在进程地址空间里。");
        puts("     但每个节点被 free 后，它的 next 字段可能已经被 allocator");
        puts("     改写成 freelist 指针了。ASan 会精确报出 use-after-free。");
        return 0;
    }

    if (strcmp(mode, "dangling") == 0) {
        puts("== 场景 3：删除后继续用被删的指针 ==");
        Node *h = NULL;
        for (int i = 1; i <= 3; i++) { h = push(h, i); }
        h = dangling_demo(h);     /* h 指向第二个节点，第一个已被释放 */
        fflush(stdout);
        free_list_ok(h);
        puts("  -> 打印出了看似正确的值，这正是悬垂指针最危险的地方。");
        return 0;
    }

    /* safe：对照组 */
    puts("== 正确写法 ==");
    Node *h = NULL;
    for (int i = 1; i <= 5; i++) { h = push(h, i); }
    printf("  链表: ");
    for (Node *c = h; c != NULL; c = c->next) { printf("%d -> ", c->value); }
    puts("NULL");
    free_list_ok(h);
    puts("  已全部释放，0 泄漏。");
    puts("");
    puts("  三条纪律：");
    puts("    1. free(cur) 之前先存 cur->next");
    puts("    2. 每个 malloc 都要有配对的 free");
    puts("    3. free 之后立刻把指针置 NULL（或让它离开作用域）");
    return 0;
}
