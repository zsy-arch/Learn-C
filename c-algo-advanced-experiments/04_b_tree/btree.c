#include "btree.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static BTreeNode *node_create(int t, bool is_leaf) {
    BTreeNode *x = malloc(sizeof *x);
    if (!x) { perror("malloc"); exit(1); }
    x->n = 0;
    x->is_leaf = is_leaf;
    x->keys = malloc(sizeof(int) * (size_t)(2 * t - 1));
    x->children = malloc(sizeof(BTreeNode *) * (size_t)(2 * t));
    if (!x->keys || !x->children) { perror("malloc"); exit(1); }
    return x;
}

static void node_free(BTreeNode *x) {
    free(x->keys);
    free(x->children);
    free(x);
}

BTree btree_create(int t) {
    /* t 必须 >= 2，在入口处就拦住，而不是让它到很远的地方去制造后果。
     *
     * t = 1 时最小度数退化：非 root 节点的 key 下限是 t-1 = 0，节点容量
     * 2t-1 = 1，整棵"B 树"变成一条链，所有关于平衡的推导全部失效。
     * t <= 0 更糟：node_create 里 `2 * t - 1` 是负数，强转成 size_t 之后
     * 变成一个接近 SIZE_MAX 的巨大值，malloc 要么失败要么给出一块完全
     * 不是预期大小的内存——真正的错误（传了个非法的 t）和暴露出来的现象
     * （malloc 失败 / 写越界）之间隔着好几层，排查起来极其费劲。 */
    if (t < 2) {
        fprintf(stderr, "btree_create: t must be >= 2 (got %d)\n", t);
        exit(1);
    }
    BTree tree;
    tree.root = NULL;
    tree.t = t;
    return tree;
}

static void destroy_recursive(BTreeNode *x) {
    if (!x) return;
    if (!x->is_leaf) {
        for (int i = 0; i <= x->n; i++) destroy_recursive(x->children[i]);
    }
    node_free(x);
}

void btree_destroy(BTree *tree) {
    destroy_recursive(tree->root);
    tree->root = NULL;
}

bool btree_search(const BTree *tree, int key) {
    const BTreeNode *x = tree->root;
    while (x) {
        int i = 0;
        while (i < x->n && key > x->keys[i]) i++;
        if (i < x->n && key == x->keys[i]) return true;
        if (x->is_leaf) return false;
        x = x->children[i];
    }
    return false;
}

/* Split the full child children[i] of x. child has 2t-1 keys.
 * Middle key (index t-1) moves up into x; the right half becomes a new
 * sibling node inserted at children[i+1]. */
static void split_child(BTreeNode *x, int i, int t) {
    BTreeNode *y = x->children[i];      /* full child, y->n == 2t-1 */
    BTreeNode *z = node_create(t, y->is_leaf);

    z->n = t - 1;
    for (int j = 0; j < t - 1; j++) z->keys[j] = y->keys[j + t];
    if (!y->is_leaf) {
        for (int j = 0; j < t; j++) z->children[j] = y->children[j + t];
    }
    int mid_key = y->keys[t - 1];
    y->n = t - 1;

    for (int j = x->n; j >= i + 1; j--) x->children[j + 1] = x->children[j];
    x->children[i + 1] = z;

    for (int j = x->n - 1; j >= i; j--) x->keys[j + 1] = x->keys[j];
    x->keys[i] = mid_key;
    x->n++;
}

/* Insert into a subtree rooted at x that is guaranteed to be non-full.
 *
 * 前置条件：调用方（btree_insert）已经确认 key 不在树里。下面两处
 * `key == ...` 的判断因此永远不会命中，保留它们纯粹是"多一道防线"，
 * **不能**当成"本函数自己保证了重复键不改结构"——它保证不了，原因见
 * btree_insert 里的长注释：第二处判断发生在 split_child **之后**。 */
