/*
 * demo.c —— 二叉树遍历全方法
 *
 * 覆盖：
 *   1. 递归遍历：前序 / 中序 / 后序
 *   2. 迭代遍历（显式栈）：前序 / 中序 / 后序
 *   3. 层序遍历（BFS，数组模拟队列）
 *   4. 莫里斯遍历（Morris Traversal）：O(1) 空间中序遍历
 *   5. 线索二叉树入门：用莫里斯遍历留下的“线索”做对照
 *   6. 从遍历序列重建二叉树：前序+中序、后序+中序
 *
 * 编译：
 *   cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g demo.c -o demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

typedef struct Node {
    int value;
    struct Node *left;
    struct Node *right;
} Node;

static Node *node_new(int value) {
    Node *n = malloc(sizeof *n);
    if (n == NULL) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }
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

/* ---------- 打印辅助 ---------- */

static void print_array(const int *arr, int n) {
    printf("[");
    for (int i = 0; i < n; i++) {
        printf("%d", arr[i]);
        if (i + 1 < n) printf(", ");
    }
    printf("]\n");
}

/* ========================================================================
 * 1. 递归遍历：前序 / 中序 / 后序
 * ====================================================================== */

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

/* ========================================================================
 * 2. 迭代遍历（显式栈）
 *
 * 递归遍历本质上是编译器帮你维护了一个“调用栈”，里面存的是
 * “当前节点 + 你在这个节点的哪个阶段（还没访问左子树 / 刚访问完左子树 / ...）”。
 * 迭代版本就是把这个隐藏的调用栈手动搬出来。
 * ====================================================================== */

#define STACK_CAP 4096

/* 前序：最简单。用栈模拟“先压右子树、再压左子树”，
 * 保证左子树先被弹出处理（栈是后进先出）。 */
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

/* 中序：需要“一路向左压栈，弹出访问，转向右子树”这套逻辑。
 * cur 扮演“当前正在探索的指针”，stack 里存的是“还没访问自己的祖先”。 */
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

/* 后序：三种迭代写法里最麻烦的一种，因为“访问自己”要等左右子树都处理完。
 * 这里用“单栈 + 结果反转”思路：先按镜像前序（根-右-左）遍历一遍，
 * 再把这段结果整体反转，就等价于左-右-根的后序。
 *
 * 注意：教科书上常见的另一种写法是“双栈法”——一个栈做遍历、另一个栈存
 * 结果，最后依次弹出第二个栈。两者原理相同（都是反转镜像前序），但这里
 * 只用了一个栈，反转直接在输出数组上原地完成，没有第二个栈。 */
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
    /* 此刻 out[start..*n) 是“根-右-左”序，整体反转即为“左-右-根” */
    int lo = start, hi = *n - 1;
    while (lo < hi) {
        int tmp = out[lo];
        out[lo] = out[hi];
        out[hi] = tmp;
        lo++;
        hi--;
    }
}

/* ========================================================================
 * 3. 层序遍历（BFS）
 * ====================================================================== */

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

/* ========================================================================
 * 4. 莫里斯遍历（Morris Traversal）—— O(1) 空间中序遍历
 *
 * 核心想法：中序遍历里，一个节点的“中序前驱”（左子树里最右边的那个节点）
 * 遍历完之后本该“回到”这个节点——但普通遍历要么靠递归调用栈记住回去的路，
 * 要么靠显式栈。莫里斯遍历的技巧是：
 *
 *   如果 cur 有左子树，就去左子树里找“最右节点” predecessor，
 *   把 predecessor->right 临时指向 cur（借用这个本来是 NULL 的指针当“线索”），
 *   这样将来从左子树内部沿着 right 一直走，就能自动“走回” cur，
 *   不需要额外的栈或者递归。
 *
 * 遍历完 cur 的左子树之后，会重新回到 cur（通过刚才建立的线索），
 * 这时必须把线索拆除（恢复 predecessor->right = NULL），
 * 否则树的结构就被永久破坏了——这是本节重点强调的“陷阱”。
 * ====================================================================== */

