/* Real performance measurement for the B-tree implementation.
 *
 * Not part of the correctness test suite (tests.c already covers that with
 * -O0 -g and sanitizers). This file is compiled with -O2 specifically to
 * measure realistic insert/search/delete timing across different minimum
 * degrees t, and to count how many nodes an in-order range scan touches so
 * the B+-tree chapter has a real number to compare its leaf-linked-list
 * range query against.
 *
 * Usage: ./perf
 * All numbers printed are measured on this machine at run time, not
 * estimated or hard-coded.
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "btree.h"

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

/* xorshift32, deterministic seed so runs are reproducible. */
static unsigned int rng_state;
static unsigned int next_rand(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static int *make_shuffled_keys(int n) {
    int *keys = malloc(sizeof(int) * (size_t)n);
    for (int i = 0; i < n; i++) keys[i] = i;
    for (int i = n - 1; i > 0; i--) {
        int j = (int)(next_rand() % (unsigned int)(i + 1));
        int tmp = keys[i];
        keys[i] = keys[j];
        keys[j] = tmp;
    }
    return keys;
}

/* Count nodes visited by an in-order traversal restricted to [low, high].
 * This simulates "how would a B-tree answer a range query" (there is no
 * leaf linked list, so a range query must walk the tree structure), for
 * comparison against the B+-tree chapter's leaf-linked-list scan count. */
static void range_scan_rec(BTreeNode *x, int low, int high, long *nodes_visited, long *keys_hit) {
    if (!x) return;
    (*nodes_visited)++;
    int i = 0;
    while (i < x->n && x->keys[i] < low) i++;
    /* Descend into every child that could contain keys in range. */
    while (i <= x->n) {
        if (!x->is_leaf) {
            range_scan_rec(x->children[i], low, high, nodes_visited, keys_hit);
        }
        if (i < x->n) {
            if (x->keys[i] >= low && x->keys[i] <= high) (*keys_hit)++;
            if (x->keys[i] > high) return;
        }
        i++;
    }
}

static void range_query_count(const BTree *tree, int low, int high, long *nodes_visited, long *keys_hit) {
    *nodes_visited = 0;
    *keys_hit = 0;
    range_scan_rec(tree->root, low, high, nodes_visited, keys_hit);
}

static void bench_one_degree(int t, int n) {
    printf("\n--- t = %d, n = %d ---\n", t, n);

    rng_state = 0xC0FFEEu + (unsigned int)t * 7919u;
    int *insert_keys = make_shuffled_keys(n);

    BTree tree = btree_create(t);

    double t0 = now_ms();
    for (int i = 0; i < n; i++) {
        btree_insert(&tree, insert_keys[i]);
    }
    double t1 = now_ms();
    printf("insert %d keys: %.2f ms (%.3f us/op)\n", n, t1 - t0, (t1 - t0) * 1000.0 / n);

    char err[256];
    if (!btree_verify(&tree, err, sizeof(err))) {
        fprintf(stderr, "VERIFY FAILED after bulk insert: %s\n", err);
        exit(1);
    }
    printf("height after insert: %d, nodes: %d\n", btree_height(&tree), btree_count_nodes(&tree));

    /* Search benchmark: shuffle a fresh order to avoid cache-friendly bias
     * from re-using the insertion order. */
    int *search_keys = make_shuffled_keys(n);
    double t2 = now_ms();
    long hits = 0;
    for (int i = 0; i < n; i++) {
        if (btree_search(&tree, search_keys[i])) hits++;
    }
    double t3 = now_ms();
    printf("search %d keys: %.2f ms (%.3f us/op), hits=%ld\n", n, t3 - t2, (t3 - t2) * 1000.0 / n, hits);

    /* Range query cost at several widths, to compare against the B+-tree
     * leaf-linked-list scan in the B+-tree chapter. */
    int widths[] = {10, 100, 1000, n / 10};
    for (size_t w = 0; w < sizeof(widths) / sizeof(widths[0]); w++) {
        int width = widths[w];
        if (width <= 0 || width > n) continue;
        int low = n / 2;
        int high = low + width;
        if (high >= n) high = n - 1;
        long nodes_visited, keys_hit;
        range_query_count(&tree, low, high, &nodes_visited, &keys_hit);
        printf("range query [%d, %d] (width %d): visited %ld nodes, matched %ld keys\n",
               low, high, high - low, nodes_visited, keys_hit);
    }

    /* Delete benchmark: delete every key in a third random order. */
    int *delete_keys = make_shuffled_keys(n);
    double t4 = now_ms();
    for (int i = 0; i < n; i++) {
        btree_delete(&tree, delete_keys[i]);
    }
    double t5 = now_ms();
    printf("delete %d keys: %.2f ms (%.3f us/op)\n", n, t5 - t4, (t5 - t4) * 1000.0 / n);

    if (!btree_verify(&tree, err, sizeof(err))) {
        fprintf(stderr, "VERIFY FAILED after bulk delete: %s\n", err);
        exit(1);
    }
    printf("height after full delete: %d, nodes: %d (0 expected)\n", btree_height(&tree), btree_count_nodes(&tree));

    btree_destroy(&tree);
    free(insert_keys);
    free(search_keys);
    free(delete_keys);
}

int main(void) {
    printf("B-tree performance measurement (compiled with -O2, run on this machine now)\n");
    printf("CLOCK_MONOTONIC via clock_gettime, xorshift32 PRNG, fixed seeds per run.\n");

    int ns[] = {1000, 10000, 100000};
    int ts[] = {2, 8, 64};

    for (size_t ti = 0; ti < sizeof(ts) / sizeof(ts[0]); ti++) {
        for (size_t ni = 0; ni < sizeof(ns) / sizeof(ns[0]); ni++) {
            bench_one_degree(ts[ti], ns[ni]);
        }
    }
    return 0;
}