static bool insert_nonfull(BTreeNode *x, int key, int t) {
    int i = 0;
    while (i < x->n && key > x->keys[i]) i++;
    if (i < x->n && key == x->keys[i]) return false; /* duplicate */

    if (x->is_leaf) {
        for (int j = x->n - 1; j >= i; j--) x->keys[j + 1] = x->keys[j];
        x->keys[i] = key;
        x->n++;
        return true;
    }

    if (x->children[i]->n == 2 * t - 1) {
        split_child(x, i, t);
        /* 注意顺序：分裂已经发生了，这一行才开始比较。如果没有上层的
         * 预先查找，走到这里返回 false 的话，树已经被改过了。 */
        if (key == x->keys[i]) return false; /* the key that moved up */
        if (key > x->keys[i]) i++;
    }
    return insert_nonfull(x->children[i], key, t);
}

bool btree_insert(BTree *tree, int key) {
    int t = tree->t;
    if (!tree->root) {
        tree->root = node_create(t, true);
        tree->root->keys[0] = key;
        tree->root->n = 1;
        return true;
    }

    /* 动手之前先查一遍：key 已存在就直接返回，一个字节都不碰。
     *
     * 这一行是"主动分裂"策略的必要代价，理由值得写清楚，因为它正是
     * 本模块修掉的一个缺陷（而且第一版只修了一半）。
     *
     * insert 的契约（btree.h 顶部）是"重复键返回 false 且**不做任何
     * 修改**"。但主动分裂的顺序天然和这条契约冲突：**先分裂，再比较**。
     * 于是任何"边下降边判重"的写法都会在某条路径上先改结构、后发现重复。
     * 两处都会中招：
     *
     *   (1) root 这一层：root 满时无条件新建 root + 分裂旧 root，
     *       再交给 insert_nonfull 去发现重复。节点数 1 -> 3，树高 0 -> 1。
     *
     *   (2) 下降路径上任意一层：insert_nonfull 发现"即将进入的孩子满了"
     *       就先 split_child，分裂之后才比较被提上去的那个 key。
     *       t=2、树形 root=[20]、孩子 [10] 和 [30 40 50]，插已存在的 40：
     *         分裂后 -> root=[20 40]，孩子 [10] [30] [50]（节点数 3 -> 4）
     *         然后 `key == x->keys[i]` 命中，返回 false。
     *       插 30 或 50 同样会先触发这次分裂，只是重复是在更深一层发现的。
     *
     * 第一版只在 (1) 处加了 `btree_search`，(2) 一直漏着——因为三个
     * test_dup_insert_* 回归用例构造的树 root 全是满的，每次都被 (1) 的
     * 检查挡住了，(2) 那条路径根本没被走到。现在把检查提到最前面，两条
     * 路径一起覆盖，`test_dup_insert_into_full_child_is_noop` 专门钉 (2)。
     *
     * 为什么这类 bug 特别难查：返回值是对的，分裂出来的树也**仍然是一棵
     * 合法 B 树**，所以 btree_verify 一个字都不会说（它检查"合法性"，不
     * 检查"有没有改"）。调用方看到 false 会认为"什么都没发生"，而实际上
     * 树的形状已经变了。回归测试因此不能靠 verify，只能比较结构指纹。
     *
     * 代价：每次插入多一次 O(log_t n) 的下降，和紧接着那次真正的插入下降
     * 同阶。A/B 实测（-O2，10 万随机 key，两个版本交替各跑 3 轮取稳定值，
     * 丢掉第一轮冷 cache 的离群点）：
     *   t=2  0.137 -> 0.210 us/op  (x1.53)  树高 12
     *   t=8  0.060 -> 0.099 us/op  (x1.64)  树高 4
     *   t=64 0.056 -> 0.097 us/op  (x1.73)  树高 2
     * 注意涨幅的方向和直觉相反：树最高的 t=2 涨得最少，树最矮的 t=64
     * 涨得最多。原因是这次预查找走的正是紧接着插入要走的同一条路径，
     * 节点都还在 cache 里，省掉的是访存、省不掉的是节点内那趟线性
     * 比较——而线性比较的长度正比于 t（t=64 的节点最多 127 个 key）。
     * t=2 那边真正的耗时大头是分裂时的 memmove 和 malloc，预查找不碰
     * 这部分，所以被摊薄了。
     *
     * 这是一个明确的取舍：用"每次插入多一次查找"换"契约无条件成立"。
     * 选后者的理由是，契约被破坏时的表现是静默的结构漂移——调用方看到
     * false 以为什么都没发生，btree_verify 也查不出来（树依然合法）；
     * 而慢 1.5~1.7 倍是可测量、可预期的。若某个场景确实在意这点开销，
     * 正确的做法是改接口（比如让 insert 返回"是否已存在"并允许结构
     * 变化），而不是留一条静默违约的路径。 */
    if (btree_search(tree, key)) return false;

    if (tree->root->n == 2 * t - 1) {
        BTreeNode *new_root = node_create(t, false);
        new_root->children[0] = tree->root;
        tree->root = new_root;
        split_child(new_root, 0, t);
    }
    return insert_nonfull(tree->root, key, t);
}

