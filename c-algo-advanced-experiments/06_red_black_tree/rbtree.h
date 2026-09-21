#ifndef RBTREE_H
#define RBTREE_H

#include <stdbool.h>
#include <stddef.h>

/* 红黑树五条性质（RB-property，出自 CLRS 第三版 Chapter 13）：
 * 1. 每个节点是红色或黑色。
 * 2. 根节点是黑色。
 * 3. 每个叶子节点（这里用哨兵 NIL 表示）是黑色。
 * 4. 如果一个节点是红色，则它的两个子节点都是黑色（不存在两个连续的红色节点）。
 * 5. 对每个节点，从该节点到其所有后代叶子的简单路径上，
 *    均包含相同数目的黑色节点（黑高一致，不含该节点自身）。
 *
 * 本实现使用「哨兵 NIL 节点」而不是 NULL 表示空叶子：
 * 整棵树只分配一个 NIL 节点，所有空指针位置都指向它。NIL 的颜色永远是黑色，
 * 且 NIL->parent 在旋转/删除修复过程中会被设置为「刚刚离开的那个位置的父节点」，
 * 这样删除修复函数可以统一从 NIL 出发往上找父节点，不需要额外传参数、
 * 不需要在每个函数里对「孩子是不是 NULL」写一次特判。
 * 代价：每次访问 x->left/x->right 时，即使 x 是叶子，也不用先判空，
 * 但反过来「打印/统计」这类只读函数要小心把 NIL 当成普通节点递归下去
 * （会死循环，因为 NIL 的 left/right 也指向自己）——本实现所有递归函数
 * 都先判 `x == tree->nil` 再展开。
 */

typedef enum { RB_RED, RB_BLACK } RBColor;

typedef struct RBNode {
    int key;
    RBColor color;
    struct RBNode *left;
    struct RBNode *right;
    struct RBNode *parent;
} RBNode;

typedef struct {
    RBNode *root;
    RBNode *nil; /* 哨兵：整棵树唯一的“空叶子”，颜色恒为黑 */
} RBTree;

RBTree rb_create(void);
void rb_destroy(RBTree *tree);

bool rb_search(const RBTree *tree, int key);
bool rb_insert(RBTree *tree, int key);   /* 重复键返回 false，不做任何修改 */
bool rb_delete(RBTree *tree, int key);   /* 键不存在返回 false */

int rb_count(const RBTree *tree);
int rb_black_height(const RBTree *tree); /* 从根到任意叶子的黑节点数（不含叶子本身），空树为 0 */

/* 性质检查：五条性质全部满足返回 true；否则把具体违反了哪条性质、
 * 在哪个节点上写进 err_buf（若非空）并返回 false。 */
bool rb_verify(const RBTree *tree, char *err_buf, size_t err_buf_size);

void rb_print(const RBTree *tree); /* 按层缩进打印，颜色标注为 R(key)/B(key) */

#endif /* RBTREE_H */
