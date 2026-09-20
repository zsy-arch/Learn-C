/* demo.c —— 单链表：头插、尾插、查找、删除、反转
 *
 * 本实验的核心问题：
 *   1. 为什么遍历删除要对头节点特判？能不能不特判？
 *   2. 反转链表的三指针法到底在干什么？
 *   3. 为什么链表节点的内存地址是「乱」的？
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

/* ---------------------------------------------------------------- */
/* 节点定义                                                            */
/* ---------------------------------------------------------------- */
typedef struct Node {
    int          value;
    struct Node *next;      /* 自引用：必须写 struct Node，typedef 名此刻还没生效 */
} Node;

/* ---------------------------------------------------------------- */
/* 1. 头插 —— O(1)                                                    */
/* ---------------------------------------------------------------- */
/*
 * 为什么要传 Node**？
 *   因为头插要【修改调用者的 head 指针本身】。
 *   C 只有值传递（见《C 语言语法与最佳实践》第七章），
 *   传 Node* 只能改节点内容，改不了「head 指向谁」。
 *   想修改 T，就传 T* —— 这里 T 是 Node*，所以要传 Node**。
 */
static bool push_front(Node **head, int value)
{
    if (head == NULL) { return false; }
    Node *n = malloc(sizeof *n);       /* sizeof *n 比 sizeof(Node) 更抗改名 */
    if (n == NULL) { return false; }
    n->value = value;
    n->next  = *head;                  /* 新节点指向原来的头 */
    *head    = n;                      /* 头指针改指向新节点 */
    return true;
}

/* ---------------------------------------------------------------- */
/* 2. 尾插 —— O(n)（没有尾指针时）                                      */
/* ---------------------------------------------------------------- */
/* 朴素写法：先找尾，再挂上去 */
static bool push_back_naive(Node **head, int value)
{
    if (head == NULL) { return false; }
    Node *n = malloc(sizeof *n);
    if (n == NULL) { return false; }
    n->value = value;
    n->next  = NULL;

    if (*head == NULL) {          /* 空表：特判 */
        *head = n;
        return true;
    }
    Node *cur = *head;
    while (cur->next != NULL) { cur = cur->next; }
    cur->next = n;
    return true;
}

/* 二级指针版本：不需要特判空表！ */
static bool push_back_pp(Node **head, int value)
{
    if (head == NULL) { return false; }
    Node *n = malloc(sizeof *n);
    if (n == NULL) { return false; }
    n->value = value;
    n->next  = NULL;

    Node **cur = head;                 /* cur 指向「那个需要被赋值的指针变量」 */
    while (*cur != NULL) {
        cur = &(*cur)->next;           /* 前进到下一个指针变量 */
    }
    *cur = n;                          /* 空表时 *cur 就是 *head，无需特判 */
    return true;
}

/* ---------------------------------------------------------------- */
/* 3. 查找                                                             */
/* ---------------------------------------------------------------- */
static Node *find(Node *head, int value)
{
    for (Node *c = head; c != NULL; c = c->next) {
        if (c->value == value) { return c; }
    }
    return NULL;
}

/* ---------------------------------------------------------------- */
/* 4. 删除 —— 两种写法的对比                                            */
/* ---------------------------------------------------------------- */
/* 4a. 朴素写法：必须特判头节点 */
static bool remove_naive(Node **head, int value)
{
    if (head == NULL || *head == NULL) { return false; }

    if ((*head)->value == value) {     /* 特判：删的是头节点 */
        Node *victim = *head;
        *head = victim->next;
        free(victim);
        return true;
    }
    Node *prev = *head;
    while (prev->next != NULL) {
        if (prev->next->value == value) {
            Node *victim = prev->next;
            prev->next = victim->next;
            free(victim);
            return true;
        }
        prev = prev->next;
    }
    return false;
}

/* 4b. 二级指针写法：零特判 */
/*
 * 核心思想：Node **cur 指向「一个 Node* 变量」，
 * 它可能是 &head，也可能是 &prev->next。
 * 对 *cur 赋值，既能改 head，也能改某个节点的 next —— 统一处理。
 */
static bool remove_pp(Node **head, int value)
{
    if (head == NULL) { return false; }
    Node **cur = head;
    while (*cur != NULL) {
        Node *entry = *cur;
        if (entry->value == value) {
            *cur = entry->next;        /* 前驱的 next（或 head）直接跳过它 */
            free(entry);
            return true;
        }
        cur = &entry->next;            /* 前进：现在 cur 指向 entry->next */
    }
    return false;
}

