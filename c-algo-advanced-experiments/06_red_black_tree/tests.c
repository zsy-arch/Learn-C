/* 红黑树测试套件：覆盖空树/单 key/重复插入/删除不存在的 key、
 * 插入修复的 3 种情形（及镜像）、删除修复的 4 种情形（及镜像）、
 * 大规模顺序与随机压力测试。每次 insert/delete 之后都调用一次
 * verify_ok()，而不是只在测试函数末尾验证——这是从 04_b_tree
 * 继承下来的方法论：只有每一步都验证，才能保证「发现问题的那一刻」
 * 就是「问题实际发生的那一刻」。 */
#include "rbtree.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        printf("  FAILED CHECK: %s (line %d)\n", #cond, __LINE__); \
        return false; \
    } \
} while (0)

#define RUN(fn) do { \
    printf("[%s] %s\n", (fn()) ? "PASS" : "FAIL", #fn); \
    if (0) {} \
} while (0)

/* RUN 上面写成 do{}while(0) 但真正的通过/失败计数要在这里手动做，
 * 因为 fn() 只能调用一次（副作用是修改树），不能调用两次分别判断和计数。 */
#undef RUN
#define RUN(fn) do { \
    bool _ok = fn(); \
    printf("[%s] %s\n", _ok ? "PASS" : "FAIL", #fn); \
    if (_ok) g_pass++; else g_fail++; \
} while (0)

static bool verify_ok(const RBTree *t, const char *ctx) {
    char err[256];
    if (!rb_verify(t, err, sizeof err)) {
        printf("  VERIFY FAILED (%s): %s\n", ctx, err);
        return false;
    }
    return true;
}

/* xorshift32 必须用 uint32_t，不能用 unsigned int。
 *
 * 这个算法的周期(2^32-1)和位移常数(13/17/5)是在「精确 32 位、溢出即
 * 截断」的前提下推导出来的。C 标准只保证 unsigned int 至少 16 位，
 * 宽度是实现定义的：在主流平台上它恰好是 32 位，所以代码"能跑"，但
 * 这是平台巧合而非语言保证。换到 unsigned int 为 64 位的实现上，
 * x ^= x << 13 不再回卷，序列立刻退化。
 *
 * 另外 state 不能是 0：0 是 xorshift 的不动点（三次异或位移后仍是 0），
 * 会产生一个恒为 0 的"随机"序列。这里用 Knuth 的黄金比例常数兜底。 */
static uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    if (x == 0) x = 0x9E3779B9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *state = x;
    return x;
}

/* ---------- 基础情形 ---------- */

static bool test_empty_tree_search_and_delete(void) {
    RBTree t = rb_create();
    CHECK(!rb_search(&t, 42));
    CHECK(!rb_delete(&t, 42));
    CHECK(rb_count(&t) == 0);
    CHECK(rb_black_height(&t) == 0);
    CHECK(verify_ok(&t, "empty tree"));
    rb_destroy(&t);
    return true;
}

static bool test_single_key_insert_delete(void) {
    RBTree t = rb_create();
    CHECK(rb_insert(&t, 7));
    CHECK(verify_ok(&t, "single insert"));
    CHECK(rb_search(&t, 7));
    CHECK(t.root->color == RB_BLACK); /* 性质 2：根恒为黑 */
    CHECK(rb_count(&t) == 1);

    CHECK(rb_delete(&t, 7));
    CHECK(verify_ok(&t, "single delete"));
    CHECK(!rb_search(&t, 7));
    CHECK(rb_count(&t) == 0);
    CHECK(t.root == t.nil);
    rb_destroy(&t);
    return true;
}

static bool test_duplicate_insert_rejected(void) {
    RBTree t = rb_create();
    CHECK(rb_insert(&t, 10));
    CHECK(!rb_insert(&t, 10)); /* 重复键：拒绝，返回 false */
    CHECK(rb_count(&t) == 1);  /* 树没有被改变 */
    CHECK(verify_ok(&t, "after rejected duplicate"));
    rb_destroy(&t);
    return true;
}

static bool test_delete_nonexistent_is_noop(void) {
    RBTree t = rb_create();
    int vals[] = {50, 30, 70, 20, 40};
    for (size_t i = 0; i < 5; i++) CHECK(rb_insert(&t, vals[i]));
    CHECK(!rb_delete(&t, 999)); /* 不存在：返回 false */
    CHECK(rb_count(&t) == 5);   /* 树没有被改变 */
    CHECK(verify_ok(&t, "after no-op delete"));
    rb_destroy(&t);
    return true;
}