/* ---- Deletion ---- */

static int find_max(BTreeNode *x) {
    while (!x->is_leaf) x = x->children[x->n];
    return x->keys[x->n - 1];
}

static int find_min(BTreeNode *x) {
    while (!x->is_leaf) x = x->children[0];
    return x->keys[0];
}

/* Merge children[i] and children[i+1] of x, pulling down x->keys[i] as
 * the separator. Result replaces children[i]; children[i+1] is freed. */
static void merge_children(BTreeNode *x, int i, int t) {
    BTreeNode *left = x->children[i];
    BTreeNode *right = x->children[i + 1];

    left->keys[left->n] = x->keys[i];
    for (int j = 0; j < right->n; j++) left->keys[left->n + 1 + j] = right->keys[j];
    if (!left->is_leaf) {
        for (int j = 0; j <= right->n; j++) left->children[left->n + 1 + j] = right->children[j];
    }
    left->n = left->n + 1 + right->n;

    for (int j = i; j < x->n - 1; j++) x->keys[j] = x->keys[j + 1];
    for (int j = i + 1; j < x->n; j++) x->children[j] = x->children[j + 1];
    x->n--;

    node_free(right);
    (void)t;
}

/* Ensure children[i] of x has at least t keys before descending into it,
 * by borrowing from a sibling (cases 3a/3b) or merging (case 3c). */
static void fill_child(BTreeNode *x, int i, int t) {
    if (i > 0 && x->children[i - 1]->n >= t) {
        /* 3a: borrow from left sibling (right-rotate through the parent) */
        BTreeNode *child = x->children[i];
        BTreeNode *left = x->children[i - 1];

        for (int j = child->n - 1; j >= 0; j--) child->keys[j + 1] = child->keys[j];
        if (!child->is_leaf) {
            for (int j = child->n; j >= 0; j--) child->children[j + 1] = child->children[j];
        }
        child->keys[0] = x->keys[i - 1];
        if (!child->is_leaf) child->children[0] = left->children[left->n];
        child->n++;

        x->keys[i - 1] = left->keys[left->n - 1];
        left->n--;
    } else if (i < x->n && x->children[i + 1]->n >= t) {
        /* 3b: borrow from right sibling (left-rotate through the parent) */
        BTreeNode *child = x->children[i];
        BTreeNode *right = x->children[i + 1];

        child->keys[child->n] = x->keys[i];
        if (!child->is_leaf) child->children[child->n + 1] = right->children[0];
        child->n++;

        x->keys[i] = right->keys[0];
        for (int j = 0; j < right->n - 1; j++) right->keys[j] = right->keys[j + 1];
        if (!right->is_leaf) {
            for (int j = 0; j <= right->n - 1; j++) right->children[j] = right->children[j + 1];
        }
        right->n--;
    } else {
        /* 3c: merge with a sibling. Prefer the right sibling unless we're
         * at the last child, in which case merge with the left one. */
        if (i < x->n) merge_children(x, i, t);
        else merge_children(x, i - 1, t);
    }
}

