/* s06_list.c —— 自引用结构体与单链表 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 自引用：结构体内部可以有「指向自己类型的指针」，
 * 因为指针大小是已知的，不需要完整类型。 */
typedef struct Node {
    int          value;
    struct Node *next;      /* 这里必须写 struct Node，typedef 名此时还没生效 */
} Node;

typedef struct {
    Node  *head;
    size_t size;
} List;

static void list_init(List *l)
{
    l->head = NULL;
    l->size = 0;
}

/* 头插：O(1) */
static int list_push_front(List *l, int v)
{
    Node *n = malloc(sizeof *n);        /* sizeof *n 比 sizeof(Node) 更抗改名 */
    if (n == NULL) { return -1; }
    n->value = v;
    n->next  = l->head;
    l->head  = n;
    l->size++;
    return 0;
}

/* 尾插：O(n)，演示「二级指针遍历」这个经典技巧 */
static int list_push_back(List *l, int v)
{
    Node *n = malloc(sizeof *n);
    if (n == NULL) { return -1; }
    n->value = v;
    n->next  = NULL;

    Node **cur = &l->head;              /* 指向「要修改的那个指针」 */
    while (*cur != NULL) {
        cur = &(*cur)->next;
    }
    *cur = n;                           /* 不需要区分「空表」和「非空表」 */
    l->size++;
    return 0;
}

/* 删除所有等于 v 的节点：二级指针让删除头节点不需要特判 */
static size_t list_remove(List *l, int v)
{
    size_t removed = 0;
    Node **cur = &l->head;
    while (*cur != NULL) {
        Node *entry = *cur;
        if (entry->value == v) {
            *cur = entry->next;         /* 把前驱的 next 直接接到后继 */
            free(entry);
            l->size--;
            removed++;
        } else {
            cur = &entry->next;
        }
    }
    return removed;
}

static void list_reverse(List *l)
{
    Node *prev = NULL;
    Node *cur  = l->head;
    while (cur != NULL) {
        Node *next = cur->next;
        cur->next  = prev;
        prev       = cur;
        cur        = next;
    }
    l->head = prev;
}

static void list_free(List *l)
{
    Node *cur = l->head;
    while (cur != NULL) {
        Node *next = cur->next;         /* 必须先存 next，free 之后就读不到了 */
        free(cur);
        cur = next;
    }
    l->head = NULL;
    l->size = 0;
}

static void list_print(const char *tag, const List *l)
{
    printf("  %-14s size=%zu  ", tag, l->size);
    for (const Node *c = l->head; c != NULL; c = c->next) {
        printf("%d -> ", c->value);
    }
    puts("NULL");
}

int main(void)
{
    puts("== 1. 自引用结构体 ==");
    printf("  sizeof(Node) = %zu  (int + padding + 指针)\n", sizeof(Node));
    printf("  struct Node 里可以放 struct Node*，但不能放 struct Node（无限递归）\n");

    List l;
    list_init(&l);

    puts("\n== 2. 头插 ==");
    for (int i = 1; i <= 3; i++) { list_push_front(&l, i); }
    list_print("push_front", &l);

    puts("\n== 3. 尾插（二级指针遍历）==");
    for (int i = 7; i <= 9; i++) { list_push_back(&l, i); }
    list_print("push_back", &l);

    puts("\n== 4. 节点在堆上的实际地址 ==");
    for (const Node *c = l.head; c != NULL; c = c->next) {
        printf("    node@%p value=%d next=%p\n",
               (const void *)c, c->value, (const void *)c->next);
    }

    puts("\n== 5. 删除（含删除头节点，无需特判）==");
    list_push_back(&l, 3);
    list_print("before remove", &l);
    printf("  删除了 %zu 个值为 3 的节点\n", list_remove(&l, 3));
    list_print("after remove", &l);

    puts("\n== 6. 反转 ==");
    list_reverse(&l);
    list_print("reversed", &l);

    puts("\n== 7. 释放整条链表 ==");
    list_free(&l);
    list_print("after free", &l);

    puts("\n== 最佳实践 ==");
    puts("  1) 遍历删除时用 Node** cur = &head，可以省掉所有头节点特判");
    puts("  2) free(cur) 之前一定先把 cur->next 存下来");
    puts("  3) 用 malloc(sizeof *n) 而不是 malloc(sizeof(Node))，改类型名时不会错");
    puts("  4) 链表的 size 自己维护，不要每次 O(n) 数一遍");

    return 0;
}
