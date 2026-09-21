/* B+-tree demo: leaf split (promoted key COPIED, stays real data in a
 * leaf), internal-node split including a root split (promoted key MOVED,
 * no duplicate left behind), leaf borrow, leaf merge with leaf-chain
 * repair, internal-node merge with separator pull-down, root collapse,
 * and a range query with real leaf-visit counts.
 *
 * Every shape below was traced by hand with bplustree_print() /
 * bplustree_print_leaf_chain() before being written here -- see
 * README.md "机制剖析" for the annotated ASCII diagrams. */
#include "bplustree.h"

#include <stdio.h>
#include <stdlib.h>

static void verify_or_die(const BPlusTree *t, const char *ctx) {
    char err[256];
    if (!bplustree_verify(t, err, sizeof err)) {
        fprintf(stderr, "bplustree_verify FAILED after %s: %s\n", ctx, err);
        exit(1);
    }
    printf("  [verify ok after %s]\n", ctx);
}

static void section(const char *title) {
    printf("\n========== %s ==========\n", title);
}

int main(void) {
    /* ---- 1. leaf split close-up: promoted key is a COPY ---- */
    section("1. Leaf 分裂特写：提升的 key 是“复制”，不是“移动”");
    {
        BPlusTree t = bplustree_create(2); /* t=2: leaf 容量 2t-1=3 */
        bplustree_insert(&t, 10, 1000);
        bplustree_insert(&t, 20, 2000);
        bplustree_insert(&t, 30, 3000);
        printf("插入 40 之前（root 是叶子，已满 n=3=2t-1）：\n");
        bplustree_print(&t);
        printf("height=%d（单层）\n", bplustree_height(&t));

        bplustree_insert(&t, 40, 4000);
        printf("\n插入 40 之后：\n");
        bplustree_print(&t);
        bplustree_print_leaf_chain(&t);
        printf("height=%d（长高一层）\n", bplustree_height(&t));
        printf("root 的 routing key 是 %d —— 它是从原来叶子 [10 20 30] 分裂时\n"
               "被“复制”上去的：key 30 依然作为真实数据留在右边的叶子 [30:3000 40:4000]\n"
               "里，root 里的 30 只是一份指路用的副本，不是把 30 从叶子里搬走。\n",
               t.root->keys[0]);
        verify_or_die(&t, "leaf split");
        bplustree_destroy(&t);
    }

    /* ---- 2. continue growing to set up a root/internal split ---- */
    section("2. 继续插入到 100，制造 root 分裂 (t=2)");
    BPlusTree shared = bplustree_create(2);
    {
        for (int k = 10; k <= 90; k += 10) bplustree_insert(&shared, k, k * 100);
        printf("插入到 90 时的树：\n");
        bplustree_print(&shared);
        bplustree_print_leaf_chain(&shared);
        printf("root（内部节点）已满：n=%d=2t-1，children=%d 个\n", shared.root->n, shared.root->n + 1);
        verify_or_die(&shared, "build to 90");
    }

    /* ---- 3. internal split (root split) close-up: promoted key MOVED ---- */
    section("3. Internal 分裂特写（root 分裂）：提升的 key 是“移动”，不留副本");
    {
        printf("插入 100 之前：\n");
        bplustree_print(&shared);

        bplustree_insert(&shared, 100, 10000);
        printf("\n插入 100 之后：\n");
        bplustree_print(&shared);
        bplustree_print_leaf_chain(&shared);
        printf("height=%d（又长高一层）\n", bplustree_height(&shared));
        printf("新 root 只有 1 个 key（%d）。这个 %d 原本是旧 root [30 50 70] 的中间\n"
               "routing key，分裂时被“移动”到新 root：左边孩子变成 [30]，右边孩子变成\n"
               "[70 90]，两边都不再含有 %d —— 跟第 1 步的 leaf 分裂正好相反，internal\n"
               "节点从来不存真实数据，所以它的分裂没有理由留一份副本在原地。\n",
               shared.root->keys[0], shared.root->keys[0], shared.root->keys[0]);
        verify_or_die(&shared, "internal/root split");
    }

    /* ---- 4. finish building the shared demo tree (used by delete/range) ---- */
    section("4. 继续插入到 130，得到一棵 3 层树（后续删除/range 演示共用）");
    {
        for (int k = 110; k <= 130; k += 10) bplustree_insert(&shared, k, k * 100);
        bplustree_print(&shared);
        bplustree_print_leaf_chain(&shared);
        printf("height=%d nodes=%d leaves=%d keys=%d\n",
            bplustree_height(&shared), bplustree_count_nodes(&shared),
            bplustree_count_leaves(&shared), bplustree_count_keys(&shared));
        verify_or_die(&shared, "build to 130");
    }

    /* ---- 5. delete with no underflow (spare key, nothing to fix) ---- */
    section("5. 删除：叶子有多余 key，不触发任何调整");
    {
        bplustree_delete(&shared, 130);
        printf("删除 130 后：\n");
        bplustree_print(&shared);
        bplustree_print_leaf_chain(&shared);
        verify_or_die(&shared, "delete 130 (no underflow)");
    }

    /* ---- 6. delete forcing a leaf BORROW ---- */
    section("6. 删除：触发 leaf 借位 (borrow)");
    {
        bplustree_delete(&shared, 120);
        printf("删除 120 后，叶子 [110] 正好落在下限 t-1=1，还不算下溢：\n");
        bplustree_print(&shared);

        printf("\n再删除 110：叶子 [110] 变空，下溢，向左邻居 [90 100] 借一个 key\n"
               "（[90 100] 有 2 个 key，超过下限 1，是合法的“出借方”）：\n");
        bplustree_delete(&shared, 110);
        bplustree_print(&shared);
        bplustree_print_leaf_chain(&shared);
        printf("parent 的 routing key 从 110 变成了 100（新的右子树最小值的复制），\n"
               "这跟借位前 [90 100]->[90] / 新叶子多了 100 是一致的。\n");
        verify_or_die(&shared, "delete 120,110 (leaf borrow)");
    }

    /* ---- 7. delete forcing a leaf MERGE, with chain-repair proof ---- */
    section("7. 删除：触发 leaf 合并 (merge)，用完整链表证明 next 指针被修好了");
    {
        printf("合并之前的链表：\n");
        bplustree_print_leaf_chain(&shared);
        printf("删除 100：叶子 [100] 只有 1 个 key，左邻居 [90] 也只有 1 个（没有多余的\n"
               "可借），两边都在下限，只能合并。合并时 routing key 100 直接被“丢弃”\n"
               "——它本来就只是一份复制品，不像 B 树合并那样需要把 key 拉下来。\n");
        bplustree_delete(&shared, 100);
        bplustree_print(&shared);
        printf("\n合并之后的链表（[100] 从链上消失，[90] 的 next 直接指向下一个存活的叶子）：\n");
        bplustree_print_leaf_chain(&shared);
        verify_or_die(&shared, "delete 100 (leaf merge + chain repair)");
        bplustree_destroy(&shared);
    }

    /* ---- 8. internal-node MERGE with separator pulled DOWN, cascading to a root collapse ---- */
    section("8. Internal 节点合并：分隔 key 被“拉下来”，并连锁触发 root 收缩");
    {
        BPlusTree t2 = bplustree_create(2);
        for (int k = 0; k < 60; k += 2) bplustree_insert(&t2, k, k);
        int drain[] = {58, 56, 54, 52, 50, 48, 46, 44, 42};
        for (size_t i = 0; i < sizeof(drain) / sizeof(drain[0]) - 1; i++) {
            bplustree_delete(&t2, drain[i]);
        }
        printf("删除到只剩最后一步之前（root 有 2 个 routing key，3 个孩子）：\n");
        bplustree_print(&t2);
        printf("height=%d\n", bplustree_height(&t2));

        printf("\n删除 42：叶子 [40] 变空并与邻居合并，导致它的父节点 [42]（一个\n"
               "internal 节点）只剩 1 个孩子、0 个 routing key，下溢。它跟兄弟 [24]\n"
               "合并：root 的 routing key 32（分隔 [24] 子树和 [40] 子树的那个 key）\n"
               "被“拉下来”塞进合并后的节点，因为它是唯一还在区分两边孙子层的东西——\n"
               "跟第 7 步的 leaf 合并（直接丢弃分隔 key）正好相反。\n");
        bplustree_delete(&t2, 42);
        bplustree_print(&t2);
        printf("height=%d（root 现在只剩 1 个 routing key，还没到收缩的地步）\n", bplustree_height(&t2));
        verify_or_die(&t2, "internal merge (no root collapse yet)");
        bplustree_destroy(&t2);
    }

    /* ---- 9. root collapse: root's only child becomes the new root ---- */
    section("9. Root 收缩：root 合并到只剩 1 个孩子时，直接被那个孩子取代");
    {
        BPlusTree t3 = bplustree_create(2);
        for (int k = 0; k < 60; k += 2) bplustree_insert(&t3, k, k);
        int drain[] = {58,56,54,52,50,48,46,44,42,40,38,36,34,32,30,28};
        for (size_t i = 0; i < sizeof(drain) / sizeof(drain[0]); i++) bplustree_delete(&t3, drain[i]);
        printf("继续删到最后只剩 %d 个 key 时（height=%d）：\n",
            bplustree_count_keys(&t3), bplustree_height(&t3));
        bplustree_print(&t3);
        verify_or_die(&t3, "drain to small tree");
        printf("\n持续删空整棵树，最终 root 应变回 NULL（空树的规范状态）：\n");
        while (bplustree_height(&t3) >= 0) {
            const BPlusNode *x = t3.root;
            while (!x->is_leaf) x = x->u.children[0];
            bplustree_delete(&t3, x->keys[0]);
        }
        printf("height=%d（期望 -1），root==NULL: %d\n", bplustree_height(&t3), t3.root == NULL);
        bplustree_destroy(&t3);
    }

    /* ---- 10. range query: leaf-visit count is the whole point of the leaf chain ---- */
    section("10. Range Query：只扫叶子链表，实测访问了多少个叶子");
    {
        BPlusTree t4 = bplustree_create(2);
        for (int k = 10; k <= 130; k += 10) bplustree_insert(&t4, k, k * 100);
        printf("树结构（t=2，共 %d 个叶子）：\n", bplustree_count_leaves(&t4));
        bplustree_print(&t4);
        bplustree_print_leaf_chain(&t4);

        int keys[32], vals[32], visited;
        int n = bplustree_range_query(&t4, 35, 95, keys, vals, 32, &visited);
        printf("\nrange_query(35, 95)：先向下走到 key=35 应该在的叶子（不是最左叶子），\n"
               "然后沿 next 链表一路向右扫描，直到超过 95 为止：\n");
        printf("找到 %d 对 key/value，一共访问了 %d 个叶子（总叶子数 %d）：\n",
            n, visited, bplustree_count_leaves(&t4));
        for (int i = 0; i < n; i++) printf("  %d:%d\n", keys[i], vals[i]);
        printf("\n对比 B 树：B 树没有叶子链表，range query 只能从根做一次中序遍历，\n"
               "沿途每个经过的内部节点都要重新决策“往哪个孩子走”；B+ 树只需要一次\n"
               "对数高度的下降定位起点，剩下全部是一条链表上的线性扫描，访问节点数\n"
               "只跟结果集大小相关，跟树的总大小（或树高）无关——这就是数据库/文件\n"
               "系统索引普遍选 B+ 树而不是 B 树的核心原因，README 的性能测试小节有\n"
               "更大规模、多档位宽度下的实测数据支撑这个结论。\n");
        bplustree_destroy(&t4);
    }

    printf("\n========== 全部演示完成 ==========\n");
    return 0;
}
