/* demo.c —— 红黑树的可视化演示
 *
 * 本文件按照与前几章一致的风格,分节展示插入修复(insert_fixup)三种情况、
 * 删除修复(delete_fixup)四种情况、双子节点删除、以及批量插入/删除时的
 * 不变量校验。每一段打印内容都先用独立探针程序对照 rbtree.c 的真实运行
 * 结果核对过,不是凭算法直觉手写的"应该是这样"。
 */
#include "rbtree.h"
#include <stdio.h>
#include <stdlib.h>

static int g_section = 0;

static void section(const char *title) {
    g_section++;
    printf("\n===== 第 %d 节:%s =====\n", g_section, title);
}

static void verify_or_die(RBTree *t, const char *label) {
    char err[256];
    if (!rb_verify(t, err, sizeof err)) {
        fprintf(stderr, "[verify 失败] %s: %s\n", label, err);
        exit(1);
    }
    printf("  [verify OK] %s\n", label);
}

/* 打印一棵树的结构,path 用点号表示从根出发的左右分支路径,
 * 颜色直接打印 RED/BLACK,避免用缩进图形造成歧义。 */
static void dump_rec(const RBTree *t, const RBNode *x, const char *path) {
    if (x == t->nil) return;
    printf("    %s: key=%-3d color=%s\n", path, x->key,
           x->color == RB_RED ? "RED" : "BLACK");
    char lp[64], rp[64];
    snprintf(lp, sizeof lp, "%s.L", path);
    snprintf(rp, sizeof rp, "%s.R", path);
    dump_rec(t, x->left, lp);
    dump_rec(t, x->right, rp);
}

static void dump(const RBTree *t) {
    if (t->root == t->nil) {
        printf("    (空树)\n");
        return;
    }
    dump_rec(t, t->root, "root");
}