/* Delete key from the subtree rooted at x, which is guaranteed (by the
 * caller) to have at least t keys, OR be the tree root. */
static void delete_from(BTreeNode *x, int key, int t) {
    int i = 0;
    while (i < x->n && key > x->keys[i]) i++;

    if (i < x->n && x->keys[i] == key) {
        if (x->is_leaf) {
            /* case 1 */
            for (int j = i; j < x->n - 1; j++) x->keys[j] = x->keys[j + 1];
            x->n--;
            return;
        }

        BTreeNode *pred_child = x->children[i];
        BTreeNode *succ_child = x->children[i + 1];

        if (pred_child->n >= t) {
            /* case 2a */
            int pred_key = find_max(pred_child);
            x->keys[i] = pred_key;
            delete_from(pred_child, pred_key, t);
        } else if (succ_child->n >= t) {
            /* case 2b */
            int succ_key = find_min(succ_child);
            x->keys[i] = succ_key;
            delete_from(succ_child, succ_key, t);
        } else {
            /* case 2c: merge pred_child, key, succ_child into one node,
             * then the key to delete is now inside that merged node. */
            merge_children(x, i, t);
            delete_from(pred_child, key, t);
        }
        return;
    }

    if (x->is_leaf) return; /* key not present anywhere; no-op */

    bool last_child = (i == x->n);
    if (x->children[i]->n < t) fill_child(x, i, t);

    /* fill_child may have merged children[i] into children[i-1] when i
     * was the last index, shifting who holds the key's subtree now. */
    if (last_child && i > x->n) delete_from(x->children[i - 1], key, t);
    else delete_from(x->children[i], key, t);
}

bool btree_delete(BTree *tree, int key) {
    if (!tree->root) return false;
    if (!btree_search(tree, key)) return false;

    delete_from(tree->root, key, tree->t);

    if (tree->root->n == 0) {
        BTreeNode *old_root = tree->root;
        tree->root = old_root->is_leaf ? NULL : old_root->children[0];
        node_free(old_root);
    }
    return true;
}

/* ---- Stats ---- */

static int count_keys_rec(const BTreeNode *x) {
    if (!x) return 0;
    int total = x->n;
    if (!x->is_leaf) for (int i = 0; i <= x->n; i++) total += count_keys_rec(x->children[i]);
    return total;
}

int btree_count_keys(const BTree *tree) { return count_keys_rec(tree->root); }

static int count_nodes_rec(const BTreeNode *x) {
    if (!x) return 0;
    int total = 1;
    if (!x->is_leaf) for (int i = 0; i <= x->n; i++) total += count_nodes_rec(x->children[i]);
    return total;
}

int btree_count_nodes(const BTree *tree) { return count_nodes_rec(tree->root); }

int btree_height(const BTree *tree) {
    int h = -1;
    const BTreeNode *x = tree->root;
    while (x) {
        h++;
        if (x->is_leaf) break;
        x = x->children[0];
    }
    return h;
}

/* ---- Verification ---- */

typedef struct {
    char *buf;
    size_t size;
    bool failed;
} VerifyCtx;

static void fail(VerifyCtx *ctx, const char *fmt, ...) {
    if (ctx->failed) return;
    ctx->failed = true;
    if (ctx->buf && ctx->size > 0) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(ctx->buf, ctx->size, fmt, args);
        va_end(args);
    }
}

/* Returns leaf depth of this subtree (via out param) for cross-checking
 * that every leaf is at the same depth. min_key/max_key (may be NULL)
 * bound the legal key range inherited from the parent. */
