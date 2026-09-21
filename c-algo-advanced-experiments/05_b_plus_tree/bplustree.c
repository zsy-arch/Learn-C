#include "bplustree.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static BPlusNode *node_create(int t, bool is_leaf) {
    BPlusNode *x = malloc(sizeof *x);
    if (!x) { perror("malloc"); exit(1); }
    x->is_leaf = is_leaf;
    x->n = 0;
    x->keys = malloc(sizeof(int) * (size_t)(2 * t - 1));
    if (!x->keys) { perror("malloc"); exit(1); }
    if (is_leaf) {
        x->u.values = malloc(sizeof(int) * (size_t)(2 * t - 1));
        if (!x->u.values) { perror("malloc"); exit(1); }
    } else {
        x->u.children = malloc(sizeof(BPlusNode *) * (size_t)(2 * t));
        if (!x->u.children) { perror("malloc"); exit(1); }
    }
    x->next = NULL;
    return x;
}

static void node_free(BPlusNode *x) {
    free(x->keys);
    if (x->is_leaf) free(x->u.values);
    else free(x->u.children);
    free(x);
}

BPlusTree bplustree_create(int t) {
    /* t < 2 is documented as unsupported in bplustree.h; enforce it here
     * instead of letting it corrupt node capacities (2t-1 would be <= 1)
     * far away from the actual mistake. */
    if (t < 2) {
        fprintf(stderr, "bplustree_create: t must be >= 2 (got %d)\n", t);
        exit(1);
    }
    BPlusTree tree;
    tree.root = NULL;
    tree.t = t;
    return tree;
}

static void destroy_recursive(BPlusNode *x) {
    if (!x) return;
    if (!x->is_leaf) {
        for (int i = 0; i <= x->n; i++) destroy_recursive(x->u.children[i]);
    }
    node_free(x);
}

void bplustree_destroy(BPlusTree *tree) {
    destroy_recursive(tree->root);
    tree->root = NULL;
}

/* ---- Search ---- */

bool bplustree_search(const BPlusTree *tree, int key, int *value_out) {
    const BPlusNode *x = tree->root;
    if (!x) return false;
    while (!x->is_leaf) {
        int i = 0;
        while (i < x->n && key >= x->keys[i]) i++;
        x = x->u.children[i];
    }
    for (int i = 0; i < x->n; i++) {
        if (x->keys[i] == key) {
            if (value_out) *value_out = x->u.values[i];
            return true;
        }
    }
    return false;
}

/* Find the leaf where `key` belongs (whether present or not), recording
 * the descent path so callers that need to walk back up (insert/delete)
 * can do so without a parent pointer on every node. path[0] is the root,
 * path[path_len-1] is the leaf; child_idx[i] is which child of path[i]
 * leads to path[i+1] (unused for the leaf itself). */
/* Why 64 is enough, stated explicitly rather than left as folklore: the
 * minimum fanout is t >= 2, so a tree holding N keys has depth <= log2(N).
 * Keys are `int`, so N <= 2^32 distinct keys and depth <= 32 < 64. The
 * assert below is therefore unreachable in practice -- it exists so that
 * changing the key type (or the fanout) fails loudly instead of silently
 * writing past the end of these arrays. */
#define DESCENT_PATH_MAX 64

typedef struct {
    BPlusNode *path[DESCENT_PATH_MAX];
    int child_idx[DESCENT_PATH_MAX];
    int len;
} DescentPath;

static void find_leaf_path(BPlusNode *root, int key, DescentPath *dp) {
    dp->len = 0;
    BPlusNode *x = root;
    for (;;) {
        assert(dp->len < DESCENT_PATH_MAX);
        dp->path[dp->len] = x;
        if (x->is_leaf) { dp->len++; return; }
        int i = 0;
        while (i < x->n && key >= x->keys[i]) i++;
        dp->child_idx[dp->len] = i;
        dp->len++;
        x = x->u.children[i];
    }
}

/* ---- Insertion ---- */

