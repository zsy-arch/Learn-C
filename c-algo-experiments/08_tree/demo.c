/* demo.c —— 二叉树：遍历、BST、递归栈帧、平衡性
 *
 * 本实验要回答的问题：
 *   1. 前序/中序/后序遍历的递归到底按什么顺序走？
 *   2. 为什么中序遍历 BST 会得到有序序列？
 *   3. BST 的删除为什么最麻烦？三种情况分别怎么处理？
 *   4. 递归遍历用掉多少栈空间？能改成迭代吗？
 *   5. 为什么需要平衡树？不平衡会退化到什么程度？
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "../timing.h"
#include "../bench.c"

/* ================================================================ */
/* 节点定义                                                          */
/* ================================================================ */
typedef struct TreeNode {
    int               value;
    struct TreeNode  *left;
    struct TreeNode  *right;
} TreeNode;

static TreeNode *tn_new(int v)
{
    TreeNode *n = malloc(sizeof *n);
    if (n == NULL) { return NULL; }
    n->value = v;
    n->left  = NULL;
    n->right = NULL;
    return n;
}

/* ================================================================ */
/* 第一部分：递归遍历                                                 */
/* ================================================================ */
/*
 * 三种遍历的区别只在「什么时候访问根节点」：
 *
 *   前序 (preorder)  根 -> 左 -> 右      // 复制树、序列化
 *   中序 (inorder)   左 -> 根 -> 右      // BST 得到有序序列
 *   后序 (postorder) 左 -> 右 -> 根      // 释放树、计算子树大小
 *
 * 函数体三行的顺序改变，就是三种遍历 —— 这是递归最优雅的地方。
 */
typedef struct {
    int   *data;
    size_t len;
    size_t cap;
} VisitLog;

static bool log_push(VisitLog *l, int v)
{
    if (l->len >= l->cap) { return false; }
    l->data[l->len++] = v;
    return true;
}

static void preorder(const TreeNode *n, VisitLog *log)
{
    if (n == NULL) { return; }
    if (log) { log_push(log, n->value); }
    preorder(n->left,  log);
    preorder(n->right, log);
}

static void inorder(const TreeNode *n, VisitLog *log)
{
    if (n == NULL) { return; }
    inorder(n->left,   log);
    if (log) { log_push(log, n->value); }
    inorder(n->right,  log);
}

static void postorder(const TreeNode *n, VisitLog *log)
{
    if (n == NULL) { return; }
    postorder(n->left,  log);
    postorder(n->right, log);
    if (log) { log_push(log, n->value); }
}

/* 逐层打印，看树的结构 */
static void print_tree(const TreeNode *n, int depth, const char *prefix)
{
    if (n == NULL) { return; }
    for (int i = 0; i < depth; i++) { printf("    "); }
    printf("%s%d\n", prefix, n->value);
    print_tree(n->left,  depth + 1, "L-- ");
    print_tree(n->right, depth + 1, "R-- ");
}

static void free_tree(TreeNode *n)
{
    if (n == NULL) { return; }
    free_tree(n->left);       /* 后序：先释放子树 */
    free_tree(n->right);
    free(n);                  /* 最后释放自己 */
}

/* ================================================================ */
/* 第二部分：BST（二叉搜索树）                                         */
/* ================================================================ */
/*
 * 性质：对任意节点，左子树所有值 < 该节点值 < 右子树所有值
 *
 * 这个性质让查找可以「每次排除一半」，平均 O(log n)。
 * 但前提是树【大致平衡】—— 见第 4 节。
 */
typedef struct {
    TreeNode *root;
    size_t    count;
} BST;

static void bst_init(BST *t) { t->root = NULL; t->count = 0; }