static void morris_inorder(Node *root, int *out, int *n) {
    Node *cur = root;
    while (cur != NULL) {
        if (cur->left == NULL) {
            /* 没有左子树：直接访问自己，然后向右走 */
            out[(*n)++] = cur->value;
            cur = cur->right;
        } else {
            /* 找左子树里的最右节点（中序前驱） */
            Node *predecessor = cur->left;
            while (predecessor->right != NULL && predecessor->right != cur) {
                predecessor = predecessor->right;
            }
            if (predecessor->right == NULL) {
                /* 第一次到达 cur：建立线索，指向左子树探索 */
                predecessor->right = cur;
                cur = cur->left;
            } else {
                /* 线索存在，说明左子树已经走完一圈、绕回来了：
                 * 拆除线索（恢复原状），访问 cur，再向右走 */
                predecessor->right = NULL;
                out[(*n)++] = cur->value;
                cur = cur->right;
            }
        }
    }
}

/* ⚠️ 错误示例：忘记拆除线索
 *
 * 这不是内存安全问题（不会触发 ASan/UBSan），而是逻辑正确性问题：
 * 遍历完成之后，树里残留了指向“祖先”的 right 指针，
 * 树的形状被永久破坏（部分节点的 right 不再指向真正的右子树，
 * 而是指向了别的节点，等价于在树里“凭空”造出了环）。
 *
 * 容易误会的一点：这个 bug 并不会让本函数自己死循环——它照样会终止，
 * 而且在这棵演示树上连输出都是对的（1..7），所以光看输出根本发现不了。
 * 原因是 right 指针（无论是真的右孩子还是残留线索）总是指向“中序位置更
 * 靠后”的节点，而线索对每个节点最多只建立一次，所以 cur 不可能无限绕圈。
 * 真正的代价被推迟了：之后谁再沿着这些指针做递归（tree_free、递归遍历），
 * 就会掉进环里出不来。
 */
static void morris_inorder_BROKEN_no_restore(Node *root, int *out, int *n) {
    Node *cur = root;
    /* guard 只是一根保险丝，不是算法必需的：上面解释了本函数在这棵演示树上
     * 本来就会正常终止。留着它是为了万一以后改了树的形状也不会把 demo 卡死。 */
    int guard = 0;
    while (cur != NULL && guard < 100000) {
        guard++;
        if (cur->left == NULL) {
            out[(*n)++] = cur->value;
            cur = cur->right;
        } else {
            Node *predecessor = cur->left;
            while (predecessor->right != NULL && predecessor->right != cur) {
                predecessor = predecessor->right;
            }
            if (predecessor->right == NULL) {
                predecessor->right = cur;   /* 建立线索 */
                cur = cur->left;
            } else {
                /* ❌ 这里故意不拆线索：predecessor->right = NULL; 被删掉了 */
                out[(*n)++] = cur->value;
                cur = cur->right;
            }
        }
    }
}

/* 判断两棵树的形状与值是否完全相同——用来验证“莫里斯遍历后树没被破坏” */
static int tree_equal(const Node *a, const Node *b) {
    if (a == NULL && b == NULL) return 1;
    if (a == NULL || b == NULL) return 0;
    if (a->value != b->value) return 0;
    return tree_equal(a->left, b->left) && tree_equal(a->right, b->right);
}

/* 深拷贝一棵树——用于“遍历前后对比”的基准 */
static Node *tree_clone(const Node *root) {
    if (root == NULL) return NULL;
    Node *n = node_new(root->value);
    n->left = tree_clone(root->left);
    n->right = tree_clone(root->right);
    return n;
}

/* 在树还没被破坏之前，把所有节点指针收集到一个数组里。
 *
 * 这是给“忘记拆除线索”的错误演示专门准备的安全网：
 * 一旦 right 指针被污染成指向祖先，树就出现了环，
 * 这时任何基于“沿着 left/right 递归”的操作（包括 tree_free 本身）
 * 都会陷入无限递归。提前用普通递归（此时树还是合法的树）把节点地址
 * 记下来，之后就能不依赖被破坏的指针结构、直接按地址逐个 free。 */
static void collect_nodes(Node *root, Node **out, int *n) {
    if (root == NULL) return;
    collect_nodes(root->left, out, n);
    out[(*n)++] = root;
    collect_nodes(root->right, out, n);
}

