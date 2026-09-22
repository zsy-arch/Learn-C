/*
 * tests.c —— 二叉树遍历多测试用例
 *
 * 覆盖：
 *   - 空树
 *   - 单节点
 *   - 只有左子树 / 只有右子树的链状树
 *   - 完全二叉树
 *   - 不平衡的随机树（固定种子，可重现）
 *   - 大规模树（1000+ 节点），验证莫里斯遍历不留后遗症
 *   - 遍历序列重建二叉树，重建树三种遍历序列必须与原树完全一致
 *
 * 编译：
 *   cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g tests.c -o tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct Node {
    int value;
    struct Node *left;
    struct Node *right;
} Node;

static Node *node_new(int value) {
    Node *n = malloc(sizeof *n);
    if (n == NULL) { fprintf(stderr, "malloc failed\n"); exit(1); }
    n->value = value;
    n->left = NULL;
    n->right = NULL;
    return n;
}

static void tree_free(Node *root) {
    if (root == NULL) return;
    tree_free(root->left);
    tree_free(root->right);
    free(root);
}

static int tree_equal(const Node *a, const Node *b) {
    if (a == NULL && b == NULL) return 1;
    if (a == NULL || b == NULL) return 0;
    if (a->value != b->value) return 0;
    return tree_equal(a->left, b->left) && tree_equal(a->right, b->right);
}

static Node *tree_clone(const Node *root) {
    if (root == NULL) return NULL;
    Node *n = node_new(root->value);
    n->left = tree_clone(root->left);
    n->right = tree_clone(root->right);
    return n;
}

static int tree_size(const Node *root) {
    if (root == NULL) return 0;
    return 1 + tree_size(root->left) + tree_size(root->right);
}

/* ---------- 遍历实现（与 demo.c 相同逻辑，独立一份便于测试自包含） ---------- */

static void preorder_recursive(const Node *root, int *out, int *n) {
    if (root == NULL) return;
    out[(*n)++] = root->value;
    preorder_recursive(root->left, out, n);
    preorder_recursive(root->right, out, n);
}

static void inorder_recursive(const Node *root, int *out, int *n) {
    if (root == NULL) return;
    inorder_recursive(root->left, out, n);
    out[(*n)++] = root->value;
    inorder_recursive(root->right, out, n);
}

static void postorder_recursive(const Node *root, int *out, int *n) {
    if (root == NULL) return;
    postorder_recursive(root->left, out, n);
    postorder_recursive(root->right, out, n);
    out[(*n)++] = root->value;
}

#define STACK_CAP 8192

static void preorder_iterative(const Node *root, int *out, int *n) {
    if (root == NULL) return;
    const Node *stack[STACK_CAP];
    int top = 0;
    stack[top++] = root;
    while (top > 0) {
        const Node *cur = stack[--top];
        out[(*n)++] = cur->value;
        if (cur->right != NULL) stack[top++] = cur->right;
        if (cur->left != NULL) stack[top++] = cur->left;
    }
}

static void inorder_iterative(const Node *root, int *out, int *n) {
    const Node *stack[STACK_CAP];
    int top = 0;
    const Node *cur = root;
    while (cur != NULL || top > 0) {
        while (cur != NULL) {
            stack[top++] = cur;
            cur = cur->left;
        }
        cur = stack[--top];
        out[(*n)++] = cur->value;
        cur = cur->right;
    }
}

static void postorder_iterative(const Node *root, int *out, int *n) {
    if (root == NULL) return;
    const Node *stack[STACK_CAP];
    int top = 0;
    int start = *n;
    stack[top++] = root;
    while (top > 0) {
        const Node *cur = stack[--top];
        out[(*n)++] = cur->value;
        if (cur->left != NULL) stack[top++] = cur->left;
        if (cur->right != NULL) stack[top++] = cur->right;
    }
    int lo = start, hi = *n - 1;
    while (lo < hi) {
        int tmp = out[lo]; out[lo] = out[hi]; out[hi] = tmp;
        lo++; hi--;
    }
}

#define QUEUE_CAP 8192

static void level_order(const Node *root, int *out, int *n) {
    if (root == NULL) return;
    const Node *queue[QUEUE_CAP];
    int head = 0, tail = 0;
    queue[tail++] = root;
    while (head < tail) {
        const Node *cur = queue[head++];
        out[(*n)++] = cur->value;
        if (cur->left != NULL) queue[tail++] = cur->left;
        if (cur->right != NULL) queue[tail++] = cur->right;
    }
}

