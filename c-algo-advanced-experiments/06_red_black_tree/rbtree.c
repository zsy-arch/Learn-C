#include "rbtree.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- 创建 / 销毁 ---- */

RBTree rb_create(void) {
    RBTree tree;
    tree.nil = malloc(sizeof *tree.nil);
    if (!tree.nil) { perror("malloc"); exit(1); }
    tree.nil->color = RB_BLACK;
    tree.nil->left = tree.nil->right = tree.nil->parent = tree.nil;
    tree.nil->key = 0; /* 值本身没有意义，NIL 不参与比较 */
    tree.root = tree.nil;
    return tree;
}

static void destroy_rec(RBTree *tree, RBNode *x) {
    if (x == tree->nil) return;
    destroy_rec(tree, x->left);
    destroy_rec(tree, x->right);
    free(x);
}

void rb_destroy(RBTree *tree) {
    destroy_rec(tree, tree->root);
    free(tree->nil);
    /* 两个指针都要置空。原来的顺序是 root = nil 之后再 free(nil)，于是
     * 销毁完成后 nil == NULL 而 root 指向刚被释放的内存——"清理了一半"
     * 比完全不清理更危险：nil == NULL 看起来像个可用的哨兵判断，误导
     * 调用方以为这个 tree 处于某种可检测的已销毁状态。 */
    tree->root = NULL;
    tree->nil = NULL;
}

/* ---- 查找 ---- */

bool rb_search(const RBTree *tree, int key) {
    RBNode *x = tree->root;
    while (x != tree->nil) {
        if (key == x->key) return true;
        x = (key < x->key) ? x->left : x->right;
    }
    return false;
}

/* ---- 旋转 ----
 * left_rotate(x)：x 的右孩子 y 顶替 x 的位置，x 变成 y 的左孩子。
 *
 *      x                       y
 *     / \                     / \
 *    a   y      ==>          x   c
 *       / \                 / \
 *      b   c               a   b
 *
 * 一共有三个节点的 parent 会变化：y（接管 x 原来的位置）、
 * x（变成 y 的左孩子）、b（从 y 的左孩子变成 x 的右孩子）。
 * 新手最容易漏掉的就是 b 的 parent——b 换了「继父」，必须显式更新。
 */
static void left_rotate(RBTree *tree, RBNode *x) {
    RBNode *y = x->right;

    x->right = y->left;           /* b 过继给 x */
    if (y->left != tree->nil) y->left->parent = x;

    y->parent = x->parent;        /* y 接管 x 原来的位置 */
    if (x->parent == tree->nil) tree->root = y;
    else if (x == x->parent->left) x->parent->left = y;
    else x->parent->right = y;

    y->left = x;                  /* x 变成 y 的左孩子 */
    x->parent = y;
}

/* right_rotate 是 left_rotate 的镜像：x 的左孩子 y 顶替 x 的位置。 */
static void right_rotate(RBTree *tree, RBNode *x) {
    RBNode *y = x->left;

    x->left = y->right;
    if (y->right != tree->nil) y->right->parent = x;

    y->parent = x->parent;
    if (x->parent == tree->nil) tree->root = y;
    else if (x == x->parent->right) x->parent->right = y;
    else x->parent->left = y;

    y->right = x;
    x->parent = y;
}

/* ---- 插入 ---- */

/* 插入修复：新节点 z 刚被染成红色插入完毕，修复「红红相邻」问题。
 * 循环不变式（CLRS Chapter 13.3）：z 是红色，且如果 z->parent 也是红色，
 * 则 z->parent 是唯一违反性质 4 的节点。每一轮要么把违规点往上推两层，
 * 要么做一次旋转+变色后彻底解决，循环结束。 */
static void insert_fixup(RBTree *tree, RBNode *z) {
    while (z->parent->color == RB_RED) {
        if (z->parent == z->parent->parent->left) {
            RBNode *uncle = z->parent->parent->right;
            if (uncle->color == RB_RED) {
                /* case 1：叔叔是红色 —— 父、叔变黑，祖父变红，
                 * 把「红红相邻」问题原样上推两层，继续在祖父处检查。 */
                z->parent->color = RB_BLACK;
                uncle->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    /* case 2：叔叔是黑色，z 是「之字形」（左-右）
                     * —— 先左旋父节点，把之字形拉直成一条线，
                     * 转化成 case 3 继续处理（z 变成新的 z->parent）。 */
                    z = z->parent;
                    left_rotate(tree, z);
                }
                /* case 3：叔叔是黑色，z 和父节点是一条线（左-左）
                 * —— 父变黑、祖父变红，再右旋祖父，彻底修复，循环结束。 */
                z->parent->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                right_rotate(tree, z->parent->parent);
            }
        } else {
            /* 镜像：z->parent 是祖父的右孩子，把上面的 left/right 全部对调 */
            RBNode *uncle = z->parent->parent->left;
            if (uncle->color == RB_RED) {
                z->parent->color = RB_BLACK;
                uncle->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->left) {
                    z = z->parent;
                    right_rotate(tree, z);
                }
                z->parent->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                left_rotate(tree, z->parent->parent);
            }
        }
    }
    tree->root->color = RB_BLACK; /* 性质 2：根恒为黑，循环里可能把根短暂染红 */
}