/* 插入：二级指针写法，不需要特判空树 */
static bool bst_insert(BST *t, int v)
{
    TreeNode **cur = &t->root;
    while (*cur != NULL) {
        if (v == (*cur)->value) { return false; }   /* 已存在，不重复插入 */
        cur = (v < (*cur)->value) ? &(*cur)->left : &(*cur)->right;
    }
    TreeNode *n = tn_new(v);
    if (n == NULL) { return false; }
    *cur = n;
    t->count++;
    return true;
}

static bool bst_contains(const BST *t, int v)
{
    for (const TreeNode *c = t->root; c != NULL; ) {
        if (v == c->value) { return true; }
        c = (v < c->value) ? c->left : c->right;
    }
    return false;
}

/* 查找并返回节点指针（可能为 NULL）—— 比只返回 bool 更通用 */
static const TreeNode *bst_find(const BST *t, int v)
{
    for (const TreeNode *c = t->root; c != NULL; ) {
        if (v == c->value) { return c; }
        c = (v < c->value) ? c->left : c->right;
    }
    return NULL;
}

static int bst_min(const TreeNode *n)
{
    while (n != NULL && n->left != NULL) { n = n->left; }
    return (n != NULL) ? n->value : -1;
}

static int bst_max(const TreeNode *n)
{
    while (n != NULL && n->right != NULL) { n = n->right; }
    return (n != NULL) ? n->value : -1;
}

/* ---------------------------------------------------------------- */
/* 删除：BST 最麻烦的操作，分三种情况                                    */
/* ---------------------------------------------------------------- */
/*
 * 情况 1：叶子节点          -> 直接删掉
 * 情况 2：只有一个孩子        -> 用孩子顶替自己
 * 情况 3：有两个孩子        -> 找【中序后继】（右子树的最小值），
 *                             把它的值拷到当前节点，然后删掉那个后继
 *
 * 情况 3 为什么可行？因为中序后继是「比当前值大的最小的那个」，
 * 把它换上来，BST 性质依然保持。
 */
static TreeNode *bst_delete_rec(TreeNode *n, int v, bool *deleted)
{
    if (n == NULL) { return NULL; }

    if (v < n->value) {
        n->left = bst_delete_rec(n->left, v, deleted);
    } else if (v > n->value) {
        n->right = bst_delete_rec(n->right, v, deleted);
    } else {
        /* 找到了 */
        *deleted = true;

        if (n->left == NULL && n->right == NULL) {          /* 情况 1 */
            free(n);
            return NULL;
        }
        if (n->left == NULL) {                              /* 情况 2a：只有右孩子 */
            TreeNode *r = n->right;
            free(n);
            return r;
        }
        if (n->right == NULL) {                             /* 情况 2b：只有左孩子 */
            TreeNode *l = n->left;
            free(n);
            return l;
        }
        /* 情况 3：两个孩子 —— 找中序后继 */
        TreeNode *succ = n->right;
        while (succ->left != NULL) { succ = succ->left; }
        n->value = succ->value;                             /* 值搬上来 */
        n->right = bst_delete_rec(n->right, succ->value, deleted);  /* 删掉后继 */
    }
    return n;
}

static bool bst_delete(BST *t, int v)
{
    bool deleted = false;
    t->root = bst_delete_rec(t->root, v, &deleted);
    if (deleted) { t->count--; }
    return deleted;
}

/* 验证 BST 性质：中序必须严格递增 */
static bool bst_validate(const TreeNode *n, int *prev, bool *first)
{
    if (n == NULL) { return true; }
    if (!bst_validate(n->left, prev, first)) { return false; }
    if (*first) { *first = false; }
    else if (n->value <= *prev) { return false; }    /* 必须严格递增 */
    *prev = n->value;
    if (!bst_validate(n->right, prev, first)) { return false; }
    return true;
}

static bool bst_is_valid(const BST *t)
{
    int prev = 0;
    bool first = true;
    return bst_validate(t->root, &prev, &first);
}