/* ---------- 插入修复：case 1（叔叔红） ---------- */

static bool test_insert_fixup_case1_red_uncle(void) {
    /* 插入 10,5,15 后：root=B(10)，两个孩子 R(5)/R(15)（叔叔关系）。
     * 再插入 1：1 挂在 5 的左边，1 是红，父 5 是红 -> 触发修复。
     * 5 的叔叔是 15，颜色红 -> case 1：父叔变黑，祖父(10)变红，
     * 违规点上推到 10，但 10 是 root，循环结束后 root 强制变黑。 */
    RBTree t = rb_create();
    int build[] = {10, 5, 15};
    for (size_t i = 0; i < 3; i++) CHECK(rb_insert(&t, build[i]));
    CHECK(verify_ok(&t, "build case1 base"));
    CHECK(t.root->key == 10 && t.root->color == RB_BLACK);
    CHECK(t.root->left->key == 5 && t.root->left->color == RB_RED);
    CHECK(t.root->right->key == 15 && t.root->right->color == RB_RED);

    CHECK(rb_insert(&t, 1));
    CHECK(verify_ok(&t, "after case1 red-uncle fixup"));
    CHECK(t.root->key == 10 && t.root->color == RB_BLACK); /* root 保持不变，只是重新染色 */
    CHECK(t.root->left->color == RB_BLACK && t.root->right->color == RB_BLACK);
    CHECK(rb_search(&t, 1) && rb_search(&t, 5) && rb_search(&t, 10) && rb_search(&t, 15));
    rb_destroy(&t);
    return true;
}

/* ---------- 插入修复：case 2+3（叔叔黑，之字形 / 一条线） ---------- */

static bool test_insert_fixup_case3_black_uncle_line(void) {
    /* 插入 10,5（此时叔叔是 nil，视为黑）。再插入 1：
     * 1 挂在 5 的左边，1-5-10 是一条左-左直线，叔叔(nil)黑
     * -> 直接走 case 3：父(5)变黑，祖父(10)变红，右旋祖父。
     * 修复后 5 顶替 10 成为新 root。 */
    RBTree t = rb_create();
    CHECK(rb_insert(&t, 10));
    CHECK(rb_insert(&t, 5));
    CHECK(verify_ok(&t, "build case3 base"));

    CHECK(rb_insert(&t, 1));
    CHECK(verify_ok(&t, "after case3 black-uncle line fixup"));
    CHECK(t.root->key == 5 && t.root->color == RB_BLACK); /* 5 右旋后顶替成新root */
    CHECK(t.root->left->key == 1 && t.root->right->key == 10);
    rb_destroy(&t);
    return true;
}

static bool test_insert_fixup_case2_black_uncle_zigzag(void) {
    /* 插入 10,5（叔叔 nil 黑）。再插入 7：7 挂在 5 的右边，
     * 5-7 相对 10 是左-右之字形，叔叔黑 -> case 2：先左旋 5，
     * 把之字形拉直成 7-5 一条线，转化成 case 3 继续处理。 */
    RBTree t = rb_create();
    CHECK(rb_insert(&t, 10));
    CHECK(rb_insert(&t, 5));
    CHECK(verify_ok(&t, "build case2 base"));

    CHECK(rb_insert(&t, 7));
    CHECK(verify_ok(&t, "after case2 zigzag then case3 fixup"));
    CHECK(t.root->key == 7 && t.root->color == RB_BLACK); /* 7 转正后顶替成新root */
    CHECK(t.root->left->key == 5 && t.root->right->key == 10);
    rb_destroy(&t);
    return true;
}

/* ---------- 删除修复：case 1（兄弟红） ---------- */

