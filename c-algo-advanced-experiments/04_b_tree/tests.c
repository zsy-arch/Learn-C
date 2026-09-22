/* B-tree test suite. See README.md for the property definitions this
 * checks. Every insert/delete in this file is followed by btree_verify()
 * so a test that only checks btree_search() results could never hide a
 * structural bug (wrong key counts, unsorted keys, uneven leaf depth,
 * dangling child pointers). */
#include "btree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        printf("  FAILED CHECK: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        return false; \
    } \
} while (0)

static bool verify_ok(BTree *t, const char *ctx) {
    char err[256];
    if (!btree_verify(t, err, sizeof err)) {
        printf("  btree_verify failed after %s: %s\n", ctx, err);
        return false;
    }
    return true;
}

#define RUN(name) do { \
    bool ok = (name)(); \
    printf("[%s] %s\n", ok ? "PASS" : "FAIL", #name); \
    if (ok) g_pass++; else g_fail++; \
} while (0)

/* ---------- basic shapes ---------- */

static bool test_empty_tree_search_and_delete(void) {
    BTree t = btree_create(3);
    CHECK(!btree_search(&t, 5));
    CHECK(!btree_delete(&t, 5));
    CHECK(verify_ok(&t, "empty"));
    CHECK(btree_height(&t) == -1);
    btree_destroy(&t);
    return true;
}

static bool test_single_key_insert_delete(void) {
    BTree t = btree_create(2);
    CHECK(btree_insert(&t, 42));
    CHECK(verify_ok(&t, "insert 42"));
    CHECK(btree_search(&t, 42));
    CHECK(!btree_search(&t, 43));
    CHECK(btree_height(&t) == 0);
    CHECK(btree_delete(&t, 42));
    CHECK(verify_ok(&t, "delete 42"));
    CHECK(!btree_search(&t, 42));
    CHECK(t.root == NULL);
    btree_destroy(&t);
    return true;
}

static bool test_duplicate_insert_rejected(void) {
    BTree t = btree_create(2);
    CHECK(btree_insert(&t, 7));
    CHECK(!btree_insert(&t, 7)); /* second insert of same key must fail */
    CHECK(btree_count_keys(&t) == 1);
    CHECK(verify_ok(&t, "duplicate insert"));
    btree_destroy(&t);
    return true;
}

static bool test_delete_nonexistent_is_noop(void) {
    BTree t = btree_create(2);
    int keys[] = {10, 20, 30, 40, 50};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) CHECK(btree_insert(&t, keys[i]));
    int before = btree_count_keys(&t);
    CHECK(!btree_delete(&t, 999));
    CHECK(btree_count_keys(&t) == before);
    CHECK(verify_ok(&t, "delete nonexistent"));
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) CHECK(btree_search(&t, keys[i]));
    btree_destroy(&t);
    return true;
}

/* ---------- degree coverage: t=2,3,4 with sequential insert ---------- */

static bool insert_sequential_and_verify(int t_deg, int n) {
    BTree t = btree_create(t_deg);
    for (int i = 1; i <= n; i++) {
        CHECK(btree_insert(&t, i));
        CHECK(verify_ok(&t, "sequential insert"));
    }
    CHECK(btree_count_keys(&t) == n);
    for (int i = 1; i <= n; i++) CHECK(btree_search(&t, i));
    btree_destroy(&t);
    return true;
}

static bool test_degree_t2_sequential(void) { return insert_sequential_and_verify(2, 60); }
static bool test_degree_t3_sequential(void) { return insert_sequential_and_verify(3, 60); }
static bool test_degree_t4_sequential(void) { return insert_sequential_and_verify(4, 60); }

/* ---------- root split / root shrink ---------- */

static bool test_root_split_on_insert(void) {
    /* t=2: root holds at most 2t-1=3 keys. The 4th insert must split it. */
    BTree t = btree_create(2);
    CHECK(btree_insert(&t, 10));
    CHECK(btree_insert(&t, 20));
    CHECK(btree_insert(&t, 30));
    CHECK(t.root->is_leaf && t.root->n == 3);
    CHECK(btree_height(&t) == 0);

    CHECK(btree_insert(&t, 40)); /* triggers root split -> height grows to 1 */
    CHECK(verify_ok(&t, "root split"));
    CHECK(btree_height(&t) == 1);
    CHECK(!t.root->is_leaf);
    CHECK(t.root->n == 1); /* new root holds just the promoted middle key */
    for (int k = 10; k <= 40; k += 10) CHECK(btree_search(&t, k));
    btree_destroy(&t);
    return true;
}