/* Split full leaf y (y->n == 2t-1) into y (left half) and a brand new
 * right sibling z. The middle key is COPIED (not moved) up into
 * *sep_key_out, because it still has to go on living in y or z as real
 * data -- only a routing copy travels to the parent. z is spliced into
 * the leaf chain immediately after y. */
static BPlusNode *split_leaf(BPlusNode *y, int t, int *sep_key_out) {
    BPlusNode *z = node_create(t, true);
    int left_n = t;       /* y keeps the smaller half, INCLUDING the middle key */
    int right_n = t - 1;  /* z gets the larger half */

    for (int j = 0; j < right_n; j++) {
        z->keys[j] = y->keys[left_n + j];
        z->u.values[j] = y->u.values[left_n + j];
    }
    z->n = right_n;
    y->n = left_n;

    z->next = y->next;
    y->next = z;

    *sep_key_out = z->keys[0]; /* copy: z->keys[0] stays real data in z */
    return z;
}

/* Split full internal node y (y->n == 2t-1 routing keys, 2t children)
 * into y (left) and new sibling z (right). The middle routing key is
 * MOVED (not copied) up to the parent -- an internal node never holds
 * real data, so there is no reason to leave a copy behind. */
static BPlusNode *split_internal(BPlusNode *y, int t, int *sep_key_out) {
    BPlusNode *z = node_create(t, false);

    z->n = t - 1;
    for (int j = 0; j < t - 1; j++) z->keys[j] = y->keys[j + t];
    for (int j = 0; j < t; j++) z->u.children[j] = y->u.children[j + t];

    *sep_key_out = y->keys[t - 1]; /* moved: removed from y, not duplicated */
    y->n = t - 1;
    return z;
}

/* Insert (routing_key, right_child) into internal node x at position i,
 * assuming x is guaranteed non-full (caller's responsibility). */
static void internal_insert_at(BPlusNode *x, int i, int routing_key, BPlusNode *right_child) {
    for (int j = x->n - 1; j >= i; j--) x->keys[j + 1] = x->keys[j];
    for (int j = x->n; j >= i + 1; j--) x->u.children[j + 1] = x->u.children[j];
    x->keys[i] = routing_key;
    x->u.children[i + 1] = right_child;
    x->n++;
}