static bool test_delete_fixup_case1_red_sibling(void) {
    /* 用 dump 探针实测确认：插入 2,1,4,3,6,5,7 之后的真实结构是
     *   root=B(2)，root.L=B(1)（叶子），root.R=R(4)，
     *   root.R.L=B(3)（叶子），root.R.R=B(6)[孩子 R(5)/R(7)]。
     * 删除黑色叶子 1：x 落在 root 的左孩子位置（nil），
     * x->parent=root(2)，兄弟是 root.R=R(4)——兄弟是红色，命中 case 1：
     * 先变色+左旋 root，把红色的 4 提上来顶替 2 的位置，
     * 4 的原左孩子 2 变成新的左子树根，兄弟换成 2 原来的右孩子 3（黑色）。
     * 旋转之后兄弟 3 的两个孩子都是 nil（黑），继续命中 case 2：
     * 3 变红，双黑标记上移到新 root(4)，循环因 x==root 结束。 */
    RBTree t = rb_create();
    int build[] = {2, 1, 4, 3, 6, 5, 7};
    for (size_t i = 0; i < 7; i++) CHECK(rb_insert(&t, build[i]));
    CHECK(verify_ok(&t, "build case1-delete base"));
    CHECK(t.root->key == 2 && t.root->color == RB_BLACK);
    CHECK(t.root->left->key == 1 && t.root->left->color == RB_BLACK);
    CHECK(t.root->right->key == 4 && t.root->right->color == RB_RED);

    CHECK(rb_delete(&t, 1));
    CHECK(verify_ok(&t, "after case1 red-sibling delete fixup (falls through to case2)"));
    CHECK(!rb_search(&t, 1));
    /* 4 顶替成新 root，2 变成它的左孩子，2 原来的右孩子 3 被染红顶替双黑 */
    CHECK(t.root->key == 4 && t.root->color == RB_BLACK);
    CHECK(t.root->left->key == 2 && t.root->left->color == RB_BLACK);
    CHECK(t.root->left->right->key == 3 && t.root->left->right->color == RB_RED);
    CHECK(rb_search(&t, 2) && rb_search(&t, 3) && rb_search(&t, 4));
    CHECK(rb_search(&t, 5) && rb_search(&t, 6) && rb_search(&t, 7));
    rb_destroy(&t);
    return true;
}

/* ---------- 删除修复：case 2（兄弟黑，兄弟两孩子都黑） ---------- */

static bool test_delete_fixup_case2_black_sibling_black_nephews(void) {
    /* 用 dump 探针实测确认：插入 20,10,30,5,15,25,35 之后，依次删除
     * 5,15,35,25（全部是红色叶子，删除时 y_original_color==RED，
     * 不会触发 delete_fixup），会让 10 和 30 都退化成普通黑色叶子：
     *   root=B(20)，root.L=B(10)（叶子），root.R=B(30)（叶子）。
     * 此时删除 10：x 落在 root 的左孩子位置（nil），兄弟是
     * root.R=B(30)，30 的两个孩子都是 nil（黑）—— 命中 case 2：
     * 兄弟变红，双黑标记原样上移到 root，因为 x 变成 tree->root
     * 循环立刻结束（循环条件 x != tree->root 不再满足）。 */
    RBTree t = rb_create();
    int build[] = {20, 10, 30, 5, 15, 25, 35};
    for (size_t i = 0; i < 7; i++) CHECK(rb_insert(&t, build[i]));
    CHECK(verify_ok(&t, "build case2-delete base (before flattening)"));
    CHECK(rb_delete(&t, 5));
    CHECK(rb_delete(&t, 15));
    CHECK(rb_delete(&t, 35));
    CHECK(rb_delete(&t, 25));
    CHECK(verify_ok(&t, "after flattening 10 and 30 into plain black leaves"));
    CHECK(t.root->key == 20 && t.root->color == RB_BLACK);
    CHECK(t.root->left->key == 10 && t.root->left->color == RB_BLACK);
    CHECK(t.root->left->left == t.nil && t.root->left->right == t.nil);
    CHECK(t.root->right->key == 30 && t.root->right->color == RB_BLACK);
    CHECK(t.root->right->left == t.nil && t.root->right->right == t.nil);

    CHECK(rb_delete(&t, 10));
    CHECK(verify_ok(&t, "after case2 black-sibling-black-nephews delete fixup"));
    CHECK(!rb_search(&t, 10));
    CHECK(rb_search(&t, 20) && rb_search(&t, 30));
    /* 双黑上移到 root 后被直接吸收：root 唯一孩子 30 应变红 */
    CHECK(t.root->key == 20 && t.root->right->key == 30 && t.root->right->color == RB_RED);
    rb_destroy(&t);
    return true;
}

/* ---------- 删除修复：case 3+4（兄弟黑，近侄红远侄黑 -> 远侄红） ---------- */

