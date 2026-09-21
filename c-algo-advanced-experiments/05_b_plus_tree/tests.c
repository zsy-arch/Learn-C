/* B+-tree test suite. See README.md for the property definitions this
 * checks. Every insert/delete in this file is followed by
 * bplustree_verify(), which (unlike the B-tree's checker) also walks the
 * leaf chain directly and cross-checks it against the tree recursion --
 * so a test that only checks bplustree_search()/range_query() results
 * could never hide a broken next pointer or a miscounted leaf. */
#include "bplustree.h"

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

static bool verify_ok(const BPlusTree *t, const char *ctx) {
    char err[256];
    if (!bplustree_verify(t, err, sizeof err)) {
        printf("  bplustree_verify failed after %s: %s\n", ctx, err);
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
    BPlusTree t = bplustree_create(3);
    int val;
    CHECK(!bplustree_search(&t, 5, &val));
    CHECK(!bplustree_delete(&t, 5));
    CHECK(verify_ok(&t, "empty"));
    CHECK(bplustree_height(&t) == -1);
    CHECK(bplustree_count_keys(&t) == 0);
    CHECK(bplustree_count_leaves(&t) == 0);
    int keys[4], vals[4], visited;
    CHECK(bplustree_range_query(&t, 0, 100, keys, vals, 4, &visited) == 0);
    CHECK(visited == 0);
    bplustree_destroy(&t);
    return true;
}

static bool test_single_key_insert_delete(void) {
    BPlusTree t = bplustree_create(2);
    int val;
    CHECK(bplustree_insert(&t, 42, 4200));
    CHECK(verify_ok(&t, "insert 42"));
    CHECK(bplustree_search(&t, 42, &val) && val == 4200);
    CHECK(!bplustree_search(&t, 43, &val));
    CHECK(bplustree_height(&t) == 0);
    CHECK(bplustree_count_leaves(&t) == 1);
    CHECK(bplustree_delete(&t, 42));
    CHECK(verify_ok(&t, "delete 42"));
    CHECK(!bplustree_search(&t, 42, &val));
    CHECK(t.root == NULL);
    bplustree_destroy(&t);
    return true;
}

static bool test_duplicate_insert_rejected(void) {
    BPlusTree t = bplustree_create(3);
    int val;
    CHECK(bplustree_insert(&t, 10, 100));
    CHECK(!bplustree_insert(&t, 10, 999)); /* rejected: key already present */
    CHECK(bplustree_search(&t, 10, &val) && val == 100);
    CHECK(verify_ok(&t, "duplicate insert rejected"));
    bplustree_destroy(&t);
    return true;
}

static bool test_delete_nonexistent_is_noop(void) {
    BPlusTree t = bplustree_create(2);
    for (int k = 1; k <= 5; k++) CHECK(bplustree_insert(&t, k * 10, k));
    CHECK(!bplustree_delete(&t, 999));
    CHECK(!bplustree_delete(&t, 15));
    CHECK(bplustree_count_keys(&t) == 5);
    CHECK(verify_ok(&t, "delete nonexistent"));
    bplustree_destroy(&t);
    return true;
}

/* ---------- multiple order values (t) ---------- */

static bool insert_sequential_and_verify(int t_deg, int n) {
    BPlusTree t = bplustree_create(t_deg);
    for (int i = 1; i <= n; i++) {
        CHECK(bplustree_insert(&t, i, i * 10));
        CHECK(verify_ok(&t, "sequential insert"));
    }
    CHECK(bplustree_count_keys(&t) == n);
    int val;
    for (int i = 1; i <= n; i++) CHECK(bplustree_search(&t, i, &val) && val == i * 10);
    for (int i = n; i >= 1; i--) {
        CHECK(bplustree_delete(&t, i));
        CHECK(verify_ok(&t, "sequential delete"));
    }
    CHECK(t.root == NULL);
    bplustree_destroy(&t);
    return true;
}

static bool test_degree_t2_sequential(void) { return insert_sequential_and_verify(2, 80); }
static bool test_degree_t3_sequential(void) { return insert_sequential_and_verify(3, 80); }
static bool test_degree_t4_sequential(void) { return insert_sequential_and_verify(4, 80); }
static bool test_degree_t10_sequential(void) { return insert_sequential_and_verify(10, 200); }

/* ---------- targeted split/borrow/merge shapes ---------- */

static bool test_leaf_split_copies_key_upward(void) {
    /* t=2: leaf capacity 2t-1=3. Fourth insert must split the root leaf. */
    BPlusTree t = bplustree_create(2);
    CHECK(bplustree_insert(&t, 10, 1));
    CHECK(bplustree_insert(&t, 20, 2));
    CHECK(bplustree_insert(&t, 30, 3));
    CHECK(t.root->is_leaf);
    CHECK(bplustree_insert(&t, 40, 4));
    CHECK(!t.root->is_leaf);
    CHECK(t.root->n == 1);
    CHECK(t.root->keys[0] == 30); /* promoted key */
    /* the promoted key must still be REAL data in the right leaf */
    BPlusNode *right = t.root->u.children[1];
    CHECK(right->is_leaf);
    CHECK(right->n >= 1 && right->keys[0] == 30);
    CHECK(verify_ok(&t, "leaf split"));
    bplustree_destroy(&t);
    return true;
}

static bool test_root_split_moves_key_no_duplicate(void) {
    /* t=2: build until the root (internal) itself splits. */
    BPlusTree t = bplustree_create(2);
    for (int k = 10; k <= 90; k += 10) CHECK(bplustree_insert(&t, k, k));
    CHECK(!t.root->is_leaf);
    CHECK(t.root->n == 3); /* full internal root: 2t-1=3 */
    int old_root_mid = t.root->keys[1];

    CHECK(bplustree_insert(&t, 100, 100));
    CHECK(!t.root->is_leaf);
    CHECK(t.root->n == 1);
    CHECK(t.root->keys[0] == old_root_mid); /* moved, not copied */

    /* the moved key must NOT appear as a routing key in either child */
    BPlusNode *left = t.root->u.children[0];
    BPlusNode *rightc = t.root->u.children[1];
    for (int i = 0; i < left->n; i++) CHECK(left->keys[i] != old_root_mid);
    for (int i = 0; i < rightc->n; i++) CHECK(rightc->keys[i] != old_root_mid);
    CHECK(verify_ok(&t, "root/internal split"));
    bplustree_destroy(&t);
    return true;
}

static bool test_leaf_borrow_from_left_sibling(void) {
    BPlusTree t = bplustree_create(2);
    for (int k = 10; k <= 130; k += 10) CHECK(bplustree_insert(&t, k, k));
    CHECK(bplustree_delete(&t, 130));
    CHECK(bplustree_delete(&t, 120)); /* last leaf now [110], n=1=t-1, no spare */
    int nodes_before = bplustree_count_nodes(&t);

    CHECK(bplustree_delete(&t, 110)); /* forces borrow: leaf [110] would be empty */
    CHECK(bplustree_count_nodes(&t) == nodes_before); /* borrow never changes node count */
    CHECK(verify_ok(&t, "leaf borrow"));
    int val;
    CHECK(!bplustree_search(&t, 110, &val));
    CHECK(bplustree_search(&t, 90, &val) && bplustree_search(&t, 100, &val));
    bplustree_destroy(&t);
    return true;
}

static bool test_leaf_merge_repairs_chain(void) {
    BPlusTree t = bplustree_create(2);
    for (int k = 10; k <= 130; k += 10) CHECK(bplustree_insert(&t, k, k));
    bplustree_delete(&t, 130);
    bplustree_delete(&t, 120);
    bplustree_delete(&t, 110); /* borrow happened here: [90,100] -> [90]/[100] */
    int leaves_before = bplustree_count_leaves(&t);

    CHECK(bplustree_delete(&t, 100)); /* [100] and [90] both at min -> merge */
    CHECK(bplustree_count_leaves(&t) == leaves_before - 1);
    CHECK(verify_ok(&t, "leaf merge + chain repair"));

    /* walk the chain by hand and confirm 100 is gone but everything else survives in order */
    const BPlusNode *x = t.root;
    while (!x->is_leaf) x = x->u.children[0];
    int prev = -1;
    int seen = 0;
    while (x) {
        for (int i = 0; i < x->n; i++) {
            CHECK(x->keys[i] > prev);
            CHECK(x->keys[i] != 100);
            prev = x->keys[i];
            seen++;
        }
        x = x->next;
    }
    CHECK(seen == bplustree_count_keys(&t));
    bplustree_destroy(&t);
    return true;
}

static bool test_internal_borrow(void) {
    /* Hand-built asymmetric shape: root has 2 children, [30] (min: 1
     * routing key, 2 leaf children) and [120 140 160] (spare: 3
     * routing keys, 4 leaf children). Thinning both of [30]'s leaves
     * to n=1 each (no spare between them) and then deleting one forces
     * a leaf merge that empties [30] down to n=0/1-child -- which must
     * then borrow a routing key + child from the spare sibling instead
     * of merging, since the sibling has more than the t-1=1 minimum. */
    BPlusTree t = bplustree_create(2);
    int leftkeys[] = {10, 20, 30, 40};
    int rightkeys[] = {100, 110, 120, 130, 140, 150, 160, 170, 180};
    for (size_t i = 0; i < sizeof(leftkeys) / sizeof(leftkeys[0]); i++) CHECK(bplustree_insert(&t, leftkeys[i], leftkeys[i]));
    for (size_t i = 0; i < sizeof(rightkeys) / sizeof(rightkeys[0]); i++) CHECK(bplustree_insert(&t, rightkeys[i], rightkeys[i]));
    CHECK(!t.root->is_leaf && t.root->n == 1); /* root: 2 children, [30] and [120 140 160] */
    CHECK(t.root->u.children[0]->n == 1);      /* [30]: at minimum, no spare */
    CHECK(t.root->u.children[1]->n == 3);      /* [120 140 160]: spare */

    CHECK(bplustree_delete(&t, 20)); /* [10,20] -> [10] */
    CHECK(bplustree_delete(&t, 40)); /* [30,40] -> [30] */
    CHECK(verify_ok(&t, "thin both left leaves to min"));

    int nodes_before = bplustree_count_nodes(&t);
    int leaves_before = bplustree_count_leaves(&t);
    int internal_before = nodes_before - leaves_before;

    CHECK(bplustree_delete(&t, 10)); /* [10] empty; sibling [30] also n=1 -> leaf merge -> parent underflows */

    int nodes_after = bplustree_count_nodes(&t);
    int leaves_after = bplustree_count_leaves(&t);
    int internal_after = nodes_after - leaves_after;
    CHECK(leaves_after == leaves_before - 1);   /* exactly one leaf freed by the leaf-level merge */
    CHECK(internal_after == internal_before);   /* NO internal node freed: this was a borrow, not a merge */
    CHECK(t.root->n == 1);                      /* root itself untouched: the borrow happened one level below it */
    CHECK(verify_ok(&t, "internal borrow"));
    bplustree_destroy(&t);
    return true;
}

static bool test_internal_merge_pulls_separator_down(void) {
    BPlusTree t = bplustree_create(2);
    for (int k = 0; k < 60; k += 2) CHECK(bplustree_insert(&t, k, k));
    int drain[] = {58, 56, 54, 52, 50, 48, 46, 44};
    for (size_t i = 0; i < sizeof(drain) / sizeof(drain[0]); i++) CHECK(bplustree_delete(&t, drain[i]));
    CHECK(verify_ok(&t, "pre internal-merge state"));
    int leaves_before = bplustree_count_leaves(&t);
    int nodes_before = bplustree_count_nodes(&t);
    int internal_before = nodes_before - leaves_before;

    CHECK(bplustree_delete(&t, 42)); /* leaf merge cascades through two internal-node merges */
    int leaves_after = bplustree_count_leaves(&t);
    int nodes_after = bplustree_count_nodes(&t);
    int internal_after = nodes_after - leaves_after;
    CHECK(leaves_after == leaves_before - 1);     /* one leaf freed */
    CHECK(internal_after == internal_before - 2); /* two internal nodes freed by the cascade */
    CHECK(verify_ok(&t, "internal merge"));
    bplustree_destroy(&t);
    return true;
}

static bool test_root_collapses_to_child(void) {
    BPlusTree t = bplustree_create(2);
    for (int k = 0; k < 60; k += 2) CHECK(bplustree_insert(&t, k, k));
    int drain[] = {58,56,54,52,50,48,46,44,42,40,38,36,34,32,30,28};
    for (size_t i = 0; i < sizeof(drain) / sizeof(drain[0]); i++) CHECK(bplustree_delete(&t, drain[i]));
    CHECK(verify_ok(&t, "pre root-collapse state"));
    int height_before = bplustree_height(&t);
    CHECK(height_before >= 1);

    /* keep deleting the tree's minimum key until the root itself shrinks */
    int prev_height = height_before;
    bool saw_shrink = false;
    while (bplustree_count_keys(&t) > 0) {
        const BPlusNode *x = t.root;
        while (!x->is_leaf) x = x->u.children[0];
        CHECK(bplustree_delete(&t, x->keys[0]));
        CHECK(verify_ok(&t, "drain to empty"));
        int h = bplustree_height(&t);
        if (h < prev_height) saw_shrink = true;
        prev_height = h;
    }
    CHECK(saw_shrink);
    CHECK(t.root == NULL);
    CHECK(bplustree_height(&t) == -1);
    bplustree_destroy(&t);
    return true;
}

static bool test_delete_root_variants(void) {
    /* root-as-leaf, single key */
    {
        BPlusTree t = bplustree_create(3);
        CHECK(bplustree_insert(&t, 7, 70));
        CHECK(bplustree_delete(&t, 7));
        CHECK(t.root == NULL);
        CHECK(verify_ok(&t, "delete root-as-leaf single key"));
        bplustree_destroy(&t);
    }
    /* root-as-internal, delete until it collapses down to a leaf root */
    {
        BPlusTree t = bplustree_create(2);
        for (int k = 10; k <= 90; k += 10) CHECK(bplustree_insert(&t, k, k));
        CHECK(!t.root->is_leaf);
        for (int k = 90; k >= 10; k -= 10) {
            CHECK(bplustree_delete(&t, k));
            CHECK(verify_ok(&t, "drain root-as-internal"));
        }
        CHECK(t.root == NULL);
        bplustree_destroy(&t);
    }
    return true;
}

/* ---------- leaf-chain integrity (also covered by verify(), checked explicitly here too) ---------- */

static bool test_leaf_chain_matches_sorted_keys(void) {
    BPlusTree t = bplustree_create(3);
    int order[] = {50, 10, 90, 30, 70, 20, 80, 40, 60, 5, 15, 25, 35};
    for (size_t i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
        CHECK(bplustree_insert(&t, order[i], order[i]));
        CHECK(verify_ok(&t, "chain build"));
    }
    const BPlusNode *x = t.root;
    while (!x->is_leaf) x = x->u.children[0];
    int prev = -1, count = 0, leaf_count = 0;
    while (x) {
        for (int i = 0; i < x->n; i++) {
            CHECK(x->keys[i] > prev);
            prev = x->keys[i];
            count++;
        }
        leaf_count++;
        x = x->next;
    }
    CHECK(count == bplustree_count_keys(&t));
    CHECK(leaf_count == bplustree_count_leaves(&t));
    bplustree_destroy(&t);
    return true;
}

/* ---------- range query cross-checked against brute force ---------- */

static int cmp_int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

static bool run_range_check(const BPlusTree *t, const int *sorted_keys, int n, int low, int high) {
    int keys[256], vals[256], visited;
    int cap = (int)(sizeof(keys) / sizeof(keys[0]));
    int found = bplustree_range_query(t, low, high, keys, vals, cap, &visited);

    int expect[256], expect_n = 0;
    for (int i = 0; i < n; i++) if (sorted_keys[i] >= low && sorted_keys[i] <= high) expect[expect_n++] = sorted_keys[i];

    CHECK(found == expect_n);
    for (int i = 0; i < found; i++) {
        CHECK(keys[i] == expect[i]);
        CHECK(vals[i] == expect[i]); /* this test always inserts value==key */
    }
    CHECK(visited >= 0);
    CHECK(visited <= bplustree_count_leaves(t));
    return true;
}

static bool test_range_query_edge_cases(void) {
    BPlusTree t = bplustree_create(3);
    int keys[100];
    int n = 0;
    for (int k = 0; k < 100; k += 3) keys[n++] = k; /* 0,3,6,...,99 */
    for (int i = 0; i < n; i++) CHECK(bplustree_insert(&t, keys[i], keys[i]));
    qsort(keys, (size_t)n, sizeof(int), cmp_int);
    CHECK(verify_ok(&t, "range query fixture"));

    CHECK(run_range_check(&t, keys, n, -1000, -1));      /* no match: entirely below range */
    CHECK(run_range_check(&t, keys, n, 10000, 20000));   /* no match: entirely above range */
    CHECK(run_range_check(&t, keys, n, 1, 1));            /* no match: single point, absent key */
    CHECK(run_range_check(&t, keys, n, 0, 0));            /* single point, present key */
    CHECK(run_range_check(&t, keys, n, keys[n - 1], keys[n - 1])); /* single point, last key */
    CHECK(run_range_check(&t, keys, n, -1000, 10000));    /* full coverage */
    CHECK(run_range_check(&t, keys, n, 40, 55));          /* partial, both bounds absent keys */
    CHECK(run_range_check(&t, keys, n, 39, 60));          /* partial, both bounds present keys */
    CHECK(run_range_check(&t, keys, n, 97, 1000));        /* partial tail past the last key */
    CHECK(run_range_check(&t, keys, n, -50, 2));          /* partial head before the first key */

    bplustree_destroy(&t);
    return true;
}

static bool test_range_query_low_greater_than_high(void) {
    BPlusTree t = bplustree_create(2);
    for (int k = 0; k < 50; k += 5) CHECK(bplustree_insert(&t, k, k));
    int keys[16], vals[16], visited;
    CHECK(bplustree_range_query(&t, 40, 10, keys, vals, 16, &visited) == 0);
    bplustree_destroy(&t);
    return true;
}

/* Contract test: when the matching range is larger than the caller's
 * buffer, the return value must be clamped to cap. Before this was fixed
 * the counter kept incrementing past the buffer and the function returned
 * the full match count (50 for cap=5), so any caller looping up to the
 * returned value read off the end of its own array -- ASan reported a
 * heap-buffer-overflow in the caller, not in this file. */
static bool test_range_query_respects_cap(void) {
    BPlusTree t = bplustree_create(3);
    for (int k = 1; k <= 100; k++) CHECK(bplustree_insert(&t, k, k * 10));

    enum { CAP = 5 };
    int keys[CAP], vals[CAP], visited;
    int n = bplustree_range_query(&t, 10, 59, keys, vals, CAP, &visited);

    CHECK(n == CAP);                    /* 50 matches, but cap is 5 */
    for (int i = 0; i < n; i++) {       /* every reported slot is real */
        CHECK(keys[i] == 10 + i);
        CHECK(vals[i] == (10 + i) * 10);
    }

    /* cap == 0 must report 0 and touch nothing. */
    CHECK(bplustree_range_query(&t, 10, 59, keys, vals, 0, &visited) == 0);

    /* An exactly-sized buffer still reports the true count. */
    int exact_keys[10], exact_vals[10];
    CHECK(bplustree_range_query(&t, 10, 19, exact_keys, exact_vals, 10, &visited) == 10);

    bplustree_destroy(&t);
    return true;
}

/* ---------- large-scale round trips ---------- */

static bool test_large_sequential_then_reverse_delete(void) {
    BPlusTree t = bplustree_create(3);
    const int n = 600;
    for (int i = 1; i <= n; i++) {
        CHECK(bplustree_insert(&t, i, i * 10));
        CHECK(verify_ok(&t, "large sequential insert"));
    }
    CHECK(bplustree_count_keys(&t) == n);
    for (int i = n; i >= 1; i--) {
        CHECK(bplustree_delete(&t, i));
        CHECK(verify_ok(&t, "large reverse delete"));
    }
    CHECK(t.root == NULL);
    bplustree_destroy(&t);
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
    BPlusTree t = bplustree_create(t_deg);

    int *keys = malloc(sizeof(int) * (size_t)n);
    int distinct = 0;
    for (int i = 0; i < n; i++) {
        int k = next_rand();
        bool dup = false;
        for (int j = 0; j < distinct; j++) if (keys[j] == k) { dup = true; break; }
        if (dup) continue;
        keys[distinct++] = k;
        CHECK(bplustree_insert(&t, k, k * 2));
        CHECK(verify_ok(&t, "random insert"));
    }
    CHECK(bplustree_count_keys(&t) == distinct);
    int val;
    for (int i = 0; i < distinct; i++) CHECK(bplustree_search(&t, keys[i], &val) && val == keys[i] * 2);

    /* a mid-run range query cross-checked against a sorted brute-force copy */
    int *sorted = malloc(sizeof(int) * (size_t)distinct);
    memcpy(sorted, keys, sizeof(int) * (size_t)distinct);
    qsort(sorted, (size_t)distinct, sizeof(int), cmp_int);
    if (distinct >= 2) {
        int low = sorted[distinct / 4];
        int high = sorted[(3 * distinct) / 4];
        int rkeys[4096], rvals[4096], visited;
        int cap = (int)(sizeof(rkeys) / sizeof(rkeys[0]));
        int found = bplustree_range_query(&t, low, high, rkeys, rvals, cap, &visited);
        int expect_n = 0;
        for (int i = 0; i < distinct; i++) if (sorted[i] >= low && sorted[i] <= high) expect_n++;
        CHECK(found == expect_n || found == cap); /* cap only bites if expect_n > cap */
    }
    free(sorted);

    /* shuffle deletion order with the same generator */
    for (int i = distinct - 1; i > 0; i--) {
        int j = next_rand() % (i + 1);
        int tmp = keys[i]; keys[i] = keys[j]; keys[j] = tmp;
    }
    for (int i = 0; i < distinct; i++) {
        CHECK(bplustree_delete(&t, keys[i]));
        CHECK(verify_ok(&t, "random delete"));
        CHECK(!bplustree_search(&t, keys[i], &val));
        /* every remaining key must still be findable */
        for (int j = i + 1; j < distinct && j < i + 6; j++) CHECK(bplustree_search(&t, keys[j], &val));
    }
    CHECK(t.root == NULL);

    free(keys);
    bplustree_destroy(&t);
    return true;
}

static bool test_random_stress_t2_1000(void) { return run_random_stress(2, 1000, 12345u); }
static bool test_random_stress_t3_1000(void) { return run_random_stress(3, 1000, 67890u); }
static bool test_random_stress_t5_1500(void) { return run_random_stress(5, 1500, 999331u); }
static bool test_random_stress_t10_2000(void) { return run_random_stress(10, 2000, 424242u); }

int main(void) {
    printf("========== B+ 树测试套件 ==========\n");

    RUN(test_empty_tree_search_and_delete);
    RUN(test_single_key_insert_delete);
    RUN(test_duplicate_insert_rejected);
    RUN(test_delete_nonexistent_is_noop);

    RUN(test_degree_t2_sequential);
    RUN(test_degree_t3_sequential);
    RUN(test_degree_t4_sequential);
    RUN(test_degree_t10_sequential);

    RUN(test_leaf_split_copies_key_upward);
    RUN(test_root_split_moves_key_no_duplicate);
    RUN(test_leaf_borrow_from_left_sibling);
    RUN(test_leaf_merge_repairs_chain);
    RUN(test_internal_borrow);
    RUN(test_internal_merge_pulls_separator_down);
    RUN(test_root_collapses_to_child);
    RUN(test_delete_root_variants);

    RUN(test_leaf_chain_matches_sorted_keys);
    RUN(test_range_query_edge_cases);
    RUN(test_range_query_low_greater_than_high);
    RUN(test_range_query_respects_cap);

    RUN(test_large_sequential_then_reverse_delete);
    RUN(test_random_stress_t2_1000);
    RUN(test_random_stress_t3_1000);
    RUN(test_random_stress_t5_1500);
    RUN(test_random_stress_t10_2000);

    printf("\n========== 汇总 ==========\n");
    printf("通过: %d, 失败: %d, 总计: %d\n", g_pass, g_fail, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}