static bool test_root_shrinks_after_merge(void) {
    /* t=2: build a height-1 tree with exactly 2 leaves under the root,
     * then delete down to a single key so the root (which held 1 key,
     * 2 children) is forced to merge its children and collapse. */
    BTree t = btree_create(2);
    int keys[] = {10, 20, 30, 40};
    for (size_t i = 0; i < 4; i++) CHECK(btree_insert(&t, keys[i]));
    CHECK(btree_height(&t) == 1);

    CHECK(btree_delete(&t, 10));
    CHECK(verify_ok(&t, "delete 10"));
    CHECK(btree_delete(&t, 40));
    CHECK(verify_ok(&t, "delete 40"));
    /* left leaf={20} right leaf={30}, root key=20 (or similar) with both
     * children now at t-1=1 key: next delete forces a merge into the root. */
    CHECK(btree_delete(&t, 20));
    CHECK(verify_ok(&t, "delete 20, root should collapse"));
    CHECK(btree_height(&t) == 0); /* root shrank back down to a single leaf */
    CHECK(btree_search(&t, 30));
    CHECK(btree_count_nodes(&t) == 1);

    CHECK(btree_delete(&t, 30));
    CHECK(verify_ok(&t, "delete last key"));
    CHECK(t.root == NULL);
    btree_destroy(&t);
    return true;
}

/* ---------- deletion case coverage: 2a/2b/2c and 3a/3b/3c ---------- */

/* Build a t=2 tree shaped so deleting the root's single separator key
 * exercises case 2a (steal predecessor). Traced with btree_print():
 * inserting 20,10,30,5,15 in this order produces root=[20], left
 * child=[5,10,15] (n=3), right child=[30] (n=1). Deleting the root
 * key 20 looks at the left child first; it has n=3 >= t=2, so case 2a
 * fires: the predecessor (max of the left subtree = 15) replaces 20
 * in the root, then 15 is recursively deleted from the left subtree. */
static bool test_delete_internal_case2a_steal_predecessor(void) {
    BTree t = btree_create(2);
    int build[] = {20, 10, 30, 5, 15};
    for (size_t i = 0; i < 5; i++) CHECK(btree_insert(&t, build[i]));
    CHECK(verify_ok(&t, "build case2a tree"));
    CHECK(!t.root->is_leaf);
    CHECK(t.root->n == 1 && t.root->keys[0] == 20);
    CHECK(t.root->children[0]->n == 3); /* [5,10,15]: enough to donate */
    CHECK(t.root->children[1]->n == 1); /* [30]: at minimum, cannot donate */

    CHECK(btree_delete(&t, 20));
    CHECK(verify_ok(&t, "delete internal key via case 2a"));
    CHECK(!btree_search(&t, 20));
    CHECK(t.root->keys[0] == 15); /* predecessor promoted into the root */
    CHECK(btree_search(&t, 5) && btree_search(&t, 10) && btree_search(&t, 30));
    CHECK(btree_search(&t, 15)); /* 15 still present -- now living in the root, not the leaf */
    btree_destroy(&t);
    return true;
}

/* Mirror image of the case2a test above: left child is minimal (cannot
 * donate), right child has a key to spare. Traced with btree_print():
 * inserting 20,10,30,40,35 produces root=[20], left child=[10] (n=1),
 * right child=[30,35,40] (n=3). Deleting the root key 20 checks the
 * left child first (case 2a needs left->n>=t=2; here left->n=1, so 2a
 * does not apply), falls through to case 2b: the successor (min of the
 * right subtree = 30) replaces 20 in the root, then 30 is recursively
 * deleted from the right subtree. */
static bool test_delete_internal_case2b_steal_successor(void) {
    BTree t = btree_create(2);
    int build[] = {20, 10, 30, 40, 35};
    for (size_t i = 0; i < 5; i++) CHECK(btree_insert(&t, build[i]));
    CHECK(verify_ok(&t, "build case2b tree"));
    CHECK(!t.root->is_leaf);
    CHECK(t.root->n == 1 && t.root->keys[0] == 20);
    CHECK(t.root->children[0]->n == 1); /* [10]: at minimum, cannot donate */
    CHECK(t.root->children[1]->n == 3); /* [30,35,40]: enough to donate */

    CHECK(btree_delete(&t, 20));
    CHECK(verify_ok(&t, "delete internal key via case 2b"));
    CHECK(!btree_search(&t, 20));
    CHECK(t.root->keys[0] == 30); /* successor promoted into the root */
    CHECK(btree_search(&t, 10) && btree_search(&t, 35) && btree_search(&t, 40));
    CHECK(btree_search(&t, 30)); /* 30 still present -- now living in the root, not the leaf */
    btree_destroy(&t);
    return true;
}

