/* B+-tree, CLRS-style "minimum degree t" convention (same parameter as
 * this project's 04_b_tree, for direct comparison).
 *
 * Unlike a B-tree, a B+-tree has two different node shapes:
 *
 *   - Internal nodes store ONLY routing keys, never real data. Each
 *     routing key is a COPY of some key that actually lives in a leaf;
 *     it exists purely to decide "which child subtree to descend into".
 *   - Leaf nodes store the real key/value pairs. All data lives here.
 *     Every leaf also has a `next` pointer, chaining every leaf in the
 *     tree into a single sorted singly-linked list.
 *
 * For a tree of minimum degree t (t >= 2):
 *   - every leaf holds between t-1 and 2t-1 key/value pairs, except the
 *     root when it is also a leaf (0 to 2t-1 pairs; 0 means empty tree)
 *   - every non-root internal node has between t and 2t children
 *     (equivalently, between t-1 and 2t-1 routing keys)
 *   - the root, if internal, has between 1 and 2t-1 routing keys (at
 *     least 1, because a root with 0 keys would have only one child and
 *     should have collapsed into that child)
 *   - every leaf has the same depth
 *   - the leaf linked list, read left to right, yields every key in the
 *     tree exactly once, in sorted order
 *
 * Duplicate keys are rejected: bplustree_insert() returns false and
 * leaves the tree unchanged if the key already exists.
 */
#ifndef BPLUSTREE_H
#define BPLUSTREE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct BPlusNode {
    bool is_leaf;
    int n;                        /* internal: # routing keys. leaf: # kv pairs */
    int *keys;                    /* internal: capacity 2t-1 routing keys.
                                    * leaf: capacity 2t-1 real keys. */
    union {
        struct BPlusNode **children; /* internal only, capacity 2t */
        int *values;                 /* leaf only, capacity 2t-1, values[i]
                                       * is the data associated with keys[i] */
    } u;
    struct BPlusNode *next;       /* leaf only: next leaf in sorted order,
                                    * NULL for the rightmost leaf. Unused
                                    * (left NULL) on internal nodes. */
} BPlusNode;

typedef struct {
    BPlusNode *root; /* NULL when empty */
    int t;             /* minimum degree, t >= 2 */
} BPlusTree;

BPlusTree bplustree_create(int t);
void bplustree_destroy(BPlusTree *tree);

bool bplustree_search(const BPlusTree *tree, int key, int *value_out);
bool bplustree_insert(BPlusTree *tree, int key, int value);
bool bplustree_delete(BPlusTree *tree, int key);

/* Collects every (key, value) with low <= key <= high, in ascending key
 * order, into caller-supplied arrays of capacity cap. Returns the number
 * of pairs written, which is never more than cap: if the range holds more
 * matches than the buffer can take, collection STOPS at cap and the
 * return value is cap. The return value is therefore always a safe bound
 * for `for (i = 0; i < ret; i++) ... keys_out[i]`; it is NOT a count of
 * how many matches exist (use a larger cap, or count separately, if you
 * need that). *nodes_visited_out (if non-NULL) is set to how many leaf
 * nodes were walked via the `next` chain, for the range-query cost
 * experiments in the README. */
int bplustree_range_query(const BPlusTree *tree, int low, int high,
                           int *keys_out, int *values_out, int cap,
                           int *nodes_visited_out);

int bplustree_count_keys(const BPlusTree *tree);
int bplustree_count_nodes(const BPlusTree *tree);
int bplustree_count_leaves(const BPlusTree *tree);
int bplustree_height(const BPlusTree *tree); /* -1 empty, 0 single leaf root */

/* Structural property check: key bounds, sortedness, uniform leaf depth,
 * routing-key/subtree-range consistency, AND leaf-list integrity (the
 * leaf chain must visit every leaf exactly once, in sorted order, and
 * the total key count reachable via the chain must equal the key count
 * reachable via tree recursion). On failure, writes a human-readable
 * reason into err_buf (if non-NULL) and returns false. */
bool bplustree_verify(const BPlusTree *tree, char *err_buf, size_t err_buf_size);

/* Indented per-level dump, for demo/debugging. */
void bplustree_print(const BPlusTree *tree);

/* Prints the leaf chain left-to-right by following `next` pointers,
 * independent of tree recursion -- used to visually confirm the sibling
 * list survived a split/borrow/merge intact. */
void bplustree_print_leaf_chain(const BPlusTree *tree);

#endif
