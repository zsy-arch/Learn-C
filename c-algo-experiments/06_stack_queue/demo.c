/* demo.c —— 栈与队列：数组实现 vs 链表实现
 *
 * 本实验要回答的问题：
 *   1. 栈用数组做还是链表做？队列呢？
 *   2. 用数组做队列时，为什么叫「循环队列」？不循环会怎样？
 *   3. 栈的经典应用：括号匹配、表达式求值 —— 到底怎么用？
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

/* ================================================================ */
/* 第一部分：栈                                                       */
/* ================================================================ */

/* ---------------------------------------------------------------- */
/* 1a. 数组栈 —— 固定容量，O(1)，cache 友好                             */
/* ---------------------------------------------------------------- */
#define STACK_CAP 64

typedef struct {
    int    data[STACK_CAP];
    size_t top;              /* 元素个数，也是下一个空位的下标 */
} ArrayStack;

static void as_init(ArrayStack *s) { s->top = 0; }
static bool as_empty(const ArrayStack *s) { return s->top == 0; }
static bool as_full(const ArrayStack *s) { return s->top >= STACK_CAP; }
static size_t as_size(const ArrayStack *s) { return s->top; }

static bool as_push(ArrayStack *s, int v)
{
    if (as_full(s)) { return false; }        /* 栈溢出 */
    s->data[s->top++] = v;
    return true;
}

static bool as_pop(ArrayStack *s, int *out)
{
    if (as_empty(s)) { return false; }       /* 栈下溢 */
    s->top--;
    if (out != NULL) { *out = s->data[s->top]; }
    return true;
}

static bool as_peek(const ArrayStack *s, int *out)
{
    if (as_empty(s)) { return false; }
    *out = s->data[s->top - 1];
    return true;
}

/* ---------------------------------------------------------------- */
/* 1b. 链表栈 —— 容量无限（受内存限制），头插头删都是 O(1)               */
/* ---------------------------------------------------------------- */
typedef struct SNode {
    int           value;
    struct SNode *next;
} SNode;

typedef struct {
    SNode *top;
    size_t size;
} ListStack;

static void ls_init(ListStack *s) { s->top = NULL; s->size = 0; }

static bool ls_push(ListStack *s, int v)
{
    SNode *n = malloc(sizeof *n);
    if (n == NULL) { return false; }
    n->value = v;
    n->next  = s->top;      /* 新节点成为新的栈顶 */
    s->top   = n;
    s->size++;
    return true;
}

static bool ls_pop(ListStack *s, int *out)
{
    if (s->top == NULL) { return false; }
    SNode *victim = s->top;
    if (out != NULL) { *out = victim->value; }
    s->top = victim->next;
    free(victim);
    s->size--;
    return true;
}

static void ls_free(ListStack *s)
{
    int dummy;
    while (ls_pop(s, &dummy)) { /* 逐个弹出即释放 */ }
}

/* ================================================================ */
/* 第二部分：队列                                                     */
/* ================================================================ */

/* ---------------------------------------------------------------- */
/* 2a. 循环队列 —— 数组实现                                            */
/* ---------------------------------------------------------------- */
/*
 * 为什么必须「循环」？
 *   如果用朴素数组：head 永远指向 0，出队就把后面所有元素前移 —— O(n)。
 *   或者 head/tail 都只增不减：tail 很快撞到数组末尾，前面的空间浪费。
 *
 * 循环队列：让 head 和 tail 到达末尾后回绕到 0（取模）。
 *
 * 怎么区分「空」和「满」？
 *   如果 head == tail 既可能表示空，也可能表示满。
 *   两种常见解法：
 *     (a) 牺牲一个槽位：空 = (head==tail)，满 = ((tail+1)%cap == head)
 *     (b) 额外维护 count 字段（本实现用这个，更直观）
 */
#define QUEUE_CAP 8