static void morris_inorder(Node *root, int *out, int *n) {
    Node *cur = root;
    while (cur != NULL) {
        if (cur->left == NULL) {
            out[(*n)++] = cur->value;
            cur = cur->right;
        } else {
            Node *predecessor = cur->left;
            while (predecessor->right != NULL && predecessor->right != cur) {
                predecessor = predecessor->right;
            }
            if (predecessor->right == NULL) {
                predecessor->right = cur;
                cur = cur->left;
            } else {
                predecessor->right = NULL;
                out[(*n)++] = cur->value;
                cur = cur->right;
            }
        }
    }
}

static Node *build_from_pre_in(const int *pre, int pre_n,
                                const int *in, int in_n) {
    if (pre_n == 0) return NULL;
    int root_val = pre[0];
    int idx = -1;
    for (int i = 0; i < in_n; i++) if (in[i] == root_val) { idx = i; break; }
    assert(idx >= 0);
    int left_size = idx;
    int right_size = in_n - idx - 1;
    Node *root = node_new(root_val);
    root->left = build_from_pre_in(pre + 1, left_size, in, left_size);
    root->right = build_from_pre_in(pre + 1 + left_size, right_size,
                                     in + idx + 1, right_size);
    return root;
}

static Node *build_from_post_in(const int *post, int post_n,
                                 const int *in, int in_n) {
    if (post_n == 0) return NULL;
    int root_val = post[post_n - 1];
    int idx = -1;
    for (int i = 0; i < in_n; i++) if (in[i] == root_val) { idx = i; break; }
    assert(idx >= 0);
    int left_size = idx;
    int right_size = in_n - idx - 1;
    Node *root = node_new(root_val);
    root->left = build_from_post_in(post, left_size, in, left_size);
    root->right = build_from_post_in(post + left_size, right_size,
                                      in + idx + 1, right_size);
    return root;
}

/* ---------- 测试框架：极简的 [PASS]/[FAIL] 计数器 ---------- */

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, name) do { \
    if (cond) { g_pass++; printf("[PASS] %s\n", name); } \
    else      { g_fail++; printf("[FAIL] %s\n", name); } \
} while (0)

static int arrays_equal(const int *a, int an, const int *b, int bn) {
    if (an != bn) return 0;
    for (int i = 0; i < an; i++) if (a[i] != b[i]) return 0;
    return 1;
}

/* ---------- 随机树生成器（固定种子，可重现） ---------- */

/* 用固定种子的 LCG 而不是 rand()，保证换机器/换 libc 结果也一样 */
static unsigned long g_rng_state = 0;

static void rng_seed(unsigned long seed) { g_rng_state = seed; }

static unsigned long rng_next(void) {
    /* Numerical Recipes 的经典 LCG 参数 */
    g_rng_state = g_rng_state * 6364136223846793005UL + 1442695040888963407UL;
    return g_rng_state;
}

/* 构造一棵“随机 BST 形状”的树：按随机顺序把 1..count 插入一棵 BST，
 * 形状不平衡但仍是合法二叉树，用于压力测试遍历算法。 */
static Node *bst_insert(Node *root, int value) {
    if (root == NULL) return node_new(value);
    if (value < root->value) root->left = bst_insert(root->left, value);
    else if (value > root->value) root->right = bst_insert(root->right, value);
    /* 相等值忽略，测试用值本身互不相同，这里仅做防御 */
    return root;
}

static Node *build_random_tree(int count, unsigned long seed) {
    rng_seed(seed);
    int *values = malloc(sizeof(int) * (size_t)count);
    if (values == NULL) { fprintf(stderr, "malloc failed\n"); exit(1); }
    for (int i = 0; i < count; i++) values[i] = i;
    /* Fisher-Yates 打乱插入顺序，让树形状不可预测 */
    for (int i = count - 1; i > 0; i--) {
        int j = (int)(rng_next() % (unsigned long)(i + 1));
        int tmp = values[i]; values[i] = values[j]; values[j] = tmp;
    }
    Node *root = NULL;
    for (int i = 0; i < count; i++) root = bst_insert(root, values[i]);
    free(values);
    return root;
}

/* 构造一条“只有左子树”的链，值从 count 递减到 1 */
static Node *build_left_chain(int count) {
    Node *root = NULL;
    for (int v = 1; v <= count; v++) {
        Node *n = node_new(v);
        n->left = root;
        root = n;
    }
    return root;
}

/* 构造一条“只有右子树”的链，值从 1 递增到 count */
static Node *build_right_chain(int count) {
    Node *root = NULL, *tail = NULL;
    for (int v = 1; v <= count; v++) {
        Node *n = node_new(v);
        if (tail == NULL) root = n; else tail->right = n;
        tail = n;
    }
    return root;
}

/* ========================================================================
 * 测试用例
 * ====================================================================== */