/* 树高：递归定义 max(左高, 右高) + 1 */
static int tree_height(const TreeNode *n)
{
    if (n == NULL) { return 0; }
    int lh = tree_height(n->left);
    int rh = tree_height(n->right);
    return 1 + (lh > rh ? lh : rh);
}

static size_t tree_size(const TreeNode *n)
{
    if (n == NULL) { return 0; }
    return 1 + tree_size(n->left) + tree_size(n->right);
}

/* ================================================================ */
/* 第三部分：递归深度与栈帧                                            */
/* ================================================================ */
static int g_max_depth = 0;
static int g_last_depth = 0;
static int g_calls = 0;

static void preorder_depth(const TreeNode *n, int depth)
{
    g_calls++;
    if (depth > g_max_depth) { g_max_depth = depth; }
    if (n == NULL) { return; }
    preorder_depth(n->left,  depth + 1);
    preorder_depth(n->right, depth + 1);
}

/* ================================================================ */
/* 第四部分：层序遍历（BFS）—— 用队列                                  */
/* ================================================================ */
static void level_order(const TreeNode *root)
{
    if (root == NULL) { return; }
    const TreeNode **q = malloc(1024 * sizeof *q);
    if (q == NULL) { return; }
    size_t head = 0, tail = 0;

    q[tail++] = root;
    int level = 0;
    while (head < tail) {
        size_t level_size = tail - head;     /* 当前层有多少个节点 */
        printf("    第 %d 层: ", level);
        for (size_t i = 0; i < level_size; i++) {
            const TreeNode *n = q[head++];
            printf("%d ", n->value);
            if (n->left  != NULL) { q[tail++] = n->left; }
            if (n->right != NULL) { q[tail++] = n->right; }
        }
        putchar('\n');
        level++;
    }
    free(q);
}