/* ========================================================================
 * 5. 线索二叉树入门
 *
 * 莫里斯遍历建立的“线索”是临时的、遍历完就拆掉的。
 * 而经典的“线索二叉树”（threaded binary tree）是把这个思路“持久化”：
 * 给每个节点加一个标志位，标记它的 right 指针到底是“真正的右子树”
 * 还是“指向中序后继的线索”。这样一来，整棵树自带一条“中序遍历链”，
 * 不需要栈、不需要递归、也不需要临时借用指针再恢复——查中序后继是 O(1)。
 *
 * 这里做一个最小实验：把线索“建立但不拆除”，此时 root 事实上已经变成了
 * 一棵“右线索二叉树”的雏形——每个中序遍历中“没有右子树”的节点，
 * 它的 right 都指向了自己的中序后继。用这个线索链重新走一遍，
 * 应该能得到和普通中序遍历完全一样的序列。
 * ====================================================================== */

typedef struct ThreadedNode {
    int value;
    struct ThreadedNode *left;
    struct ThreadedNode *right;
    int right_is_thread;   /* 1 表示 right 是线索（指向中序后继），0 表示真右子树 */
} ThreadedNode;

static ThreadedNode *tnode_new(int value) {
    ThreadedNode *n = malloc(sizeof *n);
    if (n == NULL) { fprintf(stderr, "malloc failed\n"); exit(1); }
    n->value = value;
    n->left = NULL;
    n->right = NULL;
    n->right_is_thread = 0;
    return n;
}

static void tfree(ThreadedNode *root) {
    if (root == NULL) return;
    tfree(root->left);
    if (!root->right_is_thread) tfree(root->right);
    free(root);
}

/* 用递归中序遍历建立右线索：对每个“没有右子树”的节点，
 * 把它的 right 指向“它的中序后继”。 */
static ThreadedNode *g_thread_prev = NULL;

static void build_right_threads(ThreadedNode *root) {
    if (root == NULL) return;
    build_right_threads(root->left);
    if (g_thread_prev != NULL && g_thread_prev->right == NULL) {
        g_thread_prev->right = root;
        g_thread_prev->right_is_thread = 1;
    }
    g_thread_prev = root;
    build_right_threads(root->right);
}

/* 用建立好的右线索做“O(1) 找中序后继”遍历：
 * 从最左节点出发，每次要么走真正的右子树（再一路向左），
 * 要么直接沿着线索走到后继。完全不需要栈。 */
static void threaded_inorder_walk(ThreadedNode *root, int *out, int *n) {
    if (root == NULL) return;
    ThreadedNode *cur = root;
    while (cur->left != NULL) cur = cur->left;
    while (cur != NULL) {
        out[(*n)++] = cur->value;
        if (cur->right_is_thread) {
            cur = cur->right;               /* O(1) 线索跳转 */
        } else if (cur->right != NULL) {
            cur = cur->right;
            while (cur->left != NULL) cur = cur->left;
        } else {
            cur = NULL;                     /* 到达中序遍历的最后一个节点 */
        }
    }
}

static ThreadedNode *tclone_from_node(const Node *root) {
    if (root == NULL) return NULL;
    ThreadedNode *n = tnode_new(root->value);
    n->left = tclone_from_node(root->left);
    n->right = tclone_from_node(root->right);
    n->right_is_thread = 0;
    return n;
}

/* ========================================================================
 * 6. 从遍历序列重建二叉树
 * ====================================================================== */

/* 前序 + 中序 -> 重建
 *
 * 前序的第一个元素一定是“当前子树的根”。
 * 用这个根在中序序列里定位，左边就是左子树的中序序列，右边是右子树的中序序列。
 * 知道了左子树的节点数 k，就能在前序序列里把 [1, 1+k) 切给左子树，剩下切给右子树。
 */