bool bplustree_insert(BPlusTree *tree, int key, int value) {
    int t = tree->t;

    if (!tree->root) {
        tree->root = node_create(t, true);
        tree->root->keys[0] = key;
        tree->root->u.values[0] = value;
        tree->root->n = 1;
        return true;
    }

    DescentPath dp;
    find_leaf_path(tree->root, key, &dp);
    BPlusNode *leaf = dp.path[dp.len - 1];

    int i = 0;
    while (i < leaf->n && key > leaf->keys[i]) i++;
    if (i < leaf->n && leaf->keys[i] == key) return false; /* duplicate */

    /* Leaf has room: plain shift-and-insert, no split needed. */
    if (leaf->n < 2 * t - 1) {
        for (int j = leaf->n - 1; j >= i; j--) {
            leaf->keys[j + 1] = leaf->keys[j];
            leaf->u.values[j + 1] = leaf->u.values[j];
        }
        leaf->keys[i] = key;
        leaf->u.values[i] = value;
        leaf->n++;
        return true;
    }

    /* Leaf is full (2t-1 keys already). Insert would make it 2t, which
     * exceeds capacity -- so split FIRST, then insert into whichever
     * half the key belongs in. This mirrors the B-tree's "split before
     * descending" style, just applied at the leaf itself. */
    int sep_key;
    BPlusNode *new_leaf = split_leaf(leaf, t, &sep_key);
    BPlusNode *target = (key < sep_key) ? leaf : new_leaf;

    int j = 0;
    while (j < target->n && key > target->keys[j]) j++;
    for (int k = target->n - 1; k >= j; k--) {
        target->keys[k + 1] = target->keys[k];
        target->u.values[k + 1] = target->u.values[k];
    }
    target->keys[j] = key;
    target->u.values[j] = value;
    target->n++;

    /* Propagate the new (sep_key, new_leaf) pair up the recorded path. */
    BPlusNode *right_child = new_leaf;
    for (int level = dp.len - 2; level >= 0; level--) {
        BPlusNode *parent = dp.path[level];
        int idx = dp.child_idx[level]; /* position of the child we just came from */

        if (parent->n < 2 * t - 1) {
            internal_insert_at(parent, idx, sep_key, right_child);
            return true;
        }

        /* Parent is also full: split it, then decide which half gets
         * the new (sep_key, right_child) pair before inserting.
         *
         * BUG (see README debug notes): split_internal() mutates parent->n
         * to t-1 as a side effect, so reading parent->n AFTER that call
         * yields the post-split (shrunk) count, not the pre-split child
         * count `idx` was computed against. Using it broke the routing in
         * two separate ways, for every idx in [t-1, 2t-1]:
         *   - idx == t-1     : `idx < parent->n` is false, so the child
         *                      was sent to the RIGHT half; it belongs left.
         *   - idx >= t       : the offset `idx - parent->n` == idx-(t-1)
         *                      is one too large; correct is idx - t.
         * Must snapshot the pre-split child boundary (t children survive
         * in the left half, indices 0..t-1) before calling split_internal. */
        int left_children_before_split = t;
        int parent_sep;
        BPlusNode *new_parent = split_internal(parent, t, &parent_sep);

        if (idx < left_children_before_split) {
            internal_insert_at(parent, idx, sep_key, right_child);
        } else {
            internal_insert_at(new_parent, idx - left_children_before_split, sep_key, right_child);
        }

        sep_key = parent_sep;
        right_child = new_parent;
        /* loop continues: this (sep_key, right_child) now propagates to
         * parent's parent, i.e. dp.path[level-1] */
    }

    /* Fell off the top of the path: the old root itself split. Grow the
     * tree by one level with a fresh internal root. */
    BPlusNode *new_root = node_create(t, false);
    new_root->n = 1;
    new_root->keys[0] = sep_key;
    new_root->u.children[0] = tree->root;
    new_root->u.children[1] = right_child;
    tree->root = new_root;
    return true;
}

/* ---- Deletion ---- */

static int min_leaf_keys(int t) { return t - 1; }
static int min_internal_keys(int t) { return t - 1; } /* i.e. min t children */

/* Borrow the rightmost key from left sibling into `child` (a leaf),
 * through parent `x` at index i (child is x->u.children[i]). Updates
 * the routing key x->keys[i-1] to the new smallest key of `child`. */
static void leaf_borrow_left(BPlusNode *x, int i) {
    BPlusNode *child = x->u.children[i];
    BPlusNode *left = x->u.children[i - 1];

    for (int j = child->n - 1; j >= 0; j--) {
        child->keys[j + 1] = child->keys[j];
        child->u.values[j + 1] = child->u.values[j];
    }
    child->keys[0] = left->keys[left->n - 1];
    child->u.values[0] = left->u.values[left->n - 1];
    child->n++;
    left->n--;

    x->keys[i - 1] = child->keys[0]; /* routing key = new min of child, a copy */
}

static void leaf_borrow_right(BPlusNode *x, int i) {
    BPlusNode *child = x->u.children[i];
    BPlusNode *right = x->u.children[i + 1];

    child->keys[child->n] = right->keys[0];
    child->u.values[child->n] = right->u.values[0];
    child->n++;

    for (int j = 0; j < right->n - 1; j++) {
        right->keys[j] = right->keys[j + 1];
        right->u.values[j] = right->u.values[j + 1];
    }
    right->n--;

    x->keys[i] = right->keys[0]; /* routing key = new min of right, a copy */
}