bool rb_insert(RBTree *tree, int key) {
    RBNode *y = tree->nil;
    RBNode *x = tree->root;
    while (x != tree->nil) {
        y = x;
        if (key == x->key) return false; /* 重复键：拒绝插入，树不变 */
        x = (key < x->key) ? x->left : x->right;
    }

    RBNode *z = malloc(sizeof *z);
    if (!z) { perror("malloc"); exit(1); }
    z->key = key;
    z->left = z->right = tree->nil;
    z->parent = y;
    z->color = RB_RED; /* 新节点先染红：不改变任何黑高，只可能违反性质 4 */

    if (y == tree->nil) tree->root = z;
    else if (key < y->key) y->left = z;
    else y->right = z;

    insert_fixup(tree, z);
    return true;
}

/* ---- 删除 ---- */

/* transplant：用子树 v 顶替子树 u 在其父节点里的位置（只管 parent 一侧的
 * 挂接，不管 v 自己的 left/right——那是调用者的责任）。 */
static void transplant(RBTree *tree, RBNode *u, RBNode *v) {
    if (u->parent == tree->nil) tree->root = v;
    else if (u == u->parent->left) u->parent->left = v;
    else u->parent->right = v;
    v->parent = u->parent; /* 即使 v 是 tree->nil 也要设置，删除修复要靠它找父节点 */
}

static RBNode *tree_minimum(const RBTree *tree, RBNode *x) {
    while (x->left != tree->nil) x = x->left;
    return x;
}

/* 删除修复：x 是「双黑」节点所在的位置（它比正常情况多背了一重黑色，
 * 用来补偿刚被删掉的那个黑色节点）。循环不变式：x 是黑色，但它所在路径的
 * 黑高比其他路径少 1（因为少算了 x 身上多背的那一重黑）。每一轮要么把
 * 双黑标记往上推一层，要么通过旋转/变色彻底吸收掉它，循环结束。 */
static void delete_fixup(RBTree *tree, RBNode *x) {
    while (x != tree->root && x->color == RB_BLACK) {
        if (x == x->parent->left) {
            RBNode *sibling = x->parent->right;
            if (sibling->color == RB_RED) {
                /* case 1：兄弟是红色 —— 兄弟不可能是「双黑」的最终吸收者
                 * （红色节点不能直接扛黑高债务），先变色+左旋父节点，
                 * 把一个黑色的侄子换成新兄弟，转化成 case 2/3/4 之一。 */
                sibling->color = RB_BLACK;
                x->parent->color = RB_RED;
                left_rotate(tree, x->parent);
                sibling = x->parent->right;
            }
            if (sibling->left->color == RB_BLACK && sibling->right->color == RB_BLACK) {
                /* case 2：兄弟黑色，且兄弟的两个孩子都是黑色
                 * —— 兄弟可以「借」一重黑色给 x 这条路径：兄弟变红，
                 * 双黑标记整体上移到父节点，继续在父节点处检查。 */
                sibling->color = RB_RED;
                x = x->parent;
            } else {
                if (sibling->right->color == RB_BLACK) {
                    /* case 3：兄弟黑色，近侧（left）孩子红、远侧（right）孩子黑
                     * —— 先右旋兄弟，把红色孩子转到远侧，转化成 case 4。 */
                    sibling->left->color = RB_BLACK;
                    sibling->color = RB_RED;
                    right_rotate(tree, sibling);
                    sibling = x->parent->right;
                }
                /* case 4：兄弟黑色，远侧（right）孩子红色
                 * —— 这是终点：左旋父节点把兄弟提上来顶替父节点的位置，
                 * 远侧红孩子转黑吸收掉双黑标记，x 变成 root 让循环终止。 */
                sibling->color = x->parent->color;
                x->parent->color = RB_BLACK;
                sibling->right->color = RB_BLACK;
                left_rotate(tree, x->parent);
                x = tree->root;
            }
        } else {
            /* 镜像：x 是右孩子，兄弟是 parent->left，left/right 全部对调 */
            RBNode *sibling = x->parent->left;
            if (sibling->color == RB_RED) {
                sibling->color = RB_BLACK;
                x->parent->color = RB_RED;
                right_rotate(tree, x->parent);
                sibling = x->parent->left;
            }
            if (sibling->right->color == RB_BLACK && sibling->left->color == RB_BLACK) {
                sibling->color = RB_RED;
                x = x->parent;
            } else {
                if (sibling->left->color == RB_BLACK) {
                    sibling->right->color = RB_BLACK;
                    sibling->color = RB_RED;
                    left_rotate(tree, sibling);
                    sibling = x->parent->left;
                }
                sibling->color = x->parent->color;
                x->parent->color = RB_BLACK;
                sibling->left->color = RB_BLACK;
                right_rotate(tree, x->parent);
                x = tree->root;
            }
        }
    }
    x->color = RB_BLACK; /* x 本来就是黑，若循环因 x==root 退出，这里是幂等的；
                           * 若因 x->color==RED 退出（“红黑双节点”吸收了双黑标记
                           * 的情况），这里把它染黑，一次性解决。 */
}

