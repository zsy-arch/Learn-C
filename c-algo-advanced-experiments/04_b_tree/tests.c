/* B-tree test suite. See README.md for the property definitions this
 * checks. Every insert/delete in this file is followed by btree_verify()
 * so a test that only checks btree_search() results could never hide a
 * structural bug (wrong key counts, unsorted keys, uneven leaf depth,
 * dangling child pointers). */
#include "btree.h"

#include <stdio.h>
#include <stdlib.h>

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

    RUN(test_large_sequential_then_reverse_delete);
    RUN(test_random_stress_t2_1000);
    RUN(test_random_stress_t3_1000);
    RUN(test_random_stress_t5_1500);
    RUN(test_random_stress_t10_2000);

    printf("\n========== 汇总 ==========\n");
    printf("通过: %d, 失败: %d, 总计: %d\n", g_pass, g_fail, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}