typedef struct {
    int    data[QUEUE_CAP];
    size_t head;      /* 出队位置 */
    size_t tail;      /* 入队位置 */
    size_t count;     /* 当前元素个数 */
} CircleQueue;

static void cq_init(CircleQueue *q) { q->head = q->tail = q->count = 0; }
static bool cq_empty(const CircleQueue *q) { return q->count == 0; }
static bool cq_full(const CircleQueue *q) { return q->count >= QUEUE_CAP; }

static bool cq_enqueue(CircleQueue *q, int v)
{
    if (cq_full(q)) { return false; }
    q->data[q->tail] = v;
    q->tail = (q->tail + 1) % QUEUE_CAP;      /* 关键：取模实现循环 */
    q->count++;
    return true;
}

static bool cq_dequeue(CircleQueue *q, int *out)
{
    if (cq_empty(q)) { return false; }
    if (out != NULL) { *out = q->data[q->head]; }
    q->head = (q->head + 1) % QUEUE_CAP;
    q->count--;
    return true;
}

/* 打印队列内部状态，观察 head/tail 如何移动和回绕 */
static void cq_dump(const CircleQueue *q, const char *tag)
{
    printf("  %-18s head=%zu tail=%zu count=%zu  内容: ",
           tag, q->head, q->tail, q->count);
    if (cq_empty(q)) { puts("(空)"); return; }
    size_t i = q->head;
    for (size_t k = 0; k < q->count; k++) {
        printf("%d ", q->data[i]);
        i = (i + 1) % QUEUE_CAP;
    }
    puts("");
}

/* ---------------------------------------------------------------- */
/* 2b. 链表队列 —— 带尾指针                                           */
/* ---------------------------------------------------------------- */
/*
 * 关键：维护 head（出队）和 tail（入队）两个指针，
 * 这样入队和出队都是 O(1)。
 * 如果只有 head，入队要遍历到尾 —— O(n)。
 */
typedef struct QNode {
    int           value;
    struct QNode *next;
} QNode;

typedef struct {
    QNode *head;
    QNode *tail;
    size_t size;
} ListQueue;

static void lq_init(ListQueue *q) { q->head = q->tail = NULL; q->size = 0; }

static bool lq_enqueue(ListQueue *q, int v)
{
    QNode *n = malloc(sizeof *n);
    if (n == NULL) { return false; }
    n->value = v;
    n->next  = NULL;
    if (q->tail != NULL) {
        q->tail->next = n;
    } else {
        q->head = n;             /* 空队列：头也要指向它 */
    }
    q->tail = n;
    q->size++;
    return true;
}

static bool lq_dequeue(ListQueue *q, int *out)
{
    if (q->head == NULL) { return false; }
    QNode *victim = q->head;
    if (out != NULL) { *out = victim->value; }
    q->head = victim->next;
    if (q->head == NULL) { q->tail = NULL; }    /* 最后一个元素被拿走 */
    free(victim);
    q->size--;
    return true;
}

static void lq_free(ListQueue *q)
{
    int dummy;
    while (lq_dequeue(q, &dummy)) { /* 逐个出队即释放 */ }
}

/* ================================================================ */
/* 第三部分：栈的经典应用                                              */
/* ================================================================ */

/* ---------------------------------------------------------------- */
/* 3a. 括号匹配                                                       */
/* ---------------------------------------------------------------- */
/*
 * 思路：遇到左括号入栈；遇到右括号，检查栈顶是否为配对的左括号。
 *       全部处理完后，栈必须为空。
 *
 * 为什么用栈？因为括号是「最近未匹配的优先」——嵌套结构天然适合栈。
 */