static Node *build_from_pre_in(const int *pre, int pre_n,
                                const int *in, int in_n) {
    if (pre_n == 0) {
        assert(in_n == 0);
        return NULL;
    }
    int root_val = pre[0];
    int root_idx_in_in = -1;
    for (int i = 0; i < in_n; i++) {
        if (in[i] == root_val) { root_idx_in_in = i; break; }
    }
    assert(root_idx_in_in >= 0); /* 前序/中序必须来自同一棵树，否则数据不一致 */

    int left_size = root_idx_in_in;
    int right_size = in_n - root_idx_in_in - 1;

    Node *root = node_new(root_val);
    root->left = build_from_pre_in(pre + 1, left_size,
                                    in, left_size);
    root->right = build_from_pre_in(pre + 1 + left_size, right_size,
                                     in + root_idx_in_in + 1, right_size);
    return root;
}

/* 后序 + 中序 -> 重建
 *
 * 思路对称：后序的最后一个元素是根。
 * 后序序列结构是 [左子树后序][右子树后序][根]，
 * 所以要从后往前切。
 */
static Node *build_from_post_in(const int *post, int post_n,
                                 const int *in, int in_n) {
    if (post_n == 0) {
        assert(in_n == 0);
        return NULL;
    }
    int root_val = post[post_n - 1];
    int root_idx_in_in = -1;
    for (int i = 0; i < in_n; i++) {
        if (in[i] == root_val) { root_idx_in_in = i; break; }
    }
    assert(root_idx_in_in >= 0);

    int left_size = root_idx_in_in;
    int right_size = in_n - root_idx_in_in - 1;

    Node *root = node_new(root_val);
    root->left = build_from_post_in(post, left_size,
                                     in, left_size);
    root->right = build_from_post_in(post + left_size, right_size,
                                      in + root_idx_in_in + 1, right_size);
    return root;
}

/* ========================================================================
 * 构造几棵演示用的树
 * ====================================================================== */

/*        4
 *      /   \
 *     2     6
 *    / \   / \
 *   1   3 5   7
 */
static Node *build_complete_tree(void) {
    Node *n4 = node_new(4);
    Node *n2 = node_new(2);
    Node *n6 = node_new(6);
    Node *n1 = node_new(1);
    Node *n3 = node_new(3);
    Node *n5 = node_new(5);
    Node *n7 = node_new(7);
    n4->left = n2;  n4->right = n6;
    n2->left = n1;  n2->right = n3;
    n6->left = n5;  n6->right = n7;
    return n4;
}