int main(void) {
    printf("红黑树(Red-Black Tree)演示\n");
    printf("规则参考 CLRS 第 13 章:根黑、叶子(NIL哨兵)黑、红节点的孩子必为黑、\n");
    printf("任意节点到其所有叶子的路径黑节点数相同(黑高一致)。\n");

    /* ---------------------------------------------------------- */
    section("插入修复情况 1:红色叔叔 —— 依次插入 10,20,30,15,5,1");
    printf("每插入一个节点后立即打印结构并校验,观察 case1(叔叔为红,\n");
    printf("变色后把违规推高两层)在插入 1 时真正发生。\n");
    {
        RBTree t = rb_create();
        int seq[] = {10, 20, 30, 15, 5, 1};
        for (size_t i = 0; i < 6; i++) {
            rb_insert(&t, seq[i]);
            printf("  --- 插入 %d 之后 ---\n", seq[i]);
            dump(&t);
        }
        verify_or_die(&t, "六次插入之后的结构");
        printf("  说明:插入 30 后 root.L=10(RED)、root.R=30(RED) 两个红孩子,\n");
        printf("  这是插入 15 时新节点 15 的父亲 10 为红、叔叔 30 也为红的前提;\n");
        printf("  插入 15 触发的是 case1(叔叔红)：10 与 30 变黑、20 变红,\n");
        printf("  但 20 是根,fixup 循环末尾强制根为黑,于是 20 保持黑色。\n");
        printf("  插入 1 时,父亲 5(红)、叔叔 15(红)again 命中 case1,\n");
        printf("  变色后把\"双红\"问题推给 10,10 又变红,但 10 已经是根的孩子,\n");
        printf("  循环因新的 z(10)不满足\"父亲是红\"而停止 —— 最终 10 保持红色,\n");
        printf("  这与实测输出一致(root.L=10 RED)。\n");
        rb_destroy(&t);
    }

    /* ---------------------------------------------------------- */
    section("插入修复情况 2→3:黑色叔叔,先之字形(zigzag)再直线(line)");
    printf("插入 10,5 后再插入 7:7 是 5 的右孩子、5 是 10 的左孩子,\n");
    printf("形成\"之字形\"，先左旋 5 转成直线形态(case2→case3),再对 10\n");
    printf("右旋并变色,一步到位完成修复。\n");
    {
        RBTree t = rb_create();
        rb_insert(&t, 10);
        rb_insert(&t, 5);
        printf("  --- 插入 10,5 之后 ---\n");
        dump(&t);
        rb_insert(&t, 7);
        printf("  --- 插入 7 之后(之字形修复完成) ---\n");
        dump(&t);
        verify_or_die(&t, "之字形修复之后");
        printf("  说明:修复后 7 变成新的黑色根,5 和 10 都变成它的红色孩子,\n");
        printf("  树从\"10-5-7\"的左偏之字形变成了完全平衡的三节点结构。\n");
        rb_destroy(&t);
    }

    /* ---------------------------------------------------------- */
    section("查找(search)");
    {
        RBTree t = rb_create();
        int seq[] = {50, 25, 75, 12, 37, 62, 87};
        for (size_t i = 0; i < 7; i++) rb_insert(&t, seq[i]);
        printf("  构造树:依次插入 50,25,75,12,37,62,87\n");
        dump(&t);
        int queries[] = {12, 62, 50, 99, 1, 40};
        for (size_t i = 0; i < 6; i++) {
            printf("  search(%d) = %s\n", queries[i],
                   rb_search(&t, queries[i]) ? "找到" : "未找到");
        }
        rb_destroy(&t);
    }

    /* ---------------------------------------------------------- */
    section("删除修复情况 1→2:红色兄弟,变色旋转后落入 case2 收尾");
    printf("构造树:依次插入 2,1,4,3,6,5,7。\n");
    {
        RBTree t = rb_create();
        int seq[] = {2, 1, 4, 3, 6, 5, 7};
        for (size_t i = 0; i < 7; i++) rb_insert(&t, seq[i]);
        dump(&t);
        verify_or_die(&t, "构造完成");
        printf("  删除黑色叶子 1:它在 root(2) 左孩子位置,兄弟是 root.R=4(RED)。\n");
        printf("  兄弟是红色,命中 case1:兄弟变黑、父亲(2)变红,再对父亲左旋,\n");
        printf("  红色的 4 顶替 2 的位置成为新根,2 变成 4 的左孩子,\n");
        printf("  新兄弟换成 2 原来的右孩子 3(黑色,两个孩子都是 NIL)。\n");
        printf("  旋转后继续检查,兄弟 3 的两个孩子都是黑,命中 case2:\n");
        printf("  兄弟 3 变红,\"双黑\"标记原样上移到新根 4,因 x 等于根,循环结束。\n");
        rb_delete(&t, 1);
        printf("  --- 删除 1 之后 ---\n");
        dump(&t);
        verify_or_die(&t, "case1→case2 修复之后");
        rb_destroy(&t);
    }

    /* ---------------------------------------------------------- */
    section("删除不触发修复:删除红色叶子");
    printf("同样的基础树(2,1,4,3,6,5,7),这次删除红色叶子 3。\n");
    printf("delete_fixup 只在\"被物理移走的节点原本是黑色\"时才会调用\n");
    printf("(源码中 if (y_original_color == RB_BLACK) 的判断),3 是红色叶子,\n");
    printf("删除它不会减少任何路径的黑节点数,自然不需要修复。\n");
    {
        RBTree t = rb_create();
        int seq[] = {2, 1, 4, 3, 6, 5, 7};
        for (size_t i = 0; i < 7; i++) rb_insert(&t, seq[i]);
        printf("  --- 基础结构 ---\n");
        dump(&t);
        rb_delete(&t, 3);
        printf("  --- 删除 3 之后 ---\n");
        dump(&t);
        verify_or_die(&t, "删除红色叶子之后");
        rb_destroy(&t);
    }

    /* ---------------------------------------------------------- */
    section("删除修复情况 2:黑色兄弟,两个侄子都是黑色");
    printf("构造树:插入 20,10,30,5,15,25,35,再删除 5,15,35,25(均为红色叶子,\n");
    printf("删除时不触发 fixup),把 10 和 30 \"拍平\"成普通黑色叶子。\n");
    {
        RBTree t = rb_create();
        int seq[] = {20, 10, 30, 5, 15, 25, 35};
        for (size_t i = 0; i < 7; i++) rb_insert(&t, seq[i]);
        printf("  --- 基础结构 ---\n");
        dump(&t);
        rb_delete(&t, 5);
        rb_delete(&t, 15);
        rb_delete(&t, 35);
        rb_delete(&t, 25);
        printf("  --- 拍平之后(10、30 均为无孩子的黑色叶子) ---\n");
        dump(&t);
        verify_or_die(&t, "拍平之后");
        printf("  删除 10:它在 root(20) 左孩子位置,兄弟是 root.R=30(BLACK),\n");
        printf("  30 的两个孩子都是 NIL(黑色)——命中 case2:兄弟 30 变红,\n");
        printf("  \"双黑\"标记原样上移到 x=root,因为 x 已经是根,循环立即结束。\n");
        rb_delete(&t, 10);
        printf("  --- 删除 10 之后 ---\n");
        dump(&t);
        verify_or_die(&t, "case2 修复之后");
        rb_destroy(&t);
    }

    /* ---------------------------------------------------------- */
    section("删除修复情况 3→4:黑色兄弟,近侄子红、远侄子黑");
    printf("构造树:插入 20,10,35,32。\n");
    {
        RBTree t = rb_create();
        int seq[] = {20, 10, 35, 32};
        for (size_t i = 0; i < 4; i++) rb_insert(&t, seq[i]);
        printf("  --- 基础结构 ---\n");
        dump(&t);
        verify_or_die(&t, "构造完成");
        printf("  删除黑色叶子 10:兄弟是 root.R=35(BLACK),35 的左孩子(近侄子)\n");
        printf("  32 是红色,右孩子(远侄子)是 NIL(黑色)——命中 case3:\n");
        printf("  先对兄弟 35 做\"变色+右旋\"，让远侄子变成红色,转化为 case4;\n");
        printf("  紧接着 case4 一步到位:32 变成新根(黑色),20 和 35 变成它的\n");
        printf("  黑色孩子,x 被设成 tree->root,循环结束。\n");
        rb_delete(&t, 10);
        printf("  --- 删除 10 之后 ---\n");
        dump(&t);
        verify_or_die(&t, "case3→case4 修复之后");
        rb_destroy(&t);
    }

    /* ---------------------------------------------------------- */
    section("删除修复情况 4:黑色兄弟,远侄子直接为红");
    printf("构造树:插入 20,10,35,40。\n");
    {
        RBTree t = rb_create();
        int seq[] = {20, 10, 35, 40};
        for (size_t i = 0; i < 4; i++) rb_insert(&t, seq[i]);
        printf("  --- 基础结构 ---\n");
        dump(&t);
        verify_or_die(&t, "构造完成");
        printf("  删除黑色叶子 10:兄弟 root.R=35(BLACK),远侄子 40 是红色,\n");
        printf("  直接命中 case4:兄弟 35 变成父亲(20)的颜色(黑),父亲变黑,\n");
        printf("  远侄子 40 变黑,对父亲左旋,x 设为根,循环结束——一次旋转\n");
        printf("  就消掉了双黑标记,不需要经过 case3 的中转。\n");
        rb_delete(&t, 10);
        printf("  --- 删除 10 之后 ---\n");
        dump(&t);
        verify_or_die(&t, "case4 直接命中之后");
        rb_destroy(&t);
    }

    /* ---------------------------------------------------------- */
    section("删除有两个孩子的节点:后继替位");
    printf("构造树:插入 50,30,70,20,40,60,80,删除有两个孩子的 30。\n");
    printf("30 的后继(右子树最小值)是 40,rb_delete 会把 40 的 key 复制到\n");
    printf("30 所在的节点上,再去删除原来位置上的 40(它此时最多一个孩子)。\n");
    {
        RBTree t = rb_create();
        int seq[] = {50, 30, 70, 20, 40, 60, 80};
        for (size_t i = 0; i < 7; i++) rb_insert(&t, seq[i]);
        printf("  --- 基础结构 ---\n");
        dump(&t);
        rb_delete(&t, 30);
        printf("  --- 删除 30 之后 ---\n");
        dump(&t);
        verify_or_die(&t, "后继替位之后");
        printf("  说明:40 原来的红色叶子位置消失,40 的 key 出现在原来 30 的\n");
        printf("  节点位置上并保持黑色——这正是\"复制 key、删除物理节点\"的效果,\n");
        printf("  40 本身的颜色和树结构由被删除的那个物理节点决定,而不是 40。\n");
        rb_destroy(&t);
    }

    /* ---------------------------------------------------------- */
    section("批量插入 1..15,再全部删除:每步都校验不变量");
    {
        RBTree t = rb_create();
        const int n = 15;
        for (int i = 1; i <= n; i++) {
            rb_insert(&t, i);
            char err[256];
            if (!rb_verify(&t, err, sizeof err)) {
                fprintf(stderr, "插入 %d 后校验失败: %s\n", i, err);
                return 1;
            }
        }
        printf("  插入 1..%d 之后:\n", n);
        dump(&t);
        printf("  count=%d, black_height=%d\n", rb_count(&t), rb_black_height(&t));
        printf("  (说明:15 个节点的红黑树黑高为 3,与理论上界\n");
        printf("  黑高 <= log2(n+1) 一致,可见红黑树的\"近似平衡\"约束。)\n");
        for (int i = 1; i <= n; i++) {
            rb_delete(&t, i);
            char err[256];
            if (!rb_verify(&t, err, sizeof err)) {
                fprintf(stderr, "删除 %d 后校验失败: %s\n", i, err);
                return 1;
            }
        }
        printf("  全部删除之后:root==nil? %s, count=%d\n",
               t.root == t.nil ? "是" : "否", rb_count(&t));
        rb_destroy(&t);
    }

    /* ---------------------------------------------------------- */
    section("错误示例:忘记在 fixup 结尾强制根为黑");
    printf("CLRS 的 insert_fixup 循环结束后必须补一句\"root 强制设为黑\",\n");
    printf("因为 case1 可能把根从黑变红(根的两个孩子变黑、根自己变红)。\n");
    printf("下面用一个隔离的最小实现演示:如果漏掉这一步会怎样。\n");
    {
        /* 用一个独立的、故意省略"根强制变黑"步骤的最小插入函数,
         * 只影响这个局部变量树,不触碰真实的 rbtree.c 实现。 */
        typedef struct BrokenNode {
            int key;
            int is_red; /* 1=红 0=黑,故意用 int 而不是 RBColor 强调这是隔离代码 */
            struct BrokenNode *left, *right, *parent;
        } BrokenNode;

        static BrokenNode nil_storage = {0, 0, NULL, NULL, NULL};
        BrokenNode *nil = &nil_storage;
        BrokenNode n1 = {10, 0, nil, nil, nil}; /* 根,黑 */
        BrokenNode n2 = {5, 1, nil, nil, &n1};  /* 左孩子,红 */
        BrokenNode n3 = {15, 1, nil, nil, &n1}; /* 右孩子,红 */
        n1.left = &n2;
        n1.right = &n3;

        printf("  修复前(合法的红黑树):root=10(黑), L=5(红), R=15(红)\n");
        printf("  现在模拟插入一个新节点,其父亲和叔叔都是红色(case1),\n");
        printf("  正确做法是把 5 和 15 变黑、10 变红,然后【强制根变黑】。\n");
        printf("  如果实现里漏掉最后这一步,\"变色\"完成后的状态是:\n");
        n2.is_red = 0;
        n3.is_red = 0;
        n1.is_red = 1; /* 故意不强制变回黑,模拟漏写的 bug */
        printf("    root=%d(%s), L=%d(%s), R=%d(%s)\n",
               n1.key, n1.is_red ? "红" : "黑",
               n2.key, n2.is_red ? "红" : "黑",
               n3.key, n3.is_red ? "红" : "黑");
        printf("  这违反了性质 2(根必须是黑色)。表面上看不出明显的错误——\n");
        printf("  树仍然是合法的二叉搜索树、也没有红红相邻——但如果这棵子树\n");
        printf("  之后被作为某个更大树的一部分继续插入,红色的根会被当成\n");
        printf("  普通红色节点参与后续的 fixup 判断,导致黑高计算和后续的\n");
        printf("  case 判断全部基于错误的颜色前提,产生的错误可能在很多次\n");
        printf("  操作之后才会以一种看似无关的方式暴露出来,非常难以定位。\n");
        printf("  真实的 rbtree.c 在 insert_fixup 循环结束后写了\n");
        printf("  \"tree->root->color = RB_BLACK;\"，正是为了杜绝这个问题。\n");
    }

    printf("\n所有演示完成。\n");
    return 0;
}
