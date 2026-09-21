/* B-tree, CLRS "minimum degree t" convention.
 *
 * For a tree of minimum degree t >= 2:
 *   - every non-root node has between t-1 and 2t-1 keys
 *   - every non-root internal node has between t and 2t children
 *   - the root may have between 0 and 2t-1 keys (0 means empty tree)
 *   - every leaf has the same depth
 *
 * Duplicate keys are rejected: btree_insert() returns false and leaves
 * the tree unchanged if the key already exists.
 */
#ifndef BTREE_H
#define BTREE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct BTreeNode {
    int n;                       /* number of keys currently stored */
    bool is_leaf;
    int *keys;                   /* capacity 2t-1 */
    struct BTreeNode **children; /* capacity 2t, unused when is_leaf */
} BTreeNode;

typedef struct {
    BTreeNode *root; /* NULL when empty */
    int t;            /* minimum degree, t >= 2 */
} BTree;

BTree btree_create(int t);
void btree_destroy(BTree *tree);

bool btree_search(const BTree *tree, int key);
bool btree_insert(BTree *tree, int key);
bool btree_delete(BTree *tree, int key);

int btree_count_keys(const BTree *tree);
int btree_count_nodes(const BTree *tree);
int btree_height(const BTree *tree); /* -1 for empty tree, 0 for single leaf root */

/* Structural property check. On failure, writes a human-readable reason
 * into err_buf (if non-NULL) and returns false. */
bool btree_verify(const BTree *tree, char *err_buf, size_t err_buf_size);

/* Indented per-level dump, for demo/debugging. */
void btree_print(const BTree *tree);

#endif