bool rb_delete(RBTree *tree, int key) {
    RBNode *z = tree->root;
    while (z != tree->nil && z->key != key) z = (key < z->key) ? z->left : z->right;
    if (z == tree->nil) return false; /* 键不存在：不做任何修改 */

    RBNode *y = z;                 /* y：真正被物理移出树的节点 */
    RBColor y_original_color = y->color;
    RBNode *x;                     /* x：顶替 y 原来位置的节点（可能是 tree->nil） */

    if (z->left == tree->nil) {
        x = z->right;
        transplant(tree, z, z->right);
    } else if (z->right == tree->nil) {
        x = z->left;
        transplant(tree, z, z->left);
    } else {
        /* z 有两个孩子：找后继 y（右子树最小值，因此 y 至多只有右孩子）。
         *
         * 注意这里用的**不是**「用 y 的 key 覆盖 z」那种写法。下面做的是
         * 指针拼接（CLRS 的做法）：把 y 从它原来的位置摘下来，接到 z 的
         * 位置上，最后 free(z)。也就是说，物理上被释放的节点是 z，而 y
         * 只是换了个位置——这和「复制 key 再删 y」在树形上等价，但对外
         * 部行为不同：
         *
         *   - 拼接：调用方手里已有的 RBNode* 永远对应同一个 key。
         *   - 复制 key：z 这个地址会突然变成另一个 key，外部指针失效得
         *     无声无息。
         *
         * y 继承 z 的颜色（y->color = z->color），所以 z 原位置的颜色结
         * 构不变；真正可能破坏黑高的是 y **原来**那个位置被 x 顶替，因此
         * 后面的 fixup 用的是 y_original_color 和 x，而不是 z 的颜色。 */
        y = tree_minimum(tree, z->right);
        y_original_color = y->color;
        x = y->right;
        if (y->parent == z) {
            /* y 是 z 的直接右孩子：transplant 会把 x->parent 设成 y，
             * 但 y 马上要被摘走顶替 z 的位置，所以这里手动纠正成 y，
             * 这样 delete_fixup 从 x 往上找 parent 时能找到正确的节点。 */
            x->parent = y;
        } else {
            transplant(tree, y, y->right);
            y->right = z->right;
            y->right->parent = y;
        }
        transplant(tree, z, y);
        y->left = z->left;
        y->left->parent = y;
        y->color = z->color;
    }
    free(z);

    if (y_original_color == RB_BLACK) delete_fixup(tree, x);
    return true;
}

/* ---- 统计 ---- */

static int count_rec(const RBTree *tree, const RBNode *x) {
    if (x == tree->nil) return 0;
    return 1 + count_rec(tree, x->left) + count_rec(tree, x->right);
}

int rb_count(const RBTree *tree) { return count_rec(tree, tree->root); }

int rb_black_height(const RBTree *tree) {
    int bh = 0;
    RBNode *x = tree->root;
    while (x != tree->nil) {
        if (x->color == RB_BLACK) bh++;
        x = x->left;
    }
    return bh;
}

/* ---- 性质检查 ---- */

typedef struct {
    char *buf;
    size_t size;
    bool failed;
} VerifyCtx;

static void vfail(VerifyCtx *ctx, const char *fmt, ...) {
    if (ctx->failed) return;
    ctx->failed = true;
    if (ctx->buf && ctx->size > 0) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(ctx->buf, ctx->size, fmt, args);
        va_end(args);
    }
}