int main(void) {
    printf("========== 1. 构造演示树并做递归遍历 ==========\n");
    Node *root = build_complete_tree();
    printf("  树结构（完全二叉树）：\n");
    printf("          4\n");
    printf("        /   \\\n");
    printf("       2     6\n");
    printf("      / \\   / \\\n");
    printf("     1   3 5   7\n\n");

    int buf[64];
    int n;

    n = 0; preorder_recursive(root, buf, &n);
    printf("  前序（递归）: "); print_array(buf, n);
    n = 0; inorder_recursive(root, buf, &n);
    printf("  中序（递归）: "); print_array(buf, n);
    n = 0; postorder_recursive(root, buf, &n);
    printf("  后序（递归）: "); print_array(buf, n);

    printf("\n========== 2. 迭代遍历（显式栈），结果应与递归一致 ==========\n");
    n = 0; preorder_iterative(root, buf, &n);
    printf("  前序（迭代）: "); print_array(buf, n);
    n = 0; inorder_iterative(root, buf, &n);
    printf("  中序（迭代）: "); print_array(buf, n);
    n = 0; postorder_iterative(root, buf, &n);
    printf("  后序（迭代）: "); print_array(buf, n);

    printf("\n========== 3. 层序遍历（BFS） ==========\n");
    n = 0; level_order(root, buf, &n);
    printf("  层序: "); print_array(buf, n);

    printf("\n========== 4. 莫里斯遍历（O(1) 空间中序） ==========\n");
    Node *before = tree_clone(root);
    n = 0; morris_inorder(root, buf, &n);
    printf("  莫里斯中序: "); print_array(buf, n);
    int intact = tree_equal(root, before);
    printf("  遍历后树结构与遍历前完全一致？ %s\n", intact ? "是（线索已正确拆除）" : "否（树被破坏了！）");
    assert(intact && "莫里斯遍历后树结构必须保持不变");
    tree_free(before);

    printf("\n  ⚠️ 错误示例：如果忘记拆除线索会怎样？\n");
    Node *broken_before = tree_clone(root);
    Node *broken_root = tree_clone(root);
    /* 树还没被破坏之前，先把所有节点地址记下来——
     * 破坏之后 tree_free 会因为环形指针而栈溢出，见下方“常见错误”。 */
    Node *broken_nodes[64];
    int broken_node_count = 0;
    collect_nodes(broken_root, broken_nodes, &broken_node_count);

    n = 0; morris_inorder_BROKEN_no_restore(broken_root, buf, &n);
    printf("  “错误版”遍历结果: "); print_array(buf, n);
    int broken_intact = tree_equal(broken_root, broken_before);
    printf("  遍历后树结构与遍历前一致？ %s\n", broken_intact ? "是" : "否（树的 right 指针被永久污染，形状已改变）");
    printf("  说明：被污染的节点是每个“有左子树”的节点的「中序前驱」——也就是它\n");
    printf("        左子树里最右侧的那个节点（那里原本 right == NULL，正好被借去当线索）。\n");
    printf("        本例中就是叶子 1、3、5：它们的 right 现在分别指向自己的中序后继\n");
    printf("        2、4、6，而不再是 NULL。于是 1->right==2 和 2->left==1 首尾相接，\n");
    printf("        树里出现了环。如果之后再对这棵“坏树”做普通递归中序遍历或普通\n");
    printf("        递归释放（tree_free），递归就会在这个环里来回打转、永远到不了\n");
    printf("        NULL，本实验最初就是这样把自己写进了 stack-overflow（详见 README）。\n");
    printf("        下面按“破坏前收集好的地址列表”安全释放这些节点，而不是沿着\n");
    printf("        被污染的指针递归释放。\n");
    tree_free(broken_before);
    for (int i = 0; i < broken_node_count; i++) {
        free(broken_nodes[i]);
    }

    printf("\n========== 5. 线索二叉树：把“临时线索”变成“持久线索” ==========\n");
    ThreadedNode *troot = tclone_from_node(root);
    g_thread_prev = NULL;
    build_right_threads(troot);
    n = 0; threaded_inorder_walk(troot, buf, &n);
    printf("  线索中序遍历: "); print_array(buf, n);
    printf("  说明：叶子节点 1、3、5 的 right 现在是“线索”，分别指向它们的中序后继 2、4、6；\n");
    printf("        节点 7 没有中序后继，right 保持 NULL（线索链的终点）。\n");
    tfree(troot);

    printf("\n========== 6. 从遍历序列重建二叉树 ==========\n");
    int pre[7], in[7], post[7];
    int pn = 0, in_n = 0, post_n = 0;
    preorder_recursive(root, pre, &pn);
    inorder_recursive(root, in, &in_n);
    postorder_recursive(root, post, &post_n);

    printf("  原始前序: "); print_array(pre, pn);
    printf("  原始中序: "); print_array(in, in_n);
    printf("  原始后序: "); print_array(post, post_n);

    Node *rebuilt1 = build_from_pre_in(pre, pn, in, in_n);
    n = 0; preorder_recursive(rebuilt1, buf, &n);
    printf("\n  [前序+中序 重建] 重建树的前序: "); print_array(buf, n);
    n = 0; inorder_recursive(rebuilt1, buf, &n);
    printf("  [前序+中序 重建] 重建树的中序: "); print_array(buf, n);
    assert(tree_equal(rebuilt1, root) && "前序+中序重建的树必须与原树完全相同");
    printf("  重建树与原树结构完全一致：是\n");

    Node *rebuilt2 = build_from_post_in(post, post_n, in, in_n);
    n = 0; postorder_recursive(rebuilt2, buf, &n);
    printf("\n  [后序+中序 重建] 重建树的后序: "); print_array(buf, n);
    n = 0; inorder_recursive(rebuilt2, buf, &n);
    printf("  [后序+中序 重建] 重建树的中序: "); print_array(buf, n);
    assert(tree_equal(rebuilt2, root) && "后序+中序重建的树必须与原树完全相同");
    printf("  重建树与原树结构完全一致：是\n");

    tree_free(rebuilt1);
    tree_free(rebuilt2);
    tree_free(root);

    printf("\n========== 全部演示完成，无内存泄漏（见 sanitizer 输出） ==========\n");
    return 0;
}