static bool bracket_match(const char *s, size_t *err_pos)
{
    ArrayStack st;
    as_init(&st);

    for (size_t i = 0; s[i] != '\0'; i++) {
        char c = s[i];
        if (c == '(' || c == '[' || c == '{') {
            if (!as_push(&st, (int)c)) { return false; }
        } else if (c == ')' || c == ']' || c == '}') {
            int top;
            if (!as_pop(&st, &top)) {
                *err_pos = i;
                return false;            /* 右括号多了 */
            }
            char expect = (c == ')') ? '(' : (c == ']') ? '[' : '{';
            if ((char)top != expect) {
                *err_pos = i;
                return false;            /* 括号类型不匹配 */
            }
        }
    }
    if (!as_empty(&st)) {
        *err_pos = strlen(s);
        return false;                    /* 左括号多了 */
    }
    *err_pos = 0;
    return true;
}

/* ---------------------------------------------------------------- */
/* 3b. 后缀表达式（逆波兰式）求值                                       */
/* ---------------------------------------------------------------- */
/*
 * 中缀表达式 3 + 4 * 2 需要处理优先级。
 * 后缀表达式 3 4 2 * + 不需要 —— 从左到右扫描，遇数字入栈，遇运算符弹出两个数计算。
 *
 * 这是「栈消除递归/优先级」的经典例子。
 */
static bool eval_rpn(const char *expr, long *result)
{
    ArrayStack st;
    as_init(&st);

    for (size_t i = 0; expr[i] != '\0'; i++) {
        char c = expr[i];
        if (isspace((unsigned char)c)) { continue; }

        if (isdigit((unsigned char)c)) {
            if (!as_push(&st, c - '0')) { return false; }
        } else if (c == '+' || c == '-' || c == '*' || c == '/') {
            int b, a;
            if (!as_pop(&st, &b)) { return false; }   /* 注意：先弹出的是右操作数 */
            if (!as_pop(&st, &a)) { return false; }
            long r = 0;
            switch (c) {
            case '+': r = (long)a + b; break;
            case '-': r = (long)a - b; break;
            case '*': r = (long)a * b; break;
            case '/': if (b == 0) { return false; } r = (long)a / b; break;
            default: break;
            }
            if (!as_push(&st, (int)r)) { return false; }
        } else {
            return false;                    /* 非法字符 */
        }
    }
    if (st.top != 1) { return false; }        /* 最终栈里必须只剩一个结果 */
    int r;
    as_pop(&st, &r);
    *result = r;
    return true;
}

/* ---------------------------------------------------------------- */
/* 3c. 用两个栈实现队列                                                */
/* ---------------------------------------------------------------- */
/*
 * in 栈负责入队，out 栈负责出队。
 * 出队时如果 out 为空，就把 in 全部倒进 out —— 顺序就正过来了。
 * 摊还复杂度：每个元素最多被搬一次，所以 enqueue O(1)，dequeue 摊还 O(1)。
 */
typedef struct {
    ArrayStack in;
    ArrayStack out;
} QueueViaStacks;

static void qvs_init(QueueViaStacks *q) { as_init(&q->in); as_init(&q->out); }

static bool qvs_enqueue(QueueViaStacks *q, int v) { return as_push(&q->in, v); }

static bool qvs_dequeue(QueueViaStacks *q, int *out)
{
    if (as_empty(&q->out)) {
        int tmp;
        while (as_pop(&q->in, &tmp)) {        /* 把 in 全部倒入 out */
            if (!as_push(&q->out, tmp)) { return false; }
        }
    }
    return as_pop(&q->out, out);
}