/* Merge leaf x->u.children[i] and x->u.children[i+1] into the left one.
 * Fixes the leaf chain's `next` pointer and frees the right node. The
 * routing key x->keys[i] that separated them is dropped entirely (it
 * was only ever a copy, so nothing needs to migrate into the merged
 * leaf -- unlike a B-tree merge, where the separator is real data that
 * must be pulled down). Returns via out param whether the merge target
 * was the left (i) or effectively still at index i. */
static void leaf_merge(BPlusNode *x, int i) {
    BPlusNode *left = x->u.children[i];
    BPlusNode *right = x->u.children[i + 1];

    for (int j = 0; j < right->n; j++) {
        left->keys[left->n + j] = right->keys[j];
        left->u.values[left->n + j] = right->u.values[j];
    }
    left->n += right->n;
    left->next = right->next;

    for (int j = i; j < x->n - 1; j++) x->keys[j] = x->keys[j + 1];
    for (int j = i + 1; j < x->n; j++) x->u.children[j] = x->u.children[j + 1];
    x->n--;

    node_free(right);
}

static void internal_borrow_left(BPlusNode *x, int i) {
    BPlusNode *child = x->u.children[i];
    BPlusNode *left = x->u.children[i - 1];

    for (int j = child->n - 1; j >= 0; j--) child->keys[j + 1] = child->keys[j];
    for (int j = child->n; j >= 0; j--) child->u.children[j + 1] = child->u.children[j];
    child->keys[0] = x->keys[i - 1];
    child->u.children[0] = left->u.children[left->n];
    child->n++;

    x->keys[i - 1] = left->keys[left->n - 1];
    left->n--;
}

static void internal_borrow_right(BPlusNode *x, int i) {
    BPlusNode *child = x->u.children[i];
    BPlusNode *right = x->u.children[i + 1];

    child->keys[child->n] = x->keys[i];
    child->u.children[child->n + 1] = right->u.children[0];
    child->n++;

    x->keys[i] = right->keys[0];
    for (int j = 0; j < right->n - 1; j++) right->keys[j] = right->keys[j + 1];
    for (int j = 0; j <= right->n - 1; j++) right->u.children[j] = right->u.children[j + 1];
    right->n--;
}

/* Merge internal x->u.children[i] and x->u.children[i+1], pulling down
 * the separator routing key x->keys[i] -- this one DOES have to move
 * into the merged node, because it is the only thing that still
 * separates the two halves' worth of grandchildren. */
static void internal_merge(BPlusNode *x, int i) {
    BPlusNode *left = x->u.children[i];
    BPlusNode *right = x->u.children[i + 1];

    left->keys[left->n] = x->keys[i];
    for (int j = 0; j < right->n; j++) left->keys[left->n + 1 + j] = right->keys[j];
    for (int j = 0; j <= right->n; j++) left->u.children[left->n + 1 + j] = right->u.children[j];
    left->n = left->n + 1 + right->n;

    for (int j = i; j < x->n - 1; j++) x->keys[j] = x->keys[j + 1];
    for (int j = i + 1; j < x->n; j++) x->u.children[j] = x->u.children[j + 1];
    x->n--;

    node_free(right);
}

/* After a leaf merge/borrow changes child i's minimum key, any ancestor
 * routing key that used to equal the OLD minimum and pointed at this
 * subtree needs no fix-up in this design: routing keys are only ever
 * set to "the current minimum of the right subtree" at the moment of a
 * split/borrow, and merges always drop the routing key that pointed at
 * the removed boundary. So no separate repair pass is needed -- but we
 * do need to repair x->keys[i-1] after leaf_borrow_left/right change
 * child's first key, which those two functions already do inline. */