static bool test_delete_fixup_case4_black_sibling_red_far_nephew(void) {
    /* root=B(20)，左孩子 B(10)，右孩子 B(35)，35 的右孩子 R(40)。
     * 删掉 10：兄弟是 B(35)，35 的远侧孩子(right=40)是红
     * -> 直接命中 case 4：左旋 root，35 顶替 20 的位置，40 转黑。 */
    RBTree t = rb_create();
    int build[] = {20, 10, 35, 40};
    for (size_t i = 0; i < 4; i++) CHECK(rb_insert(&t, build[i]));
    CHECK(verify_ok(&t, "build case4-delete base"));
    CHECK(t.root->right->key == 35 && t.root->right->right->key == 40);
    CHECK(t.root->right->right->color == RB_RED);

    CHECK(rb_delete(&t, 10));
    CHECK(verify_ok(&t, "after case4 red-far-nephew delete fixup"));
    CHECK(!rb_search(&t, 10));
    CHECK(t.root->key == 35 && t.root->color == RB_BLACK); /* 35 顶替成新root */
    rb_destroy(&t);
    return true;
}

static bool test_delete_fixup_case3_then_case4_red_near_nephew(void) {
    /* root=B(20)，左孩子 B(10)，右孩子 B(35)，35 的左孩子 R(32)（近侧红，远侧黑）。
     * 删掉 10：兄弟 B(35) 近侧孩子(left=32)红、远侧孩子(right=nil)黑
     * -> case 3：先右旋 35 把红孩子转到远侧，32 顶替 35 的位置，
     * 转化成 case 4 继续，最终 32 顶替 root 的位置。 */
    RBTree t = rb_create();
    int build[] = {20, 10, 35, 32};
    for (size_t i = 0; i < 4; i++) CHECK(rb_insert(&t, build[i]));
    CHECK(verify_ok(&t, "build case3-then-case4-delete base"));
    CHECK(t.root->right->key == 35 && t.root->right->left->key == 32);
    CHECK(t.root->right->left->color == RB_RED);

    CHECK(rb_delete(&t, 10));
    CHECK(verify_ok(&t, "after case3-then-case4 red-near-nephew delete fixup"));
    CHECK(!rb_search(&t, 10));
    CHECK(t.root->key == 32 && t.root->color == RB_BLACK); /* 32 转正后顶替成新root */
    CHECK(t.root->right->key == 35);
    rb_destroy(&t);
    return true;
}

/* ---------- 删除：两个孩子都存在，需要后继替换 ---------- */

static bool test_delete_two_children_successor_replace(void) {
    RBTree t = rb_create();
    int build[] = {50, 30, 70, 20, 40, 60, 80};
    for (size_t i = 0; i < 7; i++) CHECK(rb_insert(&t, build[i]));
    CHECK(verify_ok(&t, "build two-children-delete base"));
    CHECK(t.root->key == 50 && t.root->left != t.nil && t.root->right != t.nil);

    /* 删 30：它有两个孩子(20,40)，后继是 40（右子树最小值） */
    CHECK(rb_delete(&t, 30));
    CHECK(verify_ok(&t, "after deleting internal node with two children"));
    CHECK(!rb_search(&t, 30));
    CHECK(rb_search(&t, 20) && rb_search(&t, 40) && rb_search(&t, 50));
    CHECK(rb_search(&t, 60) && rb_search(&t, 70) && rb_search(&t, 80));
    rb_destroy(&t);
    return true;
}

/* ---------- 删除根节点：叶子 / 恰一个孩子 / 两个孩子 三种结构变体 ---------- */

static bool test_delete_root_as_leaf(void) {
    RBTree t = rb_create();
    CHECK(rb_insert(&t, 42));
    CHECK(t.root->key == 42 && t.root->left == t.nil && t.root->right == t.nil);
    CHECK(rb_delete(&t, 42));
    CHECK(verify_ok(&t, "after deleting root-as-leaf"));
    CHECK(t.root == t.nil);
    CHECK(rb_count(&t) == 0);
    rb_destroy(&t);
    return true;
}

static bool test_delete_root_with_one_child(void) {
    /* 插入 10,20：10 是黑色根，20 是它唯一的红色右孩子。
     * 删除根 10：transplant 直接把 20 提升为新根，
     * delete_fixup 结尾把新根强制染黑（性质 2）。 */
    RBTree t = rb_create();
    CHECK(rb_insert(&t, 10));
    CHECK(rb_insert(&t, 20));
    CHECK(t.root->key == 10 && t.root->right->key == 20 && t.root->left == t.nil);
    CHECK(rb_delete(&t, 10));
    CHECK(verify_ok(&t, "after deleting root-with-one-child"));
    CHECK(t.root->key == 20 && t.root->color == RB_BLACK);
    CHECK(rb_count(&t) == 1);
    rb_destroy(&t);
    return true;
}