/* Force case 2c: both children of the key-to-delete have exactly t-1
 * keys, so predecessor/successor subtrees must merge. */
static bool test_delete_internal_case2c_merge(void) {
    BTree t = btree_create(2); /* t-1 = 1 key minimum per non-root node */
    int build[] = {10, 20, 30};
    for (size_t i = 0; i < 3; i++) CHECK(btree_insert(&t, build[i]));
    /* root=[10,20,30], single leaf, n=3 (max for t=2). Not yet split. */
    CHECK(t.root->is_leaf && t.root->n == 3);
    CHECK(btree_insert(&t, 40)); /* splits: root=[20] children [10] [30,40] */
    CHECK(verify_ok(&t, "build case2c tree"));
    CHECK(!t.root->is_leaf);
    CHECK(t.root->n == 1);
    CHECK(t.root->keys[0] == 20);
    CHECK(t.root->children[0]->n == 1); /* [10]: exactly t-1 */
    CHECK(t.root->children[1]->n == 2); /* [30,40] */

    /* Delete 30 first so the right child also drops to t-1=1 key: [40] */
    CHECK(btree_delete(&t, 30));
    CHECK(verify_ok(&t, "delete 30"));
    CHECK(t.root->n == 1 && t.root->keys[0] == 20);
    CHECK(t.root->children[0]->n == 1 && t.root->children[1]->n == 1);

    /* Now both children of the root's key have exactly t-1 keys:
     * deleting 20 must hit case 2c (merge) rather than 2a/2b. */
    CHECK(btree_delete(&t, 20));
    CHECK(verify_ok(&t, "delete 20 via case 2c merge"));
    CHECK(!btree_search(&t, 20));
    CHECK(btree_search(&t, 10));
    CHECK(btree_search(&t, 40));
    CHECK(btree_height(&t) == 0); /* merge collapsed the root */
    btree_destroy(&t);
    return true;
}

/* Force case 3a (borrow from left sibling while descending) and
 * 3b (borrow from right sibling) by shaping siblings asymmetrically.
 *
 * Shape traced by hand with btree_print() before writing these asserts
 * (see README "调试笔记" — an earlier version of this test assumed a
 * tree shape that sequential 10..70 inserts do NOT actually produce). */
static bool test_delete_leaf_case3a_borrow_left(void) {
    BTree t = btree_create(2);
    int build[] = {10, 20, 30, 40};
    for (size_t i = 0; i < 4; i++) CHECK(btree_insert(&t, build[i]));
    CHECK(verify_ok(&t, "build case3a tree"));
    /* root=[20] children [10] [30,40] */
    CHECK(t.root->n == 1 && t.root->keys[0] == 20);
    CHECK(t.root->children[0]->n == 1);
    CHECK(t.root->children[1]->n == 2);

    CHECK(btree_delete(&t, 40)); /* right child -> [30], n=1=t-1 */
    CHECK(verify_ok(&t, "delete 40"));
    CHECK(btree_insert(&t, 5)); /* left child -> [5,10], n=2>=t, can lend */
    CHECK(verify_ok(&t, "insert 5"));
    CHECK(t.root->children[0]->n == 2);
    CHECK(t.root->children[1]->n == 1);

    /* Deleting the right child's only key forces a descent into a
     * t-1 node; only sibling is the left one, which has >= t keys:
     * case 3a (borrow through the root, no merge, height unchanged). */
    int height_before = btree_height(&t);
    CHECK(btree_delete(&t, 30));
    CHECK(verify_ok(&t, "delete 30 via case 3a borrow-left"));
    CHECK(btree_height(&t) == height_before); /* borrowing must not shrink the tree */
    CHECK(!btree_search(&t, 30));
    CHECK(btree_search(&t, 5) && btree_search(&t, 10) && btree_search(&t, 20));
    btree_destroy(&t);
    return true;
}