/* 删除所有等于 value 的节点（返回删除个数） */
static size_t remove_all(Node **head, int value)
{
    if (head == NULL) { return 0; }
    size_t removed = 0;
    Node **cur = head;
    while (*cur != NULL) {
        Node *entry = *cur;
        if (entry->value == value) {
            *cur = entry->next;
            free(entry);
            removed++;
        } else {
            cur = &entry->next;        /* 注意：只有「没删」时才前进 */
        }
    }
    return removed;
}

/* ---------------------------------------------------------------- */
/* 5. 反转 —— 三指针法                                                 */
/* ---------------------------------------------------------------- */
/*
 * 过程（以 1->2->3->NULL 为例）：
 *
 *   初始:   prev=NULL   cur=1   next=?
 *
 *   第1轮:  next = cur->next (=2)     保存后继
 *           cur->next = prev (=NULL)  掉头
 *           prev = cur (=1)
 *           cur = next (=2)
 *           => NULL <- 1   2 -> 3 -> NULL
 *
 *   第2轮:  next = 3
 *           cur->next = prev (=1)
 *           prev = 2, cur = 3
 *           => NULL <- 1 <- 2   3 -> NULL
 *
 *   第3轮:  next = NULL
 *           cur->next = prev (=2)
 *           prev = 3, cur = NULL
 *           => NULL <- 1 <- 2 <- 3
 *
 *   结束:   *head = prev (=3)
 *
 * 为什么必须有 next 这个临时变量？
 *   因为 cur->next = prev 会【覆盖】掉原来的后继，
 *   不先存下来就再也找不到后面了。
 */
static void reverse(Node **head)
{
    if (head == NULL) { return; }
    Node *prev = NULL;
    Node *cur  = *head;
    while (cur != NULL) {
        Node *next = cur->next;    /* ① 先保存后继 */
        cur->next  = prev;         /* ② 掉头 */
        prev       = cur;          /* ③ prev 前进 */
        cur        = next;         /* ④ cur 前进 */
    }
    *head = prev;                  /* 原来的尾节点成为新头 */
}

/* 递归反转：理解栈帧的好例子，但 n 大时会栈溢出 */
static Node *reverse_rec(Node *head)
{
    if (head == NULL || head->next == NULL) { return head; }
    Node *new_head = reverse_rec(head->next);   /* 先反转后面 */
    head->next->next = head;                    /* 让后继指向自己 */
    head->next = NULL;                          /* 自己变成尾 */
    return new_head;
}

/* ---------------------------------------------------------------- */
/* 辅助函数                                                            */
/* ---------------------------------------------------------------- */
static void print_list(const char *tag, const Node *head)
{
    printf("  %-22s ", tag);
    if (head == NULL) { puts("(空表)"); return; }
    for (const Node *c = head; c != NULL; c = c->next) {
        printf("%d -> ", c->value);
    }
    puts("NULL");
}

static size_t list_len(const Node *head)
{
    size_t n = 0;
    for (const Node *c = head; c != NULL; c = c->next) { n++; }
    return n;
}

/* 打印节点地址：观察 malloc 的分配顺序 */
static void print_addresses(const char *tag, const Node *head)
{
    printf("  %s\n", tag);
    int i = 0;
    for (const Node *c = head; c != NULL; c = c->next) {
        printf("    [%d] value=%-4d node@%p  next=%p\n",
               i++, c->value, (const void *)c, (const void *)c->next);
    }
}

static void free_list(Node **head)
{
    if (head == NULL) { return; }
    Node *cur = *head;
    while (cur != NULL) {
        Node *next = cur->next;        /* 必须先存！free 之后就读不到 cur->next 了 */
        free(cur);
        cur = next;
    }
    *head = NULL;
}

/* 用数组建表（尾插），方便构造测试数据 */
static Node *build_from_array(const int *a, size_t n)
{
    Node *head = NULL;
    for (size_t i = 0; i < n; i++) {
        push_back_pp(&head, a[i]);
    }
    return head;
}