bool bplustree_delete(BPlusTree *tree, int key) {
    int t = tree->t;
    if (!tree->root) return false;

    DescentPath dp;
    find_leaf_path(tree->root, key, &dp);
    BPlusNode *leaf = dp.path[dp.len - 1];

    int pos = -1;
    for (int j = 0; j < leaf->n; j++) if (leaf->keys[j] == key) { pos = j; break; }
    if (pos < 0) return false; /* not present */

    for (int j = pos; j < leaf->n - 1; j++) {
        leaf->keys[j] = leaf->keys[j + 1];
        leaf->u.values[j] = leaf->u.values[j + 1];
    }
    leaf->n--;

    if (dp.len == 1) {
        /* root is the only leaf: no borrow/merge rule applies to it, but
         * it can still empty out completely (the last key in the whole
         * tree was just deleted) -- collapse to the canonical empty-tree
         * state (root == NULL) instead of leaving a zombie n=0 leaf. */
        if (leaf->n == 0) {
            node_free(leaf);
            tree->root = NULL;
        }
        return true;
    }

    BPlusNode *child = leaf;
    for (int level = dp.len - 2; level >= 0; level--) {
        BPlusNode *parent = dp.path[level];
        int idx = dp.child_idx[level];
        bool child_is_leaf = child->is_leaf;
        int min_keys = child_is_leaf ? min_leaf_keys(t) : min_internal_keys(t);

        if (child->n >= min_keys) return true; /* no underflow, done */

        bool has_left = idx > 0;
        bool has_right = idx < parent->n;
        BPlusNode *left_sib = has_left ? parent->u.children[idx - 1] : NULL;
        BPlusNode *right_sib = has_right ? parent->u.children[idx + 1] : NULL;
        int sib_min = child_is_leaf ? min_leaf_keys(t) : min_internal_keys(t);

        if (child_is_leaf) {
            if (has_left && left_sib->n > sib_min) {
                leaf_borrow_left(parent, idx);
                return true;
            }
            if (has_right && right_sib->n > sib_min) {
                leaf_borrow_right(parent, idx);
                return true;
            }
            if (has_right) leaf_merge(parent, idx);
            else leaf_merge(parent, idx - 1);
        } else {
            if (has_left && left_sib->n > sib_min) {
                internal_borrow_left(parent, idx);
                return true;
            }
            if (has_right && right_sib->n > sib_min) {
                internal_borrow_right(parent, idx);
                return true;
            }
            if (has_right) internal_merge(parent, idx);
            else internal_merge(parent, idx - 1);
        }
        /* merge happened: parent has one fewer key/child now. Continue
         * the loop to check whether `parent` itself underflowed. */
        child = parent;
    }

    /* Reached the (former) root. If it's internal and dropped to 0
     * routing keys, it now has exactly one child -- collapse it away. */
    if (!tree->root->is_leaf && tree->root->n == 0) {
        BPlusNode *old_root = tree->root;
        tree->root = old_root->u.children[0];
        node_free(old_root);
    }
    return true;
}

/* ---- Range query ---- */

int bplustree_range_query(const BPlusTree *tree, int low, int high,
                           int *keys_out, int *values_out, int cap,
                           int *nodes_visited_out) {
    int written = 0;
    int visited = 0;
    if (nodes_visited_out) *nodes_visited_out = 0;
    if (!tree->root || low > high) return 0;

    const BPlusNode *x = tree->root;
    while (!x->is_leaf) {
        int i = 0;
        while (i < x->n && low >= x->keys[i]) i++;
        x = x->u.children[i];
    }

    const BPlusNode *leaf = x;
    while (leaf) {
        visited++;
        for (int j = 0; j < leaf->n; j++) {
            if (leaf->keys[j] < low) continue;
            if (leaf->keys[j] > high) {
                if (nodes_visited_out) *nodes_visited_out = visited;
                return written;
            }
            /* Buffer full: stop collecting. The contract in bplustree.h is
             * "returns the number of pairs written (never more than cap)",
             * so the counter must NOT keep incrementing past the buffer --
             * a caller looping `for (i = 0; i < ret; i++)` would then read
             * off the end of its own array. */
            if (written >= cap) {
                if (nodes_visited_out) *nodes_visited_out = visited;
                return written;
            }
            keys_out[written] = leaf->keys[j];
            values_out[written] = leaf->u.values[j];
            written++;
        }
        leaf = leaf->next;
    }
    if (nodes_visited_out) *nodes_visited_out = visited;
    return written;
}