static bool test_delete_root_with_two_children(void) {
    /* 插入 50,30,70,20,40,60,80，删除有两个孩子的根 50：
     * 后继是 60（右子树最小值），60 顶替成为新根并保持黑色。 */
    RBTree t = rb_create();
    int build[] = {50, 30, 70, 20, 40, 60, 80};
    for (size_t i = 0; i < 7; i++) CHECK(rb_insert(&t, build[i]));
    CHECK(t.root->key == 50 && t.root->left != t.nil && t.root->right != t.nil);
    CHECK(rb_delete(&t, 50));
    CHECK(verify_ok(&t, "after deleting root-with-two-children"));
    CHECK(t.root->key == 60 && t.root->color == RB_BLACK);
    CHECK(!rb_search(&t, 50));
    CHECK(rb_count(&t) == 6);
    rb_destroy(&t);
    return true;
}

/* ---------- 大规模顺序 + 逆序删除 ---------- */

static bool test_large_sequential_then_reverse_delete(void) {
    RBTree t = rb_create();
    int n = 500;
    for (int i = 0; i < n; i++) {
        CHECK(rb_insert(&t, i));
        CHECK(verify_ok(&t, "sequential insert"));
    }
    CHECK(rb_count(&t) == n);
    for (int i = n - 1; i >= 0; i--) {
        CHECK(rb_delete(&t, i));
        CHECK(verify_ok(&t, "reverse delete"));
    }
    CHECK(rb_count(&t) == 0);
    CHECK(t.root == t.nil);
    rb_destroy(&t);
    return true;
}

/* ---------- 随机压力测试 ---------- */

static bool random_stress(uint32_t seed, int n, int value_range) {
    RBTree t = rb_create();
    int *vals = malloc(sizeof(int) * (size_t)n);
    if (!vals) { perror("malloc"); exit(1); }
    for (int i = 0; i < n; i++) vals[i] = (int)(xorshift32(&seed) % (unsigned)value_range);

    char err[256];
    for (int i = 0; i < n; i++) {
        rb_insert(&t, vals[i]);
        if (!rb_verify(&t, err, sizeof err)) {
            printf("  VERIFY FAILED after insert %d (i=%d): %s\n", vals[i], i, err);
            free(vals);
            return false;
        }
    }

    int *order = malloc(sizeof(int) * (size_t)n);
    if (!order) { perror("malloc"); exit(1); }
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = n - 1; i > 0; i--) {
        int j = (int)(xorshift32(&seed) % (unsigned)(i + 1));
        int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
    }
    for (int i = 0; i < n; i++) {
        rb_delete(&t, vals[order[i]]);
        if (!rb_verify(&t, err, sizeof err)) {
            printf("  VERIFY FAILED after delete %d (i=%d): %s\n", vals[order[i]], i, err);
            free(vals); free(order);
            return false;
        }
    }
    bool count_ok = (rb_count(&t) == 0) && (t.root == t.nil);
    free(vals);
    free(order);
    rb_destroy(&t);
    return count_ok;
}

static bool test_random_stress_dense_1000(void) {
    /* value_range 很小(500)，n=1000：大量重复键会被拒绝插入，
     * 逼着树在「键空间稠密」的情况下反复插入/删除同一批值。 */
    return random_stress(111u, 1000, 500);
}

static bool test_random_stress_sparse_1500(void) {
    /* value_range 很大(1000000)，几乎不会有重复键，
     * 逼着树处理「键空间稀疏」下的大规模顺序性插入模式。 */
    return random_stress(2026u, 1500, 1000000);
}

static bool test_random_stress_wide_2000(void) {
    return random_stress(999999u, 2000, 100000);
}

/* ---------- 校验器自身的覆盖度 ---------- */

/* 这个测试检查的不是树，而是 rb_verify 本身。
 *
 * 原来的 verify 只做三件事：BST 有序性、性质 4、性质 5。这三项全是
 * 沿 left/right 往下走的，于是一个写坏的 parent 指针能完整通过校验
 * ——修复前这个测试里的 rb_verify 返回 true。但 left_rotate /
 * delete_fixup 全靠 parent 往上走，parent 错了 fixup 就会跑到树的另
 * 一个分支上，而校验器还在说"树是好的"。一个不能发现结构损坏的校验
 * 器，比没有校验器更坏：它给出的是虚假的安全感。
 *
 * 因为 RBNode 在头文件里是完全公开的，测试可以直接把 parent 改坏，
 * 这里就故意这么做——改坏、断言被抓到、再改回去。 */