static void verify_rec(const BTreeNode *x, int t, bool is_root, int depth,
                        const int *min_key, const int *max_key,
                        int *leaf_depth_out, VerifyCtx *ctx) {
    if (ctx->failed) return;

    int max_keys = 2 * t - 1;
    /* root 的下限要分叶子和非叶子两种情况，不能一律放成 0。
     *
     * root 是叶子时 0 个 key 是合法的——那就是空树。但 root 是**内部
     * 节点**时至少要有 1 个 key：内部节点有 n+1 个孩子，0 个 key 意味着
     * 只剩 1 个孩子，而这种形状恰恰是"应该已经被 root 收缩掉"的状态
     * （btree_delete 末尾那段 `if (tree->root->n == 0) { ... = old_root->
     * is_leaf ? NULL : old_root->children[0]; }` 就是专门做这件事的：
     * 叶子 root 清空了就变 NULL，内部 root 清空了就让唯一的孩子顶上）。
     * 原来写成 `is_root ? 0 : t - 1`，于是一棵忘记收缩
     * 的树——root 有 0 个 key、挂着 1 个孩子、白白多出一层高度——能完整
     * 通过校验。这正是校验器最不该放过的那类问题：树还能正常 search，
     * 只是永久多背了一层，而且这一层会在后续每次插入/删除里继续传播。 */
    int min_keys;
    if (x->is_leaf) min_keys = is_root ? 0 : t - 1;
    else            min_keys = is_root ? 1 : t - 1;
    if (x->n > max_keys) { fail(ctx, "node has %d keys, max is %d", x->n, max_keys); return; }
    if (x->n < min_keys) { fail(ctx, "node has %d keys, min is %d (is_root=%d)", x->n, min_keys, is_root); return; }

    for (int i = 0; i < x->n - 1; i++) {
        if (x->keys[i] >= x->keys[i + 1]) {
            fail(ctx, "keys not strictly increasing at index %d: %d >= %d", i, x->keys[i], x->keys[i + 1]);
            return;
        }
    }
    if (min_key && x->n > 0 && x->keys[0] <= *min_key) {
        fail(ctx, "smallest key %d does not respect lower bound %d", x->keys[0], *min_key);
        return;
    }
    if (max_key && x->n > 0 && x->keys[x->n - 1] >= *max_key) {
        fail(ctx, "largest key %d does not respect upper bound %d", x->keys[x->n - 1], *max_key);
        return;
    }

    if (x->is_leaf) {
        if (*leaf_depth_out == -1) *leaf_depth_out = depth;
        else if (*leaf_depth_out != depth) {
            fail(ctx, "leaf depth mismatch: saw %d and %d", *leaf_depth_out, depth);
        }
        return;
    }

    for (int i = 0; i <= x->n; i++) {
        if (!x->children[i]) { fail(ctx, "internal node missing child pointer at index %d", i); return; }
        const int *lo = (i == 0) ? min_key : &x->keys[i - 1];
        const int *hi = (i == x->n) ? max_key : &x->keys[i];
        verify_rec(x->children[i], t, false, depth + 1, lo, hi, leaf_depth_out, ctx);
        if (ctx->failed) return;
    }
}

bool btree_verify(const BTree *tree, char *err_buf, size_t err_buf_size) {
    if (err_buf && err_buf_size > 0) err_buf[0] = '\0';
    if (!tree->root) return true; /* empty tree trivially satisfies all properties */

    VerifyCtx ctx = { err_buf, err_buf_size, false };
    int leaf_depth = -1;
    verify_rec(tree->root, tree->t, true, 0, NULL, NULL, &leaf_depth, &ctx);
    return !ctx.failed;
}

/* ---- Printing ---- */

static void print_rec(const BTreeNode *x, int depth) {
    if (!x) return;
    for (int i = 0; i < depth; i++) printf("  ");
    printf("[");
    for (int i = 0; i < x->n; i++) printf("%d%s", x->keys[i], i + 1 < x->n ? " " : "");
    printf("]\n");
    if (!x->is_leaf) for (int i = 0; i <= x->n; i++) print_rec(x->children[i], depth + 1);
}

void btree_print(const BTree *tree) {
    if (!tree->root) { printf("  (empty tree)\n"); return; }
    print_rec(tree->root, 0);
}
