/* B-tree demo: build, split, search, and every delete case (leaf with no
 * underflow, leaf-underflow via borrow-left/borrow-right/merge, internal-node
 * delete via predecessor/successor steal, root shrink to empty), plus a
 * deliberately broken split to show what btree_verify() is for.
 *
 * All shapes below were traced by hand with btree_print() before being
 * written into this file -- see README.md "机制剖析" for the full
 * before/after ASCII diagrams and why each case fires the way it does. */
#include "btree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void verify_or_die(const BTree *t, const char *ctx) {
    char err[256];
    if (!btree_verify(t, err, sizeof err)) {
        fprintf(stderr, "btree_verify FAILED after %s: %s\n", ctx, err);
        exit(1);
    }
    printf("  [verify ok after %s]\n", ctx);
}

static void section(const char *title) {
    printf("\n========== %s ==========\n", title);
}

int main(void) {
    /* ---- 1. build with t=2, watch a root split happen ---- */
    section("1. 插入触发节点分裂 (t=2)");
    {
        BTree t = btree_create(2);
        int seq[] = {10, 20, 30, 40, 50, 60, 70};
        for (size_t i = 0; i < sizeof seq / sizeof seq[0]; i++) {
            bool grew = (t.root != NULL) && (t.root->n == 2 * t.t - 1);
            printf("插入 %d%s\n", seq[i], grew ? "  <-- 这一次插入前 root 已满，会触发分裂" : "");
            btree_insert(&t, seq[i]);
            btree_print(&t);
            verify_or_die(&t, "insert");
        }
        printf("最终 height=%d, node 数=%d\n", btree_height(&t), btree_count_nodes(&t));
        btree_destroy(&t);
    }

    /* ---- 2. root split close-up: before/after, which key got pushed up ---- */
    section("2. Root 分裂特写：树从 1 层长到 2 层");
    {
        BTree t = btree_create(2);
        btree_insert(&t, 10);
        btree_insert(&t, 20);
        btree_insert(&t, 30);
        printf("插入 40 之前（root 已满，n=3=2t-1）：\n");
        btree_print(&t);
        printf("height=%d（还是单层）\n", btree_height(&t));

        btree_insert(&t, 40);
        printf("\n插入 40 之后：\n");
        btree_print(&t);
        printf("height=%d（长高了一层）\n", btree_height(&t));
        printf("root 现在只有 1 个 key（%d），它是原来 [10 20 30 40] 分裂时\n"
               "被“挤”到中间、推上去的那个 key；剩下的 key 平分成左右两个孩子。\n",
               t.root->keys[0]);
        verify_or_die(&t, "root split");
        btree_destroy(&t);
    }

    /* ---- 3. search: found / not found ---- */
    section("3. 查找：命中与未命中");
    {
        BTree t = btree_create(3);
        int seq[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130};
        for (size_t i = 0; i < sizeof seq / sizeof seq[0]; i++) btree_insert(&t, seq[i]);
        printf("树结构 (t=3)：\n");
        btree_print(&t);
        int found[] = {10, 60, 130};
        int missing[] = {5, 65, 999};
        for (size_t i = 0; i < sizeof found / sizeof found[0]; i++)
            printf("search(%d) = %s\n", found[i], btree_search(&t, found[i]) ? "找到" : "未找到");
        for (size_t i = 0; i < sizeof missing / sizeof missing[0]; i++)
            printf("search(%d) = %s\n", missing[i], btree_search(&t, missing[i]) ? "找到" : "未找到");
        btree_destroy(&t);
    }

    /* ---- shared base tree for the delete demos below (t=3) ---- */
    BTree t = btree_create(3);
    {
        int seq[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 15, 25};
        for (size_t i = 0; i < sizeof seq / sizeof seq[0]; i++) btree_insert(&t, seq[i]);
    }
    section("4. 后续删除演示的公共起点 (t=3)");
    printf("插入 10,20,...,130,15,25 之后：\n");
    btree_print(&t);
    printf("每个叶子的 key 数：");
    for (int i = 0; i <= t.root->n; i++) printf("%d ", t.root->children[i]->n);
    printf("（t=3 时非 root 节点最少 t-1=2 个 key，最多 2t-1=5 个）\n");
    verify_or_die(&t, "shared base tree");

    /* ---- 5. delete from a leaf with no underflow ---- */
    section("5. 删除：叶子够用，不需要借/合并");
    {
        printf("叶子 [100 110 120 130] 有 4 个 key，删掉一个后还有 3 个，>= t-1=2，\n"
               "所以直接删，什么都不用调整。\n");
        btree_delete(&t, 130);
        btree_print(&t);
        verify_or_die(&t, "delete 130 (no underflow)");
    }

    /* ---- 6. delete causing underflow, fixed by borrowing from LEFT sibling ---- */
    section("6. 删除：叶子会下溢，向左邻居借一个 key");
    {
        printf("要删 40：它在叶子 [40 50]（n=2，已经是下限）。\n"
               "左邻居 [10 15 20 25] 有 4 个 key，够借；右邻居 [70 80] 也在下限，不够借。\n"
               "=> 走 3a：从左邻居借。\n");
        btree_delete(&t, 40);
        btree_print(&t);
        printf("借的过程：父节点的分隔 key 30 被“压”进 [40 50] 变成 [30 50]，\n"
               "左邻居里最大的 key 25 顶替 30，成为新的分隔 key，左邻居变成 [10 15 20]。\n");
        verify_or_die(&t, "delete 40 (borrow-left)");
    }

    /* ---- 7. delete causing underflow, fixed by borrowing from RIGHT sibling ---- */
    section("7. 删除：叶子会下溢，向右邻居借一个 key");
    {
        printf("当前 root 是 [25 60 90]，孩子是 [10 15 20] [30 50] [70 80] [100 110 120]。\n"
               "要删 70：它在叶子 [70 80]（n=2，下限）。\n"
               "左邻居 [30 50] 也在下限，不够借；右邻居 [100 110 120] 有 3 个 key，够借。\n"
               "=> 走 3b：从右邻居借。\n");
        btree_delete(&t, 70);
        btree_print(&t);
        printf("借的过程：分隔 key 90 被“压”进 [80]，右邻居最小的 key 100 顶替 90，\n"
               "成为新的分隔 key，右邻居变成 [110 120]。\n");
        verify_or_die(&t, "delete 70 (borrow-right)");
    }

    /* ---- 8. delete causing underflow, fixed by merging (no sibling has spare) ---- */
    section("8. 删除：叶子会下溢，且两侧邻居都不够借 -> 合并");
    {
        printf("当前 root 是 [25 60 100]，孩子是 [10 15 20] [30 50] [80 90] [110 120]。\n"
               "要删 110：它在叶子 [110 120]（n=2，下限）。\n"
               "它只有一个邻居 [80 90]（是最后一个孩子），也在下限（n=2），借不到。\n"
               "=> 两侧都不够借，只能合并：分隔 key 100 被拉下来，\n"
               "跟 [80 90] 和 [110 120] 拼成一个节点，root 从 3 个 key 掉到 2 个。\n");
        btree_delete(&t, 110);
        btree_print(&t);
        verify_or_die(&t, "delete 110 (merge, no spare on either side)");
    }

    btree_destroy(&t);

    /* ---- 9. a fresh, smaller tree purpose-built to force an unambiguous merge ---- */
    section("9. 专门构造一棵树，强制走一次纯粹的合并 (case 3c)");
    {
        BTree t2 = btree_create(2); /* t=2: min keys = 1, easy to force underflow */
        int seq[] = {10, 20, 30, 40};
        for (size_t i = 0; i < 4; i++) btree_insert(&t2, seq[i]);
        printf("t=2, 插入 10,20,30,40 之后：\n");
        btree_print(&t2);
        printf("root=[20]，孩子 [10] 和 [30 40]。\n\n");

        btree_delete(&t2, 40);
        printf("删 40 之后（右孩子降到 [30]，n=1=下限）：\n");
        btree_print(&t2);
        verify_or_die(&t2, "delete 40");

        printf("\n现在删 10：左孩子 [10] 本身就是要删的那个 key 所在的叶子，\n"
               "删完它会变空（n=0）。它唯一的邻居 [30] 也在下限（n=1），没法借，\n"
               "=> 必须合并：把分隔 key 20 和右孩子 [30] 一起并入左孩子，\n"
               "合并后 root 变空，树整体降一层。\n");
        btree_delete(&t2, 10);
        btree_print(&t2);
        printf("height=%d（从 1 降到 0，root 从内部节点变成了唯一的叶子）\n", btree_height(&t2));
        verify_or_die(&t2, "delete 10 (pure merge, root shrinks)");
        btree_destroy(&t2);
    }

    /* ---- 10. delete a key that lives in an INTERNAL node ---- */
    section("10. 删除一个存在于内部节点的 key：前驱/后继替换");
    {
        BTree t3 = btree_create(2);
        int build[] = {20, 10, 30, 5, 15};
        for (size_t i = 0; i < 5; i++) btree_insert(&t3, build[i]);
        printf("case 2a（偷前驱）：插入 20,10,30,5,15 之后：\n");
        btree_print(&t3);
        printf("root=[20]，左孩子 [5 10 15]（n=3，够借），右孩子 [30]（n=1，下限）。\n"
               "删 20：它是内部节点的 key。检查左孩子，n=3>=t=2，够借\n"
               "=> 用左子树里最大的 key（前驱，也就是 15）顶替 20，\n"
               "然后递归地把 15 从左子树里删掉。\n");
        btree_delete(&t3, 20);
        btree_print(&t3);
        printf("root 现在的 key 是 %d（前驱 15 被提上来了）\n", t3.root->keys[0]);
        verify_or_die(&t3, "delete 20 (internal, case 2a predecessor)");
        btree_destroy(&t3);

        BTree t4 = btree_create(2);
        int build2[] = {20, 10, 30, 40, 35};
        for (size_t i = 0; i < 5; i++) btree_insert(&t4, build2[i]);
        printf("\ncase 2b（偷后继）：插入 20,10,30,40,35 之后：\n");
        btree_print(&t4);
        printf("root=[20]，左孩子 [10]（n=1，下限，不够借），右孩子 [30 35 40]（n=3，够借）。\n"
               "删 20：左孩子不够借，看右孩子，n=3>=t=2，够借\n"
               "=> 用右子树里最小的 key（后继，也就是 30）顶替 20，\n"
               "然后递归地把 30 从右子树里删掉。\n");
        btree_delete(&t4, 20);
        btree_print(&t4);
        printf("root 现在的 key 是 %d（后继 30 被提上来了）\n", t4.root->keys[0]);
        verify_or_die(&t4, "delete 20 (internal, case 2b successor)");
        btree_destroy(&t4);
    }

    /* ---- 11. root collapses all the way to an empty tree ---- */
    section("11. 一直删到空树");
    {
        BTree t5 = btree_create(2);
        for (int i = 1; i <= 15; i++) btree_insert(&t5, i);
        printf("插入 1..15 之后：\n");
        btree_print(&t5);
        for (int i = 1; i <= 15; i++) {
            btree_delete(&t5, i);
            verify_or_die(&t5, "shrink-to-empty sequence");
        }
        printf("全部删完：root == NULL 是 %s，height=%d\n",
               t5.root == NULL ? "真" : "假", btree_height(&t5));
        btree_destroy(&t5);
    }

    /* ---- 12. BROKEN split: off-by-one midpoint silently breaks the invariant ---- */
    section("12. 错误示例：分裂中点算错一位，会怎样");
    {
        /* Deliberately re-implement split with mid = t instead of mid = t-1.
         * Kept local to this function so it can never leak into btree.c. */
        typedef struct BrokenNode {
            int n;
            int keys[8];
        } BrokenNode;

        BrokenNode y = {5, {1, 2, 3, 4, 5}};
        int t_deg = 3; /* max keys = 2t-1 = 5, min per non-root node = t-1 = 2 */
        printf("满叶子 [1 2 3 4 5]（t=%d，最多 2t-1=%d 个 key）要分裂。\n", t_deg, 2 * t_deg - 1);
        printf("正确做法：中间下标是 t-1=%d，也就是 key[%d]=%d 被推上去，\n"
               "左边留 t-1=%d 个，右边留 t-1=%d 个。\n",
               t_deg - 1, t_deg - 1, y.keys[t_deg - 1], t_deg - 1, t_deg - 1);

        int mid_broken = t_deg; /* BUG: off by one, should be t_deg - 1 */
        int left_n = mid_broken;
        int right_n = y.n - mid_broken - 1;
        printf("\n错误版本：把中间下标写成了 t=%d（漏了 -1），key[%d]=%d 被推上去，\n"
               "左边留 %d 个，右边留 %d 个。\n",
               mid_broken, mid_broken, y.keys[mid_broken], left_n, right_n);
        printf("最少要求每个非 root 节点有 t-1=%d 个 key，右边只有 %d 个",
               t_deg - 1, right_n);
        if (right_n < t_deg - 1) {
            printf(" -> 违反不变量！\n");
        } else {
            printf("\n");
        }
        printf("这种 bug 不会让程序崩溃，也不会被 sanitizer 抓到（没有非法内存访问），\n"
               "查找、插入在小规模测试下可能看起来都“正常工作”，直到某次删除因为\n"
               "某个节点的 key 数比假设的下限还少，触发数组下标或逻辑上的错误。\n"
               "唯一可靠的防线就是 btree_verify()：每次插入/删除后跑一次，\n"
               "任何不变量被破坏都会在“案发现场”被抓到，而不是等到很久以后才崩溃。\n");
    }

    printf("\n========== 全部演示完成 ==========\n");
    return 0;
}