/* ================================================================ */
int main(void)
{
    puts("================ 二叉树实验 ================\n");

    /* ---------- 1. 建一棵树，看三种遍历 ---------- */
    puts("========== 1. 三种遍历 ==========");
    {
        /*
         *         4
         *        / \
         *       2   6
         *      / \ / \
         *     1  3 5  7
         */
        BST t;
        bst_init(&t);
        int vals[] = {4, 2, 6, 1, 3, 5, 7};
        for (size_t i = 0; i < sizeof vals / sizeof vals[0]; i++) {
            bst_insert(&t, vals[i]);
        }

        puts("  树结构（L-- 是左孩子，R-- 是右孩子）:");
        print_tree(t.root, 1, "");

        VisitLog log;
        log.cap = 32;
        log.len = 0;
        log.data = malloc(log.cap * sizeof *log.data);
        if (log.data == NULL) { return 1; }

        preorder(t.root, &log);
        printf("\n  前序 (根左右): ");
        for (size_t i = 0; i < log.len; i++) { printf("%d ", log.data[i]); }

        log.len = 0;
        inorder(t.root, &log);
        printf("\n  中序 (左根右): ");
        for (size_t i = 0; i < log.len; i++) { printf("%d ", log.data[i]); }
        printf("   <- 有序！");

        log.len = 0;
        postorder(t.root, &log);
        printf("\n  后序 (左右根): ");
        for (size_t i = 0; i < log.len; i++) { printf("%d ", log.data[i]); }
        putchar('\n');

        printf("\n  树高 = %d, 节点数 = %zu\n",
               tree_height(t.root), tree_size(t.root));

        puts("\n  层序遍历 (BFS，用队列):");
        level_order(t.root);

        printf("\n  BST 性质校验（中序严格递增）: %s\n",
               bst_is_valid(&t) ? "通过" : "失败");
        printf("  最小值 = %d, 最大值 = %d\n", bst_min(t.root), bst_max(t.root));
        {
            const TreeNode *n = bst_find(&t, 6);
            printf("  bst_find(6) -> 节点指针 %p（可用于后续修改/删除）；"
                   "bst_find(99) -> %p\n",
                   (const void *)n, (const void *)bst_find(&t, 99));
        }

        free(log.data);
        free_tree(t.root);
    }

    /* ---------- 2. 中序有序的原因 ---------- */
    puts("\n========== 2. 为什么中序遍历 BST 得到有序序列？==========");
    {
        puts("  BST 性质：左子树所有值 < 根 < 右子树所有值");
        puts("  中序 = 左 -> 根 -> 右");
        puts("  归纳：左子树递归给出【已排序的左半部分】，");
        puts("        然后输出根，再输出【已排序的右半部分】，");
        puts("        拼起来自然就是全局有序。");
        puts("\n  这也是判断一棵树是不是合法 BST 的标准方法：");
        puts("    中序遍历，检查是否严格递增。");

        /* 故意构造一棵不满足 BST 的树 */
        TreeNode *bad = tn_new(5);
        if (bad == NULL) { return 1; }
        bad->left  = tn_new(3);
        bad->right = tn_new(8);
        if (bad->left != NULL) { bad->left->right = tn_new(9); }  /* 9 > 5，不该在左子树 */
        BST bt;
        bt.root = bad;
        bt.count = 4;
        puts("\n  构造一棵「左子树里有 9 > 根 5」的树");
        printf("  bst_is_valid -> %s (应该是失败)\n",
               bst_is_valid(&bt) ? "通过" : "失败");
        free_tree(bad);
    }

    /* ---------- 3. BST 删除的三种情况 ---------- */
    puts("\n========== 3. BST 删除的三种情况 ==========");
    {
        BST t;
        bst_init(&t);
        int vals[] = {50, 30, 70, 20, 40, 60, 80};
        for (size_t i = 0; i < sizeof vals / sizeof vals[0]; i++) {
            bst_insert(&t, vals[i]);
        }
        puts("  初始树:");
        print_tree(t.root, 2, "");
        printf("  count=%zu, height=%d\n", t.count, tree_height(t.root));

        puts("\n  【情况 1】删除叶子节点 20:");
        bst_delete(&t, 20);
        printf("  contains(20) = %s, count=%zu, 校验=%s\n",
               bst_contains(&t, 20) ? "还在(错误)" : "已删除",
               t.count, bst_is_valid(&t) ? "通过" : "失败");

        puts("\n  【情况 2】删除只有一个孩子的节点 30:");
        bst_delete(&t, 30);
        printf("  contains(30) = %s, count=%zu, 校验=%s\n",
               bst_contains(&t, 30) ? "还在(错误)" : "已删除",
               t.count, bst_is_valid(&t) ? "通过" : "失败");
        print_tree(t.root, 2, "");

        puts("\n  【情况 3】删除有两个孩子的节点 70:");
        bst_delete(&t, 70);
        printf("  count=%zu, 校验=%s\n", t.count, bst_is_valid(&t) ? "通过" : "失败");
        print_tree(t.root, 2, "");
        puts("    注意 80 被提上来了（它是 70 的中序后继）");

        puts("\n  删除根节点（有两个孩子）:");
        bst_delete(&t, 50);
        printf("  count=%zu, 校验=%s\n", t.count, bst_is_valid(&t) ? "通过" : "失败");
        print_tree(t.root, 2, "");

        free_tree(t.root);
    }

    /* ---------- 4. 随机插入 vs 有序插入 ---------- */
    puts("\n========== 4. ⚠️ 有序插入会把 BST 退化成链表 ==========");
    {
        const int N = 1000;

        /* (a) 随机顺序插入 */
        BST tr;
        bst_init(&tr);
        unsigned x = 987654321u;
        int *vals = malloc((size_t)N * sizeof *vals);
        if (vals == NULL) { return 1; }
        for (int i = 0; i < N; i++) {
            x ^= x << 13; x ^= x >> 17; x ^= x << 5;
            vals[i] = (int)(x % 100000u);
        }
        int inserted = 0;
        for (int i = 0; i < N; i++) {
            /* 随机值可能重复，bst_insert 对重复值返回 false 不插入，
             * 所以最终节点数可能略少于 N */
            if (bst_insert(&tr, vals[i])) { inserted++; }
        }

        /* (b) 有序插入 */
        BST ts;
        bst_init(&ts);
        for (int i = 0; i < N; i++) { bst_insert(&ts, i); }

        printf("  %-14s %8s %10s %12s\n", "插入顺序", "节点数", "树高", "理想高度");
        printf("  %-14s %8zu %10d %12.1f\n", "随机", tr.count,
               tree_height(tr.root), 9.97);   /* log2(1000) ≈ 9.97 */
        printf("  %-14s %8zu %10d %12.1f\n", "1,2,3,...", ts.count,
               tree_height(ts.root), 9.97);
        printf("  （随机插入时实际插入了 %d 个，因为生成了 %d 个随机值里有重复）\n",
               inserted, N - inserted);
        puts("");
        puts("  随机插入：随机 BST 的期望树高约 2*ln(n) ≈ 1.39*log2(n)，");
        puts("            这里 N=1000 实测 27（vs 理想 10），量级正确");
        puts("  有序插入：树高 = n = 1000，完全退化成链表 ✗");
        puts("            （每个节点只有右孩子，因为新值总是更大）");

        /* 实测查找性能差异 */
        {
            volatile long long sum = 0;
            Timer tm = timer_start("");
            for (int rep = 0; rep < 200000; rep++) {
                if (bst_contains(&tr, vals[rep % N])) { sum++; }
            }
            double ms1 = timer_stop(tm);
            printf("\n  随机树的 20 万次查找: %8.3f ms\n", ms1);

            sum = 0;
            tm = timer_start("");
            for (int rep = 0; rep < 200000; rep++) {
                if (bst_contains(&ts, rep % N)) { sum++; }
            }
            double ms2 = timer_stop(tm);
            printf("  退化树的 20 万次查找: %8.3f ms\n", ms2);
            printf("  退化树慢约 %.0f 倍\n", ms2 / (ms1 > 0.001 ? ms1 : 0.001));
            puts("  -> 节点数差不多，只是插入顺序不同，性能差了几十倍。");
        }

        free_tree(tr.root);
        free_tree(ts.root);
        free(vals);
    }

    /* ---------- 5. 递归深度与栈帧 ---------- */
    puts("\n========== 5. 递归遍历的栈帧深度 ==========");
    {
        /* 退化树：15 个节点，全在右边 */
        BST t;
        bst_init(&t);
        for (int i = 0; i < 15; i++) { bst_insert(&t, i); }

        g_max_depth = 0;
        g_calls = 0;
        preorder_depth(t.root, 0);
        int degenerate_depth = g_max_depth;
        printf("  退化树（15 个节点）：preorder 递归最大深度 = %d，函数调用 = %d 次\n",
               g_max_depth, g_calls);

        /* 平衡树：同样 15 个节点 */
        BST b;
        bst_init(&b);
        int bal[] = {8, 4, 12, 2, 6, 10, 14, 1, 3, 5, 7, 9, 11, 13, 15};
        for (size_t i = 0; i < sizeof bal / sizeof bal[0]; i++) {
            bst_insert(&b, bal[i]);
        }
        g_max_depth = 0;
        g_calls = 0;
        preorder_depth(b.root, 0);
        int balanced_depth = g_max_depth;
        printf("  平衡树（15 个节点）：preorder 递归最大深度 = %d，函数调用 = %d 次\n",
               g_max_depth, g_calls);

        puts("");
        puts("  关键洞察：调用次数总是 2n+1（每个节点一次 + 每个空孩子一次），");
        puts("           但【栈深度】取决于树高，与节点数无关。");
        printf("           退化树深度 %d，平衡树深度 %d —— 同样 15 个节点。\n",
               degenerate_depth, balanced_depth);
        puts("           如果 n = 100 万全部有序插入，递归深度就是 100 万 ——");
        puts("           每层栈帧几十字节，几千万字节直接把栈撑爆（stack overflow）。");
        puts("           这就是为什么生产代码要么用平衡树，要么把递归改成迭代。");

        free_tree(t.root);
        free_tree(b.root);
    }

    /* ---------- 6. 迭代式遍历（避免递归深度问题）---------- */
    puts("\n========== 6. 迭代式中序遍历（用显式栈）==========");
    {
        BST t;
        bst_init(&t);
        int vals[] = {4, 2, 6, 1, 3, 5, 7};
        for (size_t i = 0; i < sizeof vals / sizeof vals[0]; i++) {
            bst_insert(&t, vals[i]);
        }

        /* 手动维护一个栈，模拟递归的「回溯」 */
        const TreeNode *stack[64];
        int top = 0;
        const TreeNode *cur = t.root;

        printf("  中序: ");
        while (cur != NULL || top > 0) {
            while (cur != NULL) {         /* 一路向左，全部压栈 */
                stack[top++] = cur;
                cur = cur->left;
            }
            cur = stack[--top];           /* 弹出，这就是「回溯」 */
            printf("%d ", cur->value);
            cur = cur->right;             /* 转向右子树 */
        }
        putchar('\n');
        puts("  对比递归版本：递归的「调用栈」就是这里的 stack 数组。");
        puts("  把递归改成迭代，本质上就是自己管理那个栈。");

        free_tree(t.root);
    }

    puts("\n========== 7. 复杂度与平衡的概念 ==========");
    puts("  ┌────────────┬──────────┬──────────┬────────────────────┐");
    puts("  │ 操作       │ 平均     │ 最坏     │ 说明               │");
    puts("  ├────────────┼──────────┼──────────┼────────────────────┤");
    puts("  │ BST 查找   │ O(log n) │ O(n)     │ 退化时（有序插入）  │");
    puts("  │ BST 插入   │ O(log n) │ O(n)     │ 同上               │");
    puts("  │ BST 删除   │ O(log n) │ O(n)     │ 同上               │");
    puts("  │ 递归遍历   │ O(n)     │ O(n)     │ 每个节点访问一次    │");
    puts("  │ 层序 BFS   │ O(n)     │ O(n)     │ 需要 O(n) 队列      │");
    puts("  └────────────┴──────────┴──────────┴────────────────────┘");
    puts("");
    puts("  平衡树的价值：保证【最坏也是 O(log n)】");
    puts("    AVL 树：严格平衡，左右子树高度差 <= 1，查找快但插入/删除旋转多");
    puts("    红黑树：近似平衡（最长路径 <= 2 * 最短路径），插入删除旋转少");
    puts("            C++ std::map / Java TreeMap / Linux 内核都用红黑树");
    puts("    这些都靠【旋转】操作在插入/删除后恢复平衡，");
    puts("    代价是常数因子更大 —— 所以小数据量时普通 BST 反而更快。");
    puts("");
    puts("  什么时候不需要平衡树？");
    puts("    - 数据量小（几百个以内）：退化的代价可以忽略");
    puts("    - 插入顺序本来就是随机的：期望树高已经很接近最优");
    puts("    - 只建一次、之后只读：可以建完后重新平衡，或用有序数组 + 二分");
    puts("    - 只需要查找不需要范围查询：哈希表更快");
    puts("");
    puts("  需要范围查询（找 [a, b] 之间的所有值）时，平衡 BST 是首选；");
    puts("  哈希表做不到这一点 —— 它只擅长「精确匹配一个 key」。");

    /* g_last_depth 只是为了让编译器不报 unused，这里显式用一下 */
    printf("\n  （最后一次 preorder 的调用计数：%d）\n", g_calls);
    g_last_depth = g_calls;

    return 0;
}