static bool test_delete_leaf_case3b_borrow_right(void) {
    /* root=[20] children [10] [30,40] (same starting shape as the 3a
     * test above). Deleting 10 means descending into children[0]=[10],
     * which only has t-1=1 key. Its right sibling [30,40] has n=2>=t,
     * so fill_child must take the 3b branch: the root's separator key
     * (20) moves down into children[0] (making it [10,20]), and the
     * right sibling's minimum key (30) moves up to replace it as the
     * new separator. Only THEN does the actual delete-10 happen inside
     * the now-2-key child, leaving it as [20]. Traced with btree_print()
     * before writing these asserts -- see README "调试笔记". */
    BTree t = btree_create(2);
    int build[] = {10, 20, 30, 40};
    for (size_t i = 0; i < 4; i++) CHECK(btree_insert(&t, build[i]));
    CHECK(t.root->n == 1 && t.root->keys[0] == 20);
    CHECK(t.root->children[0]->n == 1 && t.root->children[0]->keys[0] == 10);
    CHECK(t.root->children[1]->n == 2); /* [30,40], has a key to lend */

    int height_before = btree_height(&t);
    CHECK(btree_delete(&t, 10));
    CHECK(verify_ok(&t, "delete 10 via case 3b borrow-right"));
    CHECK(btree_height(&t) == height_before); /* borrowing must not shrink the tree */
    CHECK(!btree_search(&t, 10));
    CHECK(t.root->keys[0] == 30); /* separator pulled up from the right sibling */
    CHECK(btree_search(&t, 20) && btree_search(&t, 30) && btree_search(&t, 40));
    btree_destroy(&t);
    return true;
}

/* Force case 3c (merge while descending, not at the root) using a
 * taller tree so the merge happens below the top level. */