static RBNode *find_node_for_corruption(RBTree *t, int key) {
    RBNode *x = t->root;
    while (x != t->nil) {
        if (key == x->key) return x;
        x = key < x->key ? x->left : x->right;
    }
    return NULL;
}

static bool test_verify_detects_corrupt_parent_pointer(void) {
    RBTree t = rb_create();
    for (int i = 1; i <= 15; i++) CHECK(rb_insert(&t, i * 10));
    CHECK(verify_ok(&t, "corrupt-parent 用例的初始树"));

    RBNode *victim = find_node_for_corruption(&t, 50);
    RBNode *other  = find_node_for_corruption(&t, 120);
    CHECK(victim != NULL);
    CHECK(other != NULL);
    CHECK(victim != other);

    /* 孩子的 parent 指向了一个不是自己父亲的真实节点 */
    RBNode *saved = victim->parent;
    victim->parent = other;
    CHECK(!rb_verify(&t, NULL, 0));
    victim->parent = saved;
    CHECK(verify_ok(&t, "恢复 victim->parent 之后"));

    /* 根的 parent 不是哨兵：insert_fixup 的 while 循环靠"nil 是黑色"
     * 在根处终止，这里一旦指向真实红节点，循环会越过根继续往上。 */
    saved = t.root->parent;
    t.root->parent = other;
    CHECK(!rb_verify(&t, NULL, 0));
    t.root->parent = saved;
    CHECK(verify_ok(&t, "恢复 root->parent 之后"));

    rb_destroy(&t);
    return true;
}

/* rb_destroy 之后 root 和 nil 都必须是 NULL。
 * 修复前的顺序是 `root = nil; free(nil); nil = NULL;`，于是销毁结束后
 * nil == NULL 而 root 指向刚被释放的内存——"清理了一半"比完全不清理
 * 更危险，因为 nil == NULL 看起来像个可用的哨兵判断，会误导调用方以
 * 为这个 tree 处于某种可检测的已销毁状态。实际后果是 rb_count 会拿
 * root 去和已释放的 nil 比较（UB），二次 rb_destroy 则是 double free。 */
static bool test_destroy_clears_both_pointers(void) {
    RBTree t = rb_create();
    for (int i = 0; i < 32; i++) CHECK(rb_insert(&t, i));
    CHECK(verify_ok(&t, "destroy 用例的初始树"));

    rb_destroy(&t);
    CHECK(t.root == NULL);
    CHECK(t.nil == NULL);
    CHECK(rb_count(&t) == 0);          /* 不再读已释放的 nil */
    CHECK(rb_black_height(&t) == 0);

    rb_destroy(&t);                    /* 幂等：free(NULL) 是 no-op */
    CHECK(t.root == NULL);
    CHECK(t.nil == NULL);
    return true;
}

int main(void) {
    printf("========== 红黑树测试套件 ==========\n");
    RUN(test_empty_tree_search_and_delete);
    RUN(test_single_key_insert_delete);
    RUN(test_duplicate_insert_rejected);
    RUN(test_delete_nonexistent_is_noop);
    RUN(test_insert_fixup_case1_red_uncle);
    RUN(test_insert_fixup_case3_black_uncle_line);
    RUN(test_insert_fixup_case2_black_uncle_zigzag);
    RUN(test_delete_fixup_case1_red_sibling);
    RUN(test_delete_fixup_case2_black_sibling_black_nephews);
    RUN(test_delete_fixup_case4_black_sibling_red_far_nephew);
    RUN(test_delete_fixup_case3_then_case4_red_near_nephew);
    RUN(test_delete_two_children_successor_replace);
    RUN(test_delete_root_as_leaf);
    RUN(test_delete_root_with_one_child);
    RUN(test_delete_root_with_two_children);
    RUN(test_large_sequential_then_reverse_delete);
    RUN(test_random_stress_dense_1000);
    RUN(test_random_stress_sparse_1500);
    RUN(test_random_stress_wide_2000);
    RUN(test_verify_detects_corrupt_parent_pointer);
    RUN(test_destroy_clears_both_pointers);

    printf("\n========== 汇总 ==========\n");
    printf("通过: %d, 失败: %d, 总计: %d\n", g_pass, g_fail, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}