/* ================================================================ */
int main(void)
{
    puts("================ 栈与队列实验 ================\n");

    /* ---------- 1. 两种栈 ---------- */
    puts("========== 1. 数组栈 vs 链表栈 ==========");
    {
        ArrayStack a;
        as_init(&a);
        for (int i = 1; i <= 5; i++) { as_push(&a, i * 10); }
        printf("  数组栈入栈 10,20,30,40,50，size=%zu\n", as_size(&a));

        int v;
        as_peek(&a, &v);
        printf("  栈顶 peek = %d （不出栈）\n", v);

        printf("  依次出栈: ");
        while (as_pop(&a, &v)) { printf("%d ", v); }
        puts("  <- 后进先出 (LIFO)");

        printf("  空栈再 pop 返回 %s（下溢保护）\n",
               as_pop(&a, &v) ? "true" : "false");

        /* 栈溢出 */
        ArrayStack b;
        as_init(&b);
        int pushed = 0;
        while (as_push(&b, pushed)) { pushed++; }
        printf("  数组栈容量 %d，塞满后第 %d 个元素 push 失败（上溢保护）\n",
               STACK_CAP, pushed + 1);

        /* 链表栈无容量限制 */
        ListStack l;
        ls_init(&l);
        for (int i = 0; i < 1000; i++) { ls_push(&l, i); }
        printf("  链表栈成功压入 1000 个元素，size=%zu\n", l.size);
        printf("  依次弹出前 5 个: ");
        for (int i = 0; i < 5; i++) { ls_pop(&l, &v); printf("%d ", v); }
        puts("(后面还有 995 个)");
        ls_free(&l);
        printf("  释放后 size=%zu\n", l.size);
    }

    /* ---------- 2. 循环队列 ---------- */
    puts("\n========== 2. 循环队列：观察 head/tail 回绕 ==========");
    {
        CircleQueue q;
        cq_init(&q);
        printf("  容量 = %d\n", QUEUE_CAP);

        for (int i = 1; i <= 6; i++) { cq_enqueue(&q, i); }
        cq_dump(&q, "入队 1..6 后:");

        int v;
        for (int i = 0; i < 3; i++) {
            cq_dequeue(&q, &v);
            printf("    出队 -> %d\n", v);
        }
        cq_dump(&q, "出队 3 个后:");

        for (int i = 7; i <= 10; i++) { cq_enqueue(&q, i); }
        cq_dump(&q, "再入队 7..10 后:");

        puts("    ^ 注意 tail 已经回绕到 head 前面了 —— 这就是「循环」队列");
        puts("      如果不用取模，tail 早就超过数组末尾了。");

        puts("\n  塞满队列看看:");
        int extra = 100;
        while (cq_enqueue(&q, extra)) { extra++; }
        cq_dump(&q, "塞满后:");
        printf("    队列里有 %zu 个元素（容量 %d），第 %d 个入队失败（上溢保护）\n",
               q.count, QUEUE_CAP, extra);
    }

    /* ---------- 3. 链表队列 ---------- */
    puts("\n========== 3. 链表队列（带 tail 指针）==========");
    {
        ListQueue q;
        lq_init(&q);
        for (int i = 1; i <= 5; i++) { lq_enqueue(&q, i); }
        printf("  入队 1..5，size=%zu\n", q.size);

        int v;
        printf("  依次出队: ");
        while (lq_dequeue(&q, &v)) { printf("%d ", v); }
        puts("  <- 先进先出 (FIFO)");

        /* 边界：清空后再入队 */
        lq_enqueue(&q, 99);
        printf("  清空后再入队 99，head 和 tail 都指向同一个节点: %s\n",
               (q.head == q.tail) ? "是" : "否");
        lq_dequeue(&q, &v);
        printf("  再出队: %d，此时 head=%s\n", v, (q.head == NULL) ? "NULL" : "非空");
        lq_free(&q);
    }

    /* ---------- 4. 括号匹配 ---------- */
    puts("\n========== 4. 栈的应用：括号匹配 ==========");
    {
        const char *tests[] = {
            "(1 + 2) * 3",
            "{[()]}",
            "([)]",                 /* 类型不匹配 */
            "((()))",               /* 嵌套 */
            "(1 + 2",               /* 左括号多了 */
            "1 + 2)",               /* 右括号多了 */
            "a[b{c(d)e}f]g",
            "[(])",                 /* 交叉不匹配 */
        };
        for (size_t i = 0; i < sizeof tests / sizeof tests[0]; i++) {
            size_t err = 0;
            bool ok = bracket_match(tests[i], &err);
            printf("  %-16s -> %-6s", tests[i], ok ? "匹配" : "不匹配");
            if (!ok) {
                printf(" (问题位置: %zu", err);
                if (err < strlen(tests[i])) {
                    printf(" 字符 '%c'", tests[i][err]);
                }
                printf(")");
            }
            putchar('\n');
        }
    }

    /* ---------- 5. 后缀表达式求值 ---------- */
    puts("\n========== 5. 栈的应用：后缀表达式求值 ==========");
    {
        struct { const char *expr; long expect; } cases[] = {
            {"3 4 2 * +",   11},        /* 3 + (4*2) = 11 */
            {"5 1 2 + 4 * + 3 -", 14},  /* 5 + ((1+2)*4) - 3 = 14 */
            {"7 2 /",        3},        /* 整数除法 */
            {"2 3 4 * +",   14},        /* 2 + (3*4) = 14 */
        };
        for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
            long r = 0;
            bool ok = eval_rpn(cases[i].expr, &r);
            printf("  \"%-20s\" -> %-3ld", cases[i].expr, r);
            if (ok && r == cases[i].expect) {
                printf("  ✓ 期望 %ld\n", cases[i].expect);
            } else {
                printf("  ✗ 期望 %ld (ok=%d)\n", cases[i].expect, ok);
            }
        }
        puts("  思路：遇数字入栈，遇运算符弹出两个数计算再入栈。");
        puts("        注意先弹出的【右操作数】，后弹出的才是左操作数 —— 顺序反了就错。");
    }

    /* ---------- 6. 两个栈实现队列 ---------- */
    puts("\n========== 6. 两个栈实现队列 ==========");
    {
        QueueViaStacks q;
        qvs_init(&q);
        for (int i = 1; i <= 5; i++) { qvs_enqueue(&q, i); }
        printf("  入队 1..5，然后...\n");
        int v;
        printf("  出队两个: ");
        for (int i = 0; i < 2; i++) { qvs_dequeue(&q, &v); printf("%d ", v); }
        putchar('\n');

        printf("  再入队 6,7\n");
        qvs_enqueue(&q, 6);
        qvs_enqueue(&q, 7);

        printf("  剩下全部出队: ");
        while (qvs_dequeue(&q, &v)) { printf("%d ", v); }
        puts("  <- 依然是 FIFO 顺序");
        puts("  摊还复杂度：每个元素最多被搬一次，所以 enqueue O(1)，dequeue 摊还 O(1)");
    }

    puts("\n========== 7. 复杂度对比 ==========");
    puts("  ┌──────────┬──────────┬──────────┬──────────┬──────────────┐");
    puts("  │ 实现     │ push/enq │  pop/deq │  peek    │ 空间         │");
    puts("  ├──────────┼──────────┼──────────┼──────────┼──────────────┤");
    puts("  │ 数组栈   │  O(1)    │  O(1)    │  O(1)    │ 固定，可能溢出 │");
    puts("  │ 链表栈   │  O(1)    │  O(1)    │  O(1)    │ 动态，指针开销 │");
    puts("  │ 循环队列 │  O(1)    │  O(1)    │  O(1)    │ 固定，可能满   │");
    puts("  │ 链表队列 │  O(1)    │  O(1)    │  O(1)    │ 动态，需 tail │");
    puts("  └──────────┴──────────┴──────────┴──────────┴──────────────┘");
    puts("  注：链表队列必须维护 tail 指针，否则入队退化成 O(n)。");
    puts("");
    puts("  怎么选？");
    puts("    - 容量可预估、追求速度  -> 数组实现（cache 友好，无 malloc）");
    puts("    - 容量不可预知、怕溢出  -> 链表实现（但要处理 malloc 失败）");
    puts("    - 队列一律用循环数组，否则出队后前面的空间就浪费了");

    return 0;
}