static bool test_delete_case3c_merge_while_descending(void) {
    BTree t = btree_create(2);
    for (int i = 1; i <= 20; i++) CHECK(btree_insert(&t, i));
    CHECK(verify_ok(&t, "build 1..20"));
    /* Delete a long scrambled sequence; with t=2 minimum-degree this
     * reliably drives multiple 3c merges below the root before things
     * shrink back down. Verified after every single delete. */
    int order[] = {6, 7, 8, 9, 5, 4, 3, 2, 1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
    for (size_t i = 0; i < 20; i++) {
        CHECK(btree_delete(&t, order[i]));
        CHECK(verify_ok(&t, "descending merge sequence"));
        CHECK(!btree_search(&t, order[i]));
    }
    CHECK(t.root == NULL);
    btree_destroy(&t);
    return true;
}

/* ---------- 回归测试：守住三个已修的缺陷 ----------
 *
 * 这一节和上面的用例性质不同。上面测的是"B 树该有的行为"，这一节测的是
 * "曾经错过的地方不许再错"。三个缺陷有一个共同点，也正是它们能一直藏着
 * 的原因：**btree_verify 全都查不出来**。
 *
 *   - 插重复键时先分裂：树被改了结构，但改完仍是一棵合法 B 树。
 *     两条路径（root 满 / 下降路径上的孩子满）都会中招，而且第一版修复
 *     只补了前者——后者由 test_dup_insert_into_full_child_is_noop 守。
 *   - 非叶 root 只剩 0 个 key：树还能正常 search，只是永久多背一层高度。
 *   - btree_create(1)：建出来的东西根本不是 B 树，但每一步都"合法"。
 *
 * 所以这些用例都不依赖 verify_ok 报错，而是各自去检查一个 verify 管不到
 * 的东西：结构指纹、min_keys 的判定本身、以及进程退出码。 */

/* 把整棵树序列化成 "[k k|<孩子><孩子>]" 形式的结构指纹。
 *
 * 为什么不直接比 height/count_nodes/count_keys 就算了？因为那三个数字是
 * 聚合量，可能一起骗人：一次分裂加一次合并就能让节点数回到原值。指纹把
 * 每个节点在第几层、装了哪些 key 全都写进字符串，任何结构变动都会让它
 * 不等——这正是"不做任何修改"这条契约需要的粒度。 */
static void shape_rec(const BTreeNode *x, char *buf, size_t cap, size_t *len) {
    if (*len + 1 >= cap) return;
    *len += (size_t)snprintf(buf + *len, cap - *len, "[");
    for (int i = 0; i < x->n && *len < cap; i++) {
        *len += (size_t)snprintf(buf + *len, cap - *len, "%s%d", i ? " " : "", x->keys[i]);
    }
    if (!x->is_leaf) {
        if (*len < cap) *len += (size_t)snprintf(buf + *len, cap - *len, "|");
        for (int i = 0; i <= x->n && *len < cap; i++) shape_rec(x->children[i], buf, cap, len);
    }
    if (*len < cap) *len += (size_t)snprintf(buf + *len, cap - *len, "]");
}

static void shape_of(const BTree *t, char *buf, size_t cap) {
    size_t len = 0;
    buf[0] = '\0';
    if (!t->root) { snprintf(buf, cap, "(empty)"); return; }
    shape_rec(t->root, buf, cap, &len);
}

/* 收集所有 key（中序），用来遍历"每一个已存在的 key 都试一次重复插入"。 */
static void collect_keys_rec(const BTreeNode *x, int *out, int *cnt) {
    if (x->is_leaf) {
        for (int i = 0; i < x->n; i++) out[(*cnt)++] = x->keys[i];
        return;
    }
    for (int i = 0; i < x->n; i++) {
        collect_keys_rec(x->children[i], out, cnt);
        out[(*cnt)++] = x->keys[i];
    }
    collect_keys_rec(x->children[x->n], out, cnt);
}

/* 核心断言：往 tree 里插一个已存在的 key，必须返回 false **且结构指纹
 * 一字不变**。缺陷版本会在这里露馅——返回值是对的，指纹不是。 */
static bool dup_insert_leaves_tree_untouched(BTree *t, int dup) {
    char before[1024], after[1024];
    shape_of(t, before, sizeof before);
    int h0 = btree_height(t), nodes0 = btree_count_nodes(t), keys0 = btree_count_keys(t);

    CHECK(!btree_insert(t, dup)); /* 契约第一半：返回 false */

    shape_of(t, after, sizeof after);
    if (strcmp(before, after) != 0) {
        printf("  重复插入 %d 改动了结构:\n    before: %s\n    after:  %s\n", dup, before, after);
        return false;
    }
    CHECK(btree_height(t) == h0);
    CHECK(btree_count_nodes(t) == nodes0);
    CHECK(btree_count_keys(t) == keys0);
    CHECK(verify_ok(t, "duplicate insert into full root"));
    CHECK(btree_search(t, dup)); /* 原来那个 key 还在 */
    return true;
}

/* root 是满的**叶子**时插重复键。t=2 下 3 个 key 就把 root 填满了。
 * 缺陷版本在这里会把节点数从 1 变成 3、树高从 0 变成 1。 */
static bool test_dup_insert_into_full_leaf_root_is_noop(void) {
    int seed[] = {10, 20, 30};
    for (size_t d = 0; d < 3; d++) {
        BTree t = btree_create(2);
        for (size_t i = 0; i < 3; i++) CHECK(btree_insert(&t, seed[i]));
        CHECK(t.root->is_leaf && t.root->n == 3); /* 满了：2t-1 = 3 */

        if (!dup_insert_leaves_tree_untouched(&t, seed[d])) { btree_destroy(&t); return false; }
        btree_destroy(&t);
    }
    return true;
}

/* root 是满的**内部节点**时插重复键——比叶子那一版更接近真实场景，因为
 * 此时分裂会真的长高一层。t=2 顺序插 1..8 正好得到 root=[2 4 6]（满）、
 * 高度 1、5 个节点（用 btree_print 实际跑出来的，不是推的）。
 * 树里每一个 key 都试一遍：叶子里的、root 里的、中间层的。 */
static bool test_dup_insert_into_full_internal_root_is_noop(void) {
    BTree t = btree_create(2);
    for (int i = 1; i <= 8; i++) CHECK(btree_insert(&t, i));
    CHECK(verify_ok(&t, "build full-internal-root tree"));
    CHECK(!t.root->is_leaf);
    CHECK(t.root->n == 3); /* 满了 */
    CHECK(btree_height(&t) == 1);
    CHECK(btree_count_nodes(&t) == 5);

    int keys[8];
    int cnt = 0;
    collect_keys_rec(t.root, keys, &cnt);
    CHECK(cnt == 8);
    for (int i = 0; i < cnt; i++) {
        if (!dup_insert_leaves_tree_untouched(&t, keys[i])) { btree_destroy(&t); return false; }
    }
    btree_destroy(&t);
    return true;
}

/* root **不满**、但下降路径上的孩子满了——上面两个用例覆盖不到的那一半。
 *
 * 为什么必须单独写：上面三个 test_dup_insert_* 构造的树 root 全是满的，
 * 于是每次都被 btree_insert 开头那次查找挡住，`insert_nonfull` 里
 * "先 split_child、再比较被提上去的 key" 这条路径一次都没走到。第一版
 * 修复只在 root 满的分支前面加了查找，这条路径漏了整整一轮回归测试。
 *
 * t=2 下顺序插 10,20,30,40,50 得到 root=[20]（n=1，不满）、孩子 [10] 和
 * [30 40 50]（n=3=2t-1，满）。往里插已存在的 30/40/50 都会先触发
 * children[1] 的分裂（节点数 3 -> 4，root 变 [20 40]），然后才发现重复。
 * 三个 key 分别对应"分裂后落在左半/正好是被提上去的中间键/落在右半"
 * 三条子路径，都得试。形状是 btree_print 实际跑出来的，不是推的。 */
static bool test_dup_insert_into_full_child_is_noop(void) {
    int dups[] = {30, 40, 50}; /* 左半 / 中间键 / 右半 */
    for (size_t d = 0; d < sizeof(dups) / sizeof(dups[0]); d++) {
        BTree t = btree_create(2);
        int build[] = {10, 20, 30, 40, 50};
        for (size_t i = 0; i < sizeof(build) / sizeof(build[0]); i++) CHECK(btree_insert(&t, build[i]));
        CHECK(verify_ok(&t, "build full-child tree"));
        /* root 不满是这个用例的全部前提，钉住它 */
        CHECK(!t.root->is_leaf && t.root->n == 1);
        CHECK(t.root->n < 2 * t.t - 1);              /* root 不满 */
        CHECK(t.root->children[1]->n == 2 * t.t - 1); /* 但这个孩子满了 */
        CHECK(btree_count_nodes(&t) == 3);

        if (!dup_insert_leaves_tree_untouched(&t, dups[d])) { btree_destroy(&t); return false; }
        btree_destroy(&t);
    }

    /* t=3 再走一遍，确认不是 t=2 的巧合。顺序插 1..8 -> root=[3]（n=1<5，
     * 不满）、孩子 [1 2] 和 [4 5 6 7 8]（n=5=2t-1，满）。 */
    {
        BTree t = btree_create(3);
        for (int i = 1; i <= 8; i++) CHECK(btree_insert(&t, i));
        CHECK(!t.root->is_leaf && t.root->n == 1);
        CHECK(t.root->children[1]->n == 5);
        CHECK(btree_count_nodes(&t) == 3);
        int probes[] = {4, 5, 6, 7, 8}; /* 满孩子里的每一个 key 都撞一遍 */
        for (size_t i = 0; i < sizeof(probes) / sizeof(probes[0]); i++) {
            if (!dup_insert_leaves_tree_untouched(&t, probes[i])) { btree_destroy(&t); return false; }
        }
        btree_destroy(&t);
    }
    return true;
}

/* 同一个不变量在 t=3 上再走一遍，确认它不是 t=2 的巧合。
 * 顺序插 1..18 得到 root=[3 6 9 12 15]（n=5=2t-1，满）、高度 1、7 个节点。 */
static bool test_dup_insert_into_full_root_t3(void) {
    BTree t = btree_create(3);
    for (int i = 1; i <= 18; i++) CHECK(btree_insert(&t, i));
    CHECK(!t.root->is_leaf && t.root->n == 5);
    CHECK(btree_height(&t) == 1);
    CHECK(btree_count_nodes(&t) == 7);

    int probes[] = {1, 3, 9, 15, 17, 18}; /* 叶子首、root 首、root 中、root 末、叶子内、叶子末 */
    for (size_t i = 0; i < sizeof(probes) / sizeof(probes[0]); i++) {
        if (!dup_insert_leaves_tree_untouched(&t, probes[i])) { btree_destroy(&t); return false; }
    }
    btree_destroy(&t);
    return true;
}

/* 手工搭一个节点。node_create 是 btree.c 里的 static，这里只能照着它的
 * 分配方式重来一遍（keys 容量 2t-1、children 容量 2t），这样 btree_destroy
 * 才能原样 free 掉，ASan 不会有话说。malloc 失败的处理也跟着库里一致。 */
static BTreeNode *make_node(int t, bool is_leaf, const int *keys, int n) {
    BTreeNode *x = malloc(sizeof *x);
    if (!x) { perror("malloc"); exit(1); }
    x->keys = malloc(sizeof(int) * (size_t)(2 * t - 1));
    x->children = malloc(sizeof(BTreeNode *) * (size_t)(2 * t));
    if (!x->keys || !x->children) { perror("malloc"); exit(1); }
    x->n = n;
    x->is_leaf = is_leaf;
    for (int i = 0; i < n; i++) x->keys[i] = keys[i];
    return x;
}

/* 直接测 verify 里 min_keys 的判定：root 的 key 下限必须分叶/非叶两种。
 *
 * 缺陷版本写的是 `is_root ? 0 : t - 1`，不分叶子——于是"非叶 root 只剩
 * 0 个 key、挂着 1 个孩子"这种忘记收缩的形状能完整通过校验。这种树
 * search 起来一切正常，只是白白多背一层高度，而且这一层会在后续每次
 * 插入/删除里继续传播。
 *
 * 三个用例是一组，缺一不可：只有"非叶 0 key 必须失败"这一条的话，一个
 * 什么都拒绝的校验器也能通过；两个合法用例是用来钉住"失败是因为 n=0
 * 而不是因为它是内部节点"的。 */
static bool test_verify_root_min_keys_splits_leaf_and_internal(void) {
    char err[256];

    /* (a) 非叶 root，0 个 key，1 个孩子 -> 必须判失败 */
    {
        int leafkeys[] = {1};
        BTree t = btree_create(2);
        t.root = make_node(2, false, NULL, 0);
        t.root->children[0] = make_node(2, true, leafkeys, 1);

        bool ok = btree_verify(&t, err, sizeof err);
        if (ok) {
            printf("  非叶 root 只有 0 个 key，verify 却放过了（忘记收缩的树被判为合法）\n");
            btree_destroy(&t);
            return false;
        }
        /* 报错原因得说到点子上，不能是撞上别的检查顺便失败的 */
        if (!strstr(err, "min is 1")) {
            printf("  verify 失败了，但原因不对: \"%s\"（期望提到 min is 1）\n", err);
            btree_destroy(&t);
            return false;
        }
        btree_destroy(&t);
    }

    /* (b) 叶子 root，0 个 key -> 合法，这就是空树 */
    {
        BTree t = btree_create(2);
        t.root = make_node(2, true, NULL, 0);
        CHECK(btree_verify(&t, err, sizeof err));
        btree_destroy(&t);
    }

    /* (c) 非叶 root，1 个 key，两个最小孩子 -> 合法（下限就是 1，不是 t-1） */
    {
        int lk[] = {1}, rk[] = {3}, rootk[] = {2};
        BTree t = btree_create(2);
        t.root = make_node(2, false, rootk, 1);
        t.root->children[0] = make_node(2, true, lk, 1);
        t.root->children[1] = make_node(2, true, rk, 1);
        CHECK(btree_verify(&t, err, sizeof err));
        btree_destroy(&t);
    }
    return true;
}

/* 上一个用例是手工搭形状直接怼 verify；这个是走真实的删除路径，确认
 * 实现和校验器对"非叶 root 不许剩 0 个 key"这件事的理解是一致的。
 * 每删一次都查一遍：root 要么是叶子，要么至少有 1 个 key。 */
static bool test_delete_never_leaves_keyless_internal_root(void) {
    for (int t_deg = 2; t_deg <= 4; t_deg++) {
        BTree t = btree_create(t_deg);
        const int n = 60;
        for (int i = 1; i <= n; i++) CHECK(btree_insert(&t, i));

        /* 两头往中间删，尽量多触发 root 收缩 */
        int lo = 1, hi = n;
        while (lo <= hi) {
            CHECK(btree_delete(&t, lo));
            CHECK(verify_ok(&t, "shrink-toward-middle delete"));
            if (t.root) CHECK(t.root->is_leaf || t.root->n >= 1);
            lo++;
            if (lo > hi) break;
            CHECK(btree_delete(&t, hi));
            CHECK(verify_ok(&t, "shrink-toward-middle delete"));
            if (t.root) CHECK(t.root->is_leaf || t.root->n >= 1);
            hi--;
        }
        CHECK(t.root == NULL);
        btree_destroy(&t);
    }
    return true;
}

/* btree_create 对 t < 2 的守卫由独立程序 guard_check 验证（`make guard`）。
 * 守卫的做法是打 stderr 然后 exit(1)，同进程内测不了，只能 fork 看退出码；
 * 而 macOS 的 `leaks --atExit` 撞上 fork 会永久挂死，所以这个用例不留在
 * 本文件里——tests.c 必须保持 fork-free，否则整个 suite 就漏检泄漏了。
 * 详见 guard_check.c 顶部注释。 */

/* ---------- larger structured + randomized coverage ---------- */

static bool test_large_sequential_then_reverse_delete(void) {
    BTree t = btree_create(3);
    const int n = 500;
    for (int i = 1; i <= n; i++) {
        CHECK(btree_insert(&t, i));
        CHECK(verify_ok(&t, "large sequential insert"));
    }
    CHECK(btree_count_keys(&t) == n);
    for (int i = n; i >= 1; i--) {
        CHECK(btree_delete(&t, i));
        CHECK(verify_ok(&t, "large reverse delete"));
    }
    CHECK(t.root == NULL);
    btree_destroy(&t);
    return true;
}

static unsigned g_rand_state;
static int next_rand(void) {
    /* xorshift32, deterministic across platforms unlike rand() */
    g_rand_state ^= g_rand_state << 13;
    g_rand_state ^= g_rand_state >> 17;
    g_rand_state ^= g_rand_state << 5;
    return (int)(g_rand_state % 1000000u);
}

static bool run_random_stress(int t_deg, int n, unsigned seed) {
    g_rand_state = seed;
    BTree t = btree_create(t_deg);

    int *keys = malloc(sizeof(int) * (size_t)n);
    int distinct = 0;
    for (int i = 0; i < n; i++) {
        int k = next_rand();
        bool dup = false;
        for (int j = 0; j < distinct; j++) if (keys[j] == k) { dup = true; break; }
        if (dup) continue;
        keys[distinct++] = k;
        CHECK(btree_insert(&t, k));
        CHECK(verify_ok(&t, "random insert"));
    }
    CHECK(btree_count_keys(&t) == distinct);
    for (int i = 0; i < distinct; i++) CHECK(btree_search(&t, keys[i]));

    /* shuffle deletion order with the same generator */
    for (int i = distinct - 1; i > 0; i--) {
        int j = next_rand() % (i + 1);
        int tmp = keys[i]; keys[i] = keys[j]; keys[j] = tmp;
    }
    for (int i = 0; i < distinct; i++) {
        CHECK(btree_delete(&t, keys[i]));
        CHECK(verify_ok(&t, "random delete"));
        CHECK(!btree_search(&t, keys[i]));
        /* every remaining key must still be findable */
        for (int j = i + 1; j < distinct && j < i + 6; j++) CHECK(btree_search(&t, keys[j]));
    }
    CHECK(t.root == NULL);

    free(keys);
    btree_destroy(&t);
    return true;
}

static bool test_random_stress_t2_1000(void) { return run_random_stress(2, 1000, 12345u); }
static bool test_random_stress_t3_1000(void) { return run_random_stress(3, 1000, 67890u); }
static bool test_random_stress_t5_1500(void) { return run_random_stress(5, 1500, 999331u); }
static bool test_random_stress_t10_2000(void) { return run_random_stress(10, 2000, 424242u); }

int main(void) {
    printf("========== B-tree 测试套件 ==========\n");

    RUN(test_empty_tree_search_and_delete);
    RUN(test_single_key_insert_delete);
    RUN(test_duplicate_insert_rejected);
    RUN(test_delete_nonexistent_is_noop);

    RUN(test_degree_t2_sequential);
    RUN(test_degree_t3_sequential);
    RUN(test_degree_t4_sequential);

    RUN(test_root_split_on_insert);
    RUN(test_root_shrinks_after_merge);

    RUN(test_delete_internal_case2a_steal_predecessor);
    RUN(test_delete_internal_case2b_steal_successor);
    RUN(test_delete_internal_case2c_merge);
    RUN(test_delete_leaf_case3a_borrow_left);
    RUN(test_delete_leaf_case3b_borrow_right);
    RUN(test_delete_case3c_merge_while_descending);

    /* 回归测试：守住三个 btree_verify 查不出来的已修缺陷 */
    RUN(test_dup_insert_into_full_leaf_root_is_noop);
    RUN(test_dup_insert_into_full_internal_root_is_noop);
    RUN(test_dup_insert_into_full_child_is_noop);
    RUN(test_dup_insert_into_full_root_t3);
    RUN(test_verify_root_min_keys_splits_leaf_and_internal);
    RUN(test_delete_never_leaves_keyless_internal_root);

    RUN(test_large_sequential_then_reverse_delete);
    RUN(test_random_stress_t2_1000);
    RUN(test_random_stress_t3_1000);
    RUN(test_random_stress_t5_1500);
    RUN(test_random_stress_t10_2000);

    printf("\n========== 汇总 ==========\n");
    printf("通过: %d, 失败: %d, 总计: %d\n", g_pass, g_fail, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}