static void test_empty_tree(void) {
    int buf[8];
    int n;

    n = 0; preorder_recursive(NULL, buf, &n);
    CHECK(n == 0, "test_empty_preorder_recursive");

    n = 0; inorder_iterative(NULL, buf, &n);
    CHECK(n == 0, "test_empty_inorder_iterative");

    n = 0; level_order(NULL, buf, &n);
    CHECK(n == 0, "test_empty_level_order");

    n = 0; morris_inorder(NULL, buf, &n);
    CHECK(n == 0, "test_empty_morris");

    Node *rebuilt = build_from_pre_in(NULL, 0, NULL, 0);
    CHECK(rebuilt == NULL, "test_empty_rebuild_pre_in");
}

static void test_single_node(void) {
    Node *root = node_new(42);
    int buf[8]; int n;

    n = 0; preorder_recursive(root, buf, &n);
    CHECK(n == 1 && buf[0] == 42, "test_single_preorder");

    n = 0; inorder_iterative(root, buf, &n);
    CHECK(n == 1 && buf[0] == 42, "test_single_inorder_iterative");

    n = 0; postorder_iterative(root, buf, &n);
    CHECK(n == 1 && buf[0] == 42, "test_single_postorder_iterative");

    n = 0; level_order(root, buf, &n);
    CHECK(n == 1 && buf[0] == 42, "test_single_level_order");

    Node *before = tree_clone(root);
    n = 0; morris_inorder(root, buf, &n);
    CHECK(n == 1 && buf[0] == 42, "test_single_morris_result");
    CHECK(tree_equal(root, before), "test_single_morris_structure_intact");
    tree_free(before);

    tree_free(root);
}

/* 对一棵任意树做“递归 vs 迭代 vs Morris(仅中序) vs 重建”的全套一致性检查 */
static void verify_all_traversals_consistent(Node *root, const char *label) {
    char name[128];
    int rec_pre[4096], rec_in[4096], rec_post[4096];
    int it_pre[4096], it_in[4096], it_post[4096];
    int lvl[4096];
    int morris[4096];
    int n;

    n = 0; preorder_recursive(root, rec_pre, &n);   int pre_n = n;
    n = 0; inorder_recursive(root, rec_in, &n);     int in_n = n;
    n = 0; postorder_recursive(root, rec_post, &n); int post_n = n;

    n = 0; preorder_iterative(root, it_pre, &n);
    snprintf(name, sizeof name, "test_%s_preorder_iter_matches_recursive", label);
    CHECK(arrays_equal(it_pre, n, rec_pre, pre_n), name);

    n = 0; inorder_iterative(root, it_in, &n);
    snprintf(name, sizeof name, "test_%s_inorder_iter_matches_recursive", label);
    CHECK(arrays_equal(it_in, n, rec_in, in_n), name);

    n = 0; postorder_iterative(root, it_post, &n);
    snprintf(name, sizeof name, "test_%s_postorder_iter_matches_recursive", label);
    CHECK(arrays_equal(it_post, n, rec_post, post_n), name);

    n = 0; level_order(root, lvl, &n);
    snprintf(name, sizeof name, "test_%s_level_order_size_matches", label);
    CHECK(n == pre_n, name);

    Node *before = tree_clone(root);
    n = 0; morris_inorder(root, morris, &n);
    snprintf(name, sizeof name, "test_%s_morris_matches_inorder", label);
    CHECK(arrays_equal(morris, n, rec_in, in_n), name);
    snprintf(name, sizeof name, "test_%s_morris_structure_intact", label);
    CHECK(tree_equal(root, before), name);
    tree_free(before);

    /* 遍历序列重建 */
    Node *rebuilt_pre_in = build_from_pre_in(rec_pre, pre_n, rec_in, in_n);
    snprintf(name, sizeof name, "test_%s_rebuild_pre_in_matches_original", label);
    CHECK(tree_equal(rebuilt_pre_in, root), name);
    tree_free(rebuilt_pre_in);

    Node *rebuilt_post_in = build_from_post_in(rec_post, post_n, rec_in, in_n);
    snprintf(name, sizeof name, "test_%s_rebuild_post_in_matches_original", label);
    CHECK(tree_equal(rebuilt_post_in, root), name);
    tree_free(rebuilt_post_in);
}

static void test_left_chain(void) {
    Node *root = build_left_chain(50);
    CHECK(tree_size(root) == 50, "test_left_chain_size");
    verify_all_traversals_consistent(root, "left_chain");
    tree_free(root);
}

static void test_right_chain(void) {
    Node *root = build_right_chain(50);
    CHECK(tree_size(root) == 50, "test_right_chain_size");
    verify_all_traversals_consistent(root, "right_chain");
    tree_free(root);
}

/*        4
 *      /   \
 *     2     6
 *    / \   / \
 *   1   3 5   7
 */