/* 递归验证性质 4（无连续红色）+ 性质 5（黑高一致）+ BST 有序性，
 * 通过 out_bh 把本子树的黑高传给调用者用于左右比较。 */
static void verify_rec(const RBTree *tree, const RBNode *x, const int *lo, const int *hi,
                        int *out_bh, VerifyCtx *ctx) {
    if (ctx->failed) return;
    if (x == tree->nil) { *out_bh = 0; return; }

    if (lo && x->key <= *lo) { vfail(ctx, "BST 性质被破坏：节点 %d 应该大于下界 %d", x->key, *lo); return; }
    if (hi && x->key >= *hi) { vfail(ctx, "BST 性质被破坏：节点 %d 应该小于上界 %d", x->key, *hi); return; }

    if (x->color == RB_RED) {
        if (x->left->color == RB_RED || x->right->color == RB_RED) {
            vfail(ctx, "性质 4 被破坏：红色节点 %d 有红色孩子", x->key);
            return;
        }
    }

    int lbh, rbh;
    /* 父指针一致性。原来的 verify 只检查了 BST 序、颜色和黑高，这三项
     * 全是**向下**看的（只读 left/right），所以一个被写坏的 parent 指针
     * 可以完整通过校验——实测把某个节点的 parent 改成错的节点，
     * rb_verify 依旧返回 true。
     *
     * 这对红黑树是个要命的漏洞：旋转和 delete_fixup 全靠 parent 往上
     * 走，parent 错了会导致 fixup 走到树的另一个分支上去，而校验器却
     * 说树是好的。既然 verify 的职责是"发现结构被破坏"，向上的那一半
     * 链接就必须一起查。 */
    if (x->left != tree->nil && x->left->parent != x)
        vfail(ctx, "node %d: left child %d has wrong parent pointer", x->key, x->left->key);
    if (x->right != tree->nil && x->right->parent != x)
        vfail(ctx, "node %d: right child %d has wrong parent pointer", x->key, x->right->key);

    verify_rec(tree, x->left, lo, &x->key, &lbh, ctx);
    if (ctx->failed) return;
    verify_rec(tree, x->right, &x->key, hi, &rbh, ctx);
    if (ctx->failed) return;

    if (lbh != rbh) {
        vfail(ctx, "性质 5 被破坏：节点 %d 的左子树黑高 %d != 右子树黑高 %d", x->key, lbh, rbh);
        return;
    }
    *out_bh = lbh + (x->color == RB_BLACK ? 1 : 0);
}

bool rb_verify(const RBTree *tree, char *err_buf, size_t err_buf_size) {
    if (err_buf && err_buf_size > 0) err_buf[0] = '\0';

    if (tree->nil->color != RB_BLACK) {
        if (err_buf) snprintf(err_buf, err_buf_size, "性质 3 被破坏：哨兵 NIL 不是黑色");
        return false;
    }
    if (tree->root == tree->nil) return true; /* 空树trivially满足全部性质 */
    if (tree->root->color != RB_BLACK) {
        if (err_buf) snprintf(err_buf, err_buf_size, "性质 2 被破坏：根节点 %d 是红色", tree->root->key);
        return false;
    }

    VerifyCtx ctx = { err_buf, err_buf_size, false };
    int bh;
    /* 根的 parent 必须是哨兵。verify_rec 只能检查"孩子的 parent 指回自
     * 己"，根没有父节点，所以这一条只能在顶层查。insert_fixup 里
     * `while (z->parent->color == RB_RED)` 正是靠 nil 是黑色才能在根处
     * 自然终止——如果 root->parent 指向某个真实的红节点，这个循环会
     * 越过根继续往上走。 */
    if (tree->root != tree->nil && tree->root->parent != tree->nil)
        vfail(&ctx, "root %d: parent is not the nil sentinel", tree->root->key);

    verify_rec(tree, tree->root, NULL, NULL, &bh, &ctx);
    return !ctx.failed;
}

/* ---- 打印 ---- */

static void print_rec(const RBTree *tree, const RBNode *x, int depth) {
    if (x == tree->nil) return;
    print_rec(tree, x->right, depth + 1);
    for (int i = 0; i < depth; i++) printf("    ");
    printf("%s(%d)\n", x->color == RB_RED ? "R" : "B", x->key);
    print_rec(tree, x->left, depth + 1);
}

void rb_print(const RBTree *tree) {
    if (tree->root == tree->nil) { printf("  (empty tree)\n"); return; }
    print_rec(tree, tree->root, 0);
}