/* ---- Stats ---- */

static int count_keys_rec(const BPlusNode *x) {
    if (!x) return 0;
    if (x->is_leaf) return x->n;
    int total = 0;
    for (int i = 0; i <= x->n; i++) total += count_keys_rec(x->u.children[i]);
    return total;
}

int bplustree_count_keys(const BPlusTree *tree) { return count_keys_rec(tree->root); }

static int count_nodes_rec(const BPlusNode *x) {
    if (!x) return 0;
    int total = 1;
    if (!x->is_leaf) for (int i = 0; i <= x->n; i++) total += count_nodes_rec(x->u.children[i]);
    return total;
}

int bplustree_count_nodes(const BPlusTree *tree) { return count_nodes_rec(tree->root); }

int bplustree_count_leaves(const BPlusTree *tree) {
    if (!tree->root) return 0;
    int count = 0;
    const BPlusNode *x = tree->root;
    while (!x->is_leaf) x = x->u.children[0];
    while (x) { count++; x = x->next; }
    return count;
}

int bplustree_height(const BPlusTree *tree) {
    int h = -1;
    const BPlusNode *x = tree->root;
    while (x) {
        h++;
        if (x->is_leaf) break;
        x = x->u.children[0];
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

/* Bound semantics differ from a B-tree on purpose: a B+-tree routing key
 * is a COPY of the smallest key in its right subtree (see split_leaf),
 * so the right subtree's minimum key must be allowed to EQUAL the
 * separator, not just exceed it. min_key is therefore an inclusive lower
 * bound (subtree keys must be >= *min_key), while max_key stays an
 * exclusive upper bound (subtree keys must be < *max_key) -- the same
 * key never appears as both "someone's inclusive min" and "someone's
 * exclusive max" at once because a node's own keys sit strictly between
 * its neighbouring routing keys at the parent's level. */
static void verify_rec(const BPlusNode *x, int t, bool is_root, int depth,
                        const int *min_key, const int *max_key,
                        int *leaf_depth_out, int *leaf_count_out,
                        VerifyCtx *ctx) {
    if (ctx->failed) return;

    int max_keys = 2 * t - 1;
    int min_keys;
    if (x->is_leaf) min_keys = is_root ? 0 : (t - 1);
    else min_keys = is_root ? 1 : (t - 1);

    if (x->n > max_keys) { fail(ctx, "node has %d keys, max is %d", x->n, max_keys); return; }
    if (x->n < min_keys) { fail(ctx, "node has %d keys, min is %d (is_root=%d, is_leaf=%d)", x->n, min_keys, is_root, x->is_leaf); return; }

    for (int i = 0; i < x->n - 1; i++) {
        if (x->keys[i] >= x->keys[i + 1]) {
            fail(ctx, "keys not strictly increasing at index %d: %d >= %d", i, x->keys[i], x->keys[i + 1]);
            return;
        }
    }
    if (min_key && x->n > 0 && x->keys[0] < *min_key) {
        fail(ctx, "smallest key %d does not respect inclusive lower bound %d", x->keys[0], *min_key);
        return;
    }
    if (max_key && x->n > 0 && x->keys[x->n - 1] >= *max_key) {
        fail(ctx, "largest key %d does not respect exclusive upper bound %d", x->keys[x->n - 1], *max_key);
        return;
    }

    if (x->is_leaf) {
        if (*leaf_depth_out == -1) *leaf_depth_out = depth;
        else if (*leaf_depth_out != depth) {
            fail(ctx, "leaf depth mismatch: saw %d and %d", *leaf_depth_out, depth);
        }
        (*leaf_count_out)++;
        return;
    }

    for (int i = 0; i <= x->n; i++) {
        if (!x->u.children[i]) { fail(ctx, "internal node missing child pointer at index %d", i); return; }
        /* children[i] holds keys in [keys[i-1], keys[i]) -- inclusive on
         * the left (routing key IS the right subtree's minimum), strict
         * on the right (that same key is the NEXT child's inclusive
         * minimum, so this child must stay strictly below it). */
        const int *lo = (i == 0) ? min_key : &x->keys[i - 1];
        const int *hi = (i == x->n) ? max_key : &x->keys[i];
        verify_rec(x->u.children[i], t, false, depth + 1, lo, hi, leaf_depth_out, leaf_count_out, ctx);
        if (ctx->failed) return;
    }
}

bool bplustree_verify(const BPlusTree *tree, char *err_buf, size_t err_buf_size) {
    if (err_buf && err_buf_size > 0) err_buf[0] = '\0';
    if (!tree->root) return true;

    VerifyCtx ctx = { err_buf, err_buf_size, false };
    int leaf_depth = -1;
    int leaf_count_via_tree = 0;
    verify_rec(tree->root, tree->t, true, 0, NULL, NULL, &leaf_depth, &leaf_count_via_tree, &ctx);
    if (ctx.failed) return false;

    /* Leaf-chain integrity: walk `next` from the leftmost leaf and cross
     * check against what tree recursion saw. This is the property a
     * B-tree has no equivalent of. */
    const BPlusNode *x = tree->root;
    while (!x->is_leaf) x = x->u.children[0];

    int chain_leaf_count = 0;
    int chain_key_count = 0;
    int prev_key;
    bool have_prev = false;
    for (const BPlusNode *leaf = x; leaf; leaf = leaf->next) {
        chain_leaf_count++;
        for (int i = 0; i < leaf->n; i++) {
            if (have_prev && leaf->keys[i] <= prev_key) {
                fail(&ctx, "leaf chain not strictly increasing: %d after %d", leaf->keys[i], prev_key);
                return false;
            }
            prev_key = leaf->keys[i];
            have_prev = true;
            chain_key_count++;
        }
    }

    if (chain_leaf_count != leaf_count_via_tree) {
        fail(&ctx, "leaf chain visits %d leaves, tree recursion saw %d", chain_leaf_count, leaf_count_via_tree);
        return false;
    }
    int total_keys_via_tree = count_keys_rec(tree->root);
    if (chain_key_count != total_keys_via_tree) {
        fail(&ctx, "leaf chain holds %d keys, tree recursion counted %d", chain_key_count, total_keys_via_tree);
        return false;
    }

    return true;
}

/* ---- Printing ---- */

static void print_rec(const BPlusNode *x, int depth) {
    if (!x) return;
    for (int i = 0; i < depth; i++) printf("  ");
    if (x->is_leaf) {
        printf("[");
        for (int i = 0; i < x->n; i++) printf("%d:%d%s", x->keys[i], x->u.values[i], i + 1 < x->n ? " " : "");
        printf("] (leaf)\n");
    } else {
        printf("[");
        for (int i = 0; i < x->n; i++) printf("%d%s", x->keys[i], i + 1 < x->n ? " " : "");
        printf("]\n");
        for (int i = 0; i <= x->n; i++) print_rec(x->u.children[i], depth + 1);
    }
}

void bplustree_print(const BPlusTree *tree) {
    if (!tree->root) { printf("  (empty tree)\n"); return; }
    print_rec(tree->root, 0);
}

void bplustree_print_leaf_chain(const BPlusTree *tree) {
    if (!tree->root) { printf("  (empty tree, no leaves)\n"); return; }
    const BPlusNode *x = tree->root;
    while (!x->is_leaf) x = x->u.children[0];

    printf("  leaf chain: ");
    for (const BPlusNode *leaf = x; leaf; leaf = leaf->next) {
        printf("[");
        for (int i = 0; i < leaf->n; i++) printf("%d%s", leaf->keys[i], i + 1 < leaf->n ? "," : "");
        printf("]");
        if (leaf->next) printf(" -> ");
    }
    printf(" -> NULL\n");
}