static Node *build_complete_tree(void) {
    Node *n4 = node_new(4), *n2 = node_new(2), *n6 = node_new(6);
    Node *n1 = node_new(1), *n3 = node_new(3), *n5 = node_new(5), *n7 = node_new(7);
    n4->left = n2; n4->right = n6;
    n2->left = n1; n2->right = n3;
    n6->left = n5; n6->right = n7;
    return n4;
}

static void test_complete_tree(void) {
    Node *root = build_complete_tree();
    CHECK(tree_size(root) == 7, "test_complete_tree_size");
    verify_all_traversals_consistent(root, "complete_tree");
    tree_free(root);
}

static void test_random_tree_small(void) {
    Node *root = build_random_tree(37, 12345UL);
    CHECK(tree_size(root) == 37, "test_random_small_size");
    verify_all_traversals_consistent(root, "random_small");
    tree_free(root);
}

static void test_random_tree_medium(void) {
    Node *root = build_random_tree(500, 987654321UL);
    CHECK(tree_size(root) == 500, "test_random_medium_size");
    verify_all_traversals_consistent(root, "random_medium");
    tree_free(root);
}

/* 大规模树：重点验证莫里斯遍历在大输入下 (a) 结果正确 (b) 不留后遗症 (c) 能跑完，
 * 用来对照“递归遍历在退化链状树上可能栈溢出，而莫里斯遍历是纯迭代、不受此限制”。 */
static void test_large_tree_morris(void) {
    int count = 5000;
    Node *root = build_random_tree(count, 42UL);
    CHECK(tree_size(root) == count, "test_large_tree_size");

    int *rec_in = malloc(sizeof(int) * (size_t)count);
    int *morris = malloc(sizeof(int) * (size_t)count);
    if (rec_in == NULL || morris == NULL) { fprintf(stderr, "malloc failed\n"); exit(1); }
    int n;

    n = 0; inorder_recursive(root, rec_in, &n);
    int in_n = n;

    Node *before = tree_clone(root);
    n = 0; morris_inorder(root, morris, &n);
    CHECK(arrays_equal(morris, n, rec_in, in_n), "test_large_tree_morris_matches_inorder");
    CHECK(tree_equal(root, before), "test_large_tree_morris_structure_intact");

    /* 跑第二遍，验证“线索已完全拆除”不是偶然——如果上一轮有残留线索，
     * 第二次遍历的结果或树形状会跟第一次不一致。 */
    n = 0; morris_inorder(root, morris, &n);
    CHECK(arrays_equal(morris, n, rec_in, in_n), "test_large_tree_morris_repeatable");
    CHECK(tree_equal(root, before), "test_large_tree_morris_structure_intact_after_second_run");

    tree_free(before);
    free(rec_in);
    free(morris);
    tree_free(root);
}

/* 边界：树的所有节点值都相同（重复键）——遍历算法不依赖值的大小关系，
 * 只依赖指针结构，所以这里主要验证“重建”在有重复值时的行为边界。
 * 注意：前序/中序/后序 + 中序重建算法用“值在中序序列里第一次出现的位置”
 * 定位根节点，如果值重复，定位可能不唯一——这里用不同分支形状但相同值
 * 来验证至少不会崩、且遍历本身完全正确（重建部分对全同值只做弱校验）。 */
static void test_duplicate_values_traversal_only(void) {
    Node *root = node_new(7);
    root->left = node_new(7);
    root->right = node_new(7);
    root->left->left = node_new(7);

    int buf[8]; int n;
    n = 0; inorder_recursive(root, buf, &n);
    CHECK(n == 4 && buf[0] == 7 && buf[1] == 7 && buf[2] == 7 && buf[3] == 7,
          "test_duplicate_values_inorder_count_and_values");

    Node *before = tree_clone(root);
    n = 0; morris_inorder(root, buf, &n);
    CHECK(n == 4, "test_duplicate_values_morris_count");
    CHECK(tree_equal(root, before), "test_duplicate_values_morris_structure_intact");
    tree_free(before);

    tree_free(root);
}

int main(void) {
    printf("========== 二叉树遍历测试套件 ==========\n\n");

    test_empty_tree();
    test_single_node();
    test_left_chain();
    test_right_chain();
    test_complete_tree();
    test_random_tree_small();
    test_random_tree_medium();
    test_large_tree_morris();
    test_duplicate_values_traversal_only();

    printf("\n========== 测试统计 ==========\n");
    printf("通过: %d, 失败: %d, 总计: %d\n", g_pass, g_fail, g_pass + g_fail);

    if (g_fail == 0) {
        printf("结果: 全部通过\n");
        return 0;
    } else {
        printf("结果: 存在失败用例\n");
        return 1;
    }
}