/* ---------------------------------------------------------------- */
int main(void)
{
    puts("================ 单链表实验 ================\n");

    /* ---------- 1. 头插 vs 尾插 ---------- */
    puts("========== 1. 头插 vs 尾插 ==========");
    {
        Node *h1 = NULL;
        for (int i = 1; i <= 4; i++) { push_front(&h1, i); }
        print_list("头插 1,2,3,4:", h1);
        puts("     -> 顺序反过来了，因为每次都插在头部");

        Node *h2 = NULL;
        for (int i = 1; i <= 4; i++) { push_back_naive(&h2, i); }
        print_list("尾插 1,2,3,4:", h2);

        Node *h3 = NULL;
        for (int i = 1; i <= 4; i++) { push_back_pp(&h3, i); }
        print_list("尾插(二级指针):", h3);

        printf("  三种方式长度: %zu %zu %zu\n", list_len(h1), list_len(h2), list_len(h3));

        puts("\n  内存地址（尾插建的表 1->2->3->4）:");
        print_addresses("", h2);
        puts("     -> 地址递增（malloc 顺序分配），但不保证连续");
        puts("     -> 链表的逻辑顺序完全由 next 决定，与地址无关");

        /* 空表也能正确工作 —— 这是二级指针版本的优点 */
        Node *empty = NULL;
        push_back_pp(&empty, 42);
        print_list("空表尾插 42:", empty);
        push_back_naive(&empty, 43);
        print_list("再尾插 43:", empty);

        free_list(&h1); free_list(&h2); free_list(&h3); free_list(&empty);
    }

    /* ---------- 2. 查找 ---------- */
    puts("\n========== 2. 查找 ==========");
    {
        int a[] = {10, 20, 30, 20, 40};
        Node *h = build_from_array(a, 5);
        print_list("链表:", h);
        printf("  find(30) = %p  value=%d\n", (void *)find(h, 30), find(h, 30)->value);
        printf("  find(99) = %p  (NULL 表示没找到)\n", (void *)find(h, 99));

        puts("\n  ⚠️  常见错误：用「找到的指针」当循环条件而不检查 NULL:");
        puts("      while (find(h, x) != NULL) { ... }   // 每次都从头找，O(n^2)");
        free_list(&h);
    }

    /* ---------- 3. 删除：特判 vs 二级指针 ---------- */
    puts("\n========== 3. 删除：特判头节点 vs 二级指针零特判 ==========");
    {
        int a[] = {1, 2, 3, 4, 5};
        Node *h1 = build_from_array(a, 5);
        Node *h2 = build_from_array(a, 5);

        print_list("原始:", h1);
        printf("  remove_naive(&h1, 1) [删头节点] = %d\n", remove_naive(&h1, 1));
        print_list("结果:", h1);
        printf("  remove_naive(&h1, 3) [删中间]   = %d\n", remove_naive(&h1, 3));
        print_list("结果:", h1);

        print_list("\n  同一个表用二级指针版:", h2);
        printf("  remove_pp(&h2, 1) [删头节点] = %d\n", remove_pp(&h2, 1));
        print_list("结果:", h2);
        printf("  remove_pp(&h2, 3) [删中间]   = %d\n", remove_pp(&h2, 3));
        print_list("结果:", h2);

        puts("\n  两种写法结果完全一致，但 remove_pp 少了一个 if 分支。");
        puts("  区别在代码规模变大时才会体现：特判版本有两份几乎相同的删除逻辑，");
        puts("  将来改一处忘一处就是 bug。");

        free_list(&h1); free_list(&h2);
    }

    /* ---------- 4. 删除所有匹配 ---------- */
    puts("\n========== 4. 删除所有等于 x 的节点 ==========");
    {
        int a[] = {1, 2, 1, 3, 1, 4, 1};
        Node *h = build_from_array(a, 7);
        print_list("原始:", h);
        size_t n = remove_all(&h, 1);
        printf("  删除了 %zu 个值为 1 的节点\n", n);
        print_list("结果:", h);

        puts("\n  关键细节：");
        puts("      一级指针写法：删头要特判，删完还要把 prev 接回来");
        puts("      二级指针写法：*cur = entry->next 一句话搞定，");
        puts("                   而且「删了就不前进，没删才前进」天然正确");
        free_list(&h);
    }

    /* ---------- 5. 反转 ---------- */
    puts("\n========== 5. 反转 ==========");
    {
        int a[] = {1, 2, 3, 4, 5};
        Node *h = build_from_array(a, 5);
        print_list("原始:", h);

        puts("  （执行过程见源码注释中的三指针推演）");
        reverse(&h);
        print_list("迭代反转后:", h);

        /* 递归反转返回新头，必须重新赋值。
         * 注意：只调用【一次】，调两次就抵消回原样了。 */
        h = reverse_rec(h);
        print_list("递归反转后:", h);

        puts("  -> 两次反转，回到原样");

        /* 边界：空表和单节点 */
        Node *e = NULL;
        reverse(&e);
        print_list("反转空表:", e);

        int one_val = 7;
        Node *one = build_from_array(&one_val, 1);
        reverse(&one);
        print_list("反转单节点:", one);
        free_list(&one);

        free_list(&h);
    }

    /* ---------- 6. 性能对比：链表 vs 数组 ---------- */
    puts("\n========== 6. ⚠️ 链表不一定比数组快（cache 的故事）==========");
    {
        const size_t N = 2000000;
        int *arr = malloc(N * sizeof *arr);
        if (arr == NULL) { return 1; }
        for (size_t i = 0; i < N; i++) { arr[i] = (int)i; }

        /* 建一个同样内容的链表 */
        Node *head = NULL;
        Node **tail = &head;
        for (size_t i = 0; i < N; i++) {
            Node *n = malloc(sizeof *n);
            if (n == NULL) { break; }
            n->value = (int)i;
            n->next = NULL;
            *tail = n;
            tail = &n->next;
        }

        puts("  实验 A：顺序遍历求和（5 轮）");
        {
            volatile long long sum = 0;
            clock_t t0 = clock();
            for (size_t rep = 0; rep < 5; rep++) {
                long long s = 0;
                for (size_t i = 0; i < N; i++) { s += arr[i]; }
                sum += s;
            }
            double ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
            printf("    数组（连续内存）: %8.3f ms  (校验和 %lld)\n", ms, sum);
        }
        {
            volatile long long sum = 0;
            clock_t t0 = clock();
            for (size_t rep = 0; rep < 5; rep++) {
                long long s = 0;
                for (Node *c = head; c != NULL; c = c->next) { s += c->value; }
                sum += s;
            }
            double ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
            printf("    链表（分散内存）: %8.3f ms  (校验和 %lld)\n", ms, sum);
        }
        puts("    -> 两者差不多！因为两种都是【顺序访问】，");
        puts("       硬件预取器（prefetcher）能提前把下一块内存拉进 cache。");

        puts("\n  实验 B：按下标访问（数组 O(1) vs 链表 O(n)）");
        puts("          两边用【同样的伪随机下标序列】，只做 2000 次访问");
        {
            /* 为什么只做 2000 次？链表按下标访问是 O(n)，
             * 总代价 O(n * 访问次数)。200 万 * 2000 = 4e9 次指针跳转，
             * 已经要跑一两秒。再大就要跑几分钟了。 */
            const size_t Q = 2000;
            unsigned x = 12345u;
            volatile long long sum = 0;
            clock_t t0 = clock();
            {
                long long s = 0;
                x = 12345u;
                for (size_t i = 0; i < Q; i++) {
                    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
                    s += arr[x % N];
                }
                sum += s;
            }
            double ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
            printf("    数组按下标访问 (%zu 次): %8.3f ms  (校验和 %lld)\n", Q, ms, sum);
        }
        {
            const size_t Q = 2000;
            unsigned x;
            volatile long long sum = 0;
            clock_t t0 = clock();
            {
                long long s = 0;
                x = 12345u;
                for (size_t i = 0; i < Q; i++) {
                    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
                    size_t k = x % N;
                    Node *c = head;
                    for (size_t j = 0; j < k && c != NULL; j++) { c = c->next; }
                    if (c != NULL) { s += c->value; }
                }
                sum += s;
            }
            double ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
            printf("    链表按下标访问 (%zu 次): %8.3f ms  (校验和 %lld)\n", Q, ms, sum);
        }
        puts("    -> 数组快几个数量级：链表按下标访问是 O(n)，平均要走 100 万步。");
        puts("    -> 结论：链表适合「已经持有节点指针」时做插入/删除；");
        puts("       需要随机访问或顺序扫描时，数组几乎总是更好。");

        free(arr);
        free_list(&head);
    }

    puts("\n========== 7. 小结 ==========");
    puts("  1. 头插 O(1)，尾插 O(n)（除非维护 tail 指针）");
    puts("  2. C 只有值传递：要修改 head 本身，必须传 Node**");
    puts("  3. 二级指针遍历删除可以消除头节点特判，是 C 的经典技巧");
    puts("  4. 反转必须用三个指针：prev / cur / next");
    puts("  5. free(cur) 之前必须先保存 cur->next");
    puts("  6. 链表的主要劣势不是复杂度，而是 cache 局部性差");

    return 0;
}
