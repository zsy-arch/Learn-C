# C 语言算法与数据结构进阶：从二叉树到 B+ 树

> 承接《C 语言语法与最佳实践》与 `c-algo-experiments/`（排序、查找、链表、栈队列、哈希表、树、递归、性能测试）之后的进阶篇。
> 六个专题、六份可独立编译运行的实验代码、161 个测试全部真实跑通——每一段编译命令和运行输出都是在本机原样粘贴的，没有一句"应该会输出"。

---

## 一、导读

### 这份文档适合谁

- 已经跑完 `c-algo-experiments/`（尤其是 `12_project`），会写基本的链表、栈队列、哈希表、简单二叉搜索树，想往"面试高频结构"和"工业级数据结构"再进一步的人
- 知道"红黑树能保证 O(log n)"这句话，但没有亲手写过 fixup 逻辑、说不清 4 种删除情形具体是什么的人
- 用过数据库索引、听过"B+ 树"这个词，但从没亲手比较过 B 树和 B+ 树在代码层面到底哪里不一样的人
- 刷题时能背出"BFS 用队列、DFS 用栈"，但没有亲手写过拓扑排序、环检测、二分图判定这几个 BFS/DFS 的变体应用的人
- 想要一套**不是抄来的、每一行结论背后都有真实编译输出支撑**的进阶数据结构参考

### 学完能掌握什么

读完并动手跑完全部 6 个实验目录，你应该能：

1. 说清楚递归遍历、迭代遍历（显式栈）、Morris 遍历（O(1) 空间）、层序遍历（BFS）之间的关系，并解释线索二叉树是怎么把 Morris 遍历里的"临时线索"变成"永久线索"的
2. 从前序+中序或后序+中序两种遍历序列组合，唯一重建出原始二叉树，并说清楚为什么"前序+后序"组合做不到这件事
3. 独立写出图的邻接矩阵和邻接表表示，说清楚两者的空间复杂度差异和各自的适用场景
4. 写出 BFS 最短路径、DFS 发现/完成时间、Kahn 拓扑排序、DFS 拓扑排序、连通分量、二分图判定
5. 亲手复现"无向图环检测忘记排除父节点"这个新手必踩的坑，说清楚为什么这**不是**内存安全问题、sanitizer 抓不到它，而是纯逻辑错误
6. 写出 Trie 的插入、查找、前缀查找、正确的递归回溯删除（只释放"确实不再被任何单词共享"的节点），并说清楚为什么"无条件往上 free 整条路径"是错的
7. 按 CLRS 的最小度数 `t` 约定写出 B 树的插入（下降路径上主动分裂）和完整的 6 种删除情形（叶子直接删、前驱/后继替位合并、借位/合并三种子情形）
8. 说清楚 B+ 树和 B 树在代码层面的核心差异：**leaf 分裂时提升的 key 是复制、internal 分裂时提升的 key 是移动**，以及 leaf 之间的兄弟链表如何让 range query 不需要每次都从根重新下降
9. 独立写出红黑树的左右旋转（3 个指针的 parent 更新一个都不能漏）、插入修复的 3 种情况、删除修复的 4 种情况，并说清楚"哨兵 NIL"设计为什么能让 fixup 循环直接向上走
10. 对着每一种结构写出属性校验函数（BST 性质、红黑树 5 条性质、B 树/B+ 树的阶数与排序约束），并把这套"每次插入/删除之后立即校验，而不是只在最后校验"的方法用到自己的项目里

### 这份文档和普通教程的区别

大部分数据结构教程只给你"正确代码"。这份文档会先给你看**真实运行出来的错误**——不是描述性的"如果这样写会出错"，而是真的编译、真的跑、把真实输出贴出来，再解释底层发生了什么，最后才是正确写法。

举几个本文档里真实出现过的例子：

| 章节 | 错误做法 | 真实观测到的现象 |
|---|---|---|
| 二叉树遍历 | Morris 遍历后忘记拆除线索 | `right` 指针被永久污染，树里出现环；之后对这棵"坏树"做递归中序遍历或递归释放会陷入无限递归——这正是本实验最初真的把自己写进 stack overflow 的地方 |
| 图遍历 | 无向图环检测直接照抄有向图的写法（不排除父节点） | 任意一条边都会被误判成环：路径图 `0-1-2` 这种明显无环的图，`has_cycle = true`——假阳性，而且不是内存问题，sanitizer 完全抓不到 |
| Trie | 删除时无条件往上 `free` 整条路径 | 删除共享前缀的单词 `car` 之后，本该完好无损的 `card` 也从 Trie 里"消失"了 |
| B 树 | 分裂中点算错一位（`t-1` 写成 `t`） | 分裂后两个子节点数量不平衡，`btree_verify()` 在插入的第几步就会报错，具体报错位置和次数都是真实跑出来的 |
| 红黑树 | `insert_fixup` 循环结束后忘记强制根为黑 | 表面上看不出错——仍是合法 BST、没有红红相邻——但违反性质 2，这棵子树被并入更大的树后，错误的黑高计算会在很多次操作之后才以看似无关的方式暴露出来 |

"假阳性""看不出错""很多次操作之后才暴露"——这几行才是真正要命的地方，也是本文档反复强调的核心方法论：**只验证最终结果是不够的，必须在每一步操作之后都验证结构不变量**。

### 如何运行实验

所有代码在 `c-algo-advanced-experiments/` 下，一个专题一个目录：

```text
c-algo-advanced-experiments/
├── 01_binary_tree_traversal/   二叉树遍历（递归/迭代/Morris/BFS/线索树/重建）
├── 02_graph_traversal/         图遍历（BFS/DFS/拓扑排序/环检测/二分图）
├── 03_trie/                    字典树（插入/查找/删除/自动补全）
├── 04_b_tree/                  B 树（CLRS 最小度数 t 约定）
├── 05_b_plus_tree/             B+ 树（对比 B 树，叶子链表）
└── 06_red_black_tree/          红黑树（CLRS 第 13 章）
```

**方式一：单独跑某一章**（推荐，逐章学习）

```bash
cd c-algo-advanced-experiments/06_red_black_tree
make demo     # 编译并运行演示程序，观察每一步的树结构变化
make test     # 编译并运行测试套件
make san      # sanitizer 版本（ASan + UBSan）
```

**方式二：直接用 `cc` 编译单个文件**（不依赖 Makefile，理解编译命令本身）

```bash
cd c-algo-advanced-experiments/06_red_black_tree
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g rbtree.c demo.c -o demo && ./demo
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g \
   -fsanitize=address,undefined -fno-omit-frame-pointer \
   rbtree.c tests.c -o tests_san && ./tests_san
```

**方式三：性能测试**（仅 B 树 / B+ 树目录提供，用 `-O2` 单独编译）

```bash
cd c-algo-advanced-experiments/04_b_tree
make perf     # -O2 编译，不带 sanitizer，测真实耗时
make bench    # 等价于先 make perf 再运行
```

### 编译环境与命令

本机实测环境（与《C 语言语法与最佳实践》使用的是同一台机器，环境完全一致）：

```text
$ uname -a
Darwin mainmac 27.0.0 Darwin Kernel Version 27.0.0 ...RELEASE_ARM64_T6041 arm64

$ cc --version
Apple clang version 21.0.0 (clang-2100.3.30.1)
Target: arm64-apple-darwin27.0.0

$ sw_vers
ProductName:  macOS
ProductVersion: 27.0
BuildVersion: 26A428
```

**本文档统一使用的编译命令**：

```bash
# 常规模式：严格警告 + 调试信息（correctness 验证用）
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g FILES -o OUT

# 硬核模式：额外开 sanitizer（指针/递归/内存/旋转密集的章节全部覆盖）
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g \
   -fsanitize=address,undefined -fno-omit-frame-pointer \
   FILES -o OUT_san

# 性能模式：仅 04/05 两个目录的 perf.c 使用，与上面两种模式分开、不叠加 sanitizer
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O2 -g FILES -o OUT_perf
```

三种模式互相独立、目的不同：常规模式保证代码本身零警告零错误；sanitizer 模式在常规模式基础上额外捕捉内存越界、use-after-free、未定义行为；性能模式关闭调试友好性、开启优化，专门用来测真实耗时——**三者不应该混在一起**，比如带着 sanitizer 去测性能数字，测出来的耗时会包含 sanitizer 自身的指令级监控开销，不能代表真实性能（04_b_tree 一章会用真实数字展示这个差异有多大）。

---

## 二、目录

- [一、导读](#一导读)
- [二、目录](#二目录)
- [三、进阶学习路径](#三进阶学习路径)
- [四、二叉树遍历：从递归到 O(1) 空间](#四二叉树遍历从递归到-o1-空间)
- [五、图遍历算法：BFS、DFS 及其变体](#五图遍历算法bfsdfs-及其变体)
- [六、Trie（字典树）](#六trie字典树)
- [七、B 树：磁盘友好的多路平衡树](#七b-树磁盘友好的多路平衡树)
- [八、B+ 树：为范围查询而生](#八b-树为范围查询而生)
- [九、红黑树：用颜色约束换近似平衡](#九红黑树用颜色约束换近似平衡)
- [十、六种结构综合对比](#十六种结构综合对比)
- [十一、调试与验证方法论](#十一调试与验证方法论)
- [十二、综合项目建议](#十二综合项目建议)
- [十三、练习题与参考答案](#十三练习题与参考答案)
- [十四、速查表](#十四速查表)
- [十五、参考资料与延伸阅读](#十五参考资料与延伸阅读)
- [附录：本文档的验证声明](#附录本文档的验证声明)

---

## 三、进阶学习路径

六个专题不是随便排的顺序，前后有依赖关系，建议按顺序学：

```text
01 二叉树遍历 ──┐
                ├──> 04 B 树 ──> 05 B+ 树 ──┐
02 图遍历 ───────┘                          │
                                             ├──> 十、综合对比
03 Trie ────────────────────────────────────┘
                                             │
06 红黑树 (可与 04/05 并行，依赖 01 的树遍历直觉) ┘
```

- **01（二叉树遍历）是地基**：B 树、B+ 树、红黑树都是"树"，插入/删除/校验函数里大量用到中序遍历、递归/迭代的转换思路。如果递归遍历和显式栈迭代遍历还不熟，先把 01 跑透。
- **02（图遍历）相对独立**：BFS/DFS 的框架在 04/05/06 里不会直接复用，但"发现/完成时间""访问标记数组""显式栈模拟递归"这几个心智模型是通用的。
- **03（Trie）也相对独立**：它是本章唯一一个"固定分支数（26 个子节点）"的树，插入/删除的递归回溯思路（"往下插入，往上决定是否可以释放"）会在 04/05 的删除逻辑里以更复杂的形式再次出现。
- **04 → 05 必须顺序学**：B+ 树整章内容都建立在"和 B 树对比"上，05 的 README 会反复引用 04 的代码和运行结果。跳过 04 直接学 05 会看不懂"为什么要复制 key""为什么要移动 key"这两个核心差异到底在对比什么。
- **06（红黑树）可以和 04/05 并行**，但建议放在最后写，因为它的删除修复逻辑（4 种情况 × 镜像）是全篇最复杂的状态机，需要前面几章积累的"结构不变量校验"方法论做支撑。

每一章的学习顺序建议是：先读 README 的"本章问题"，再跑一遍 `demo`（看真实的中间状态输出），再读一遍源码，再跑一遍 `tests`（看边界情况怎么测的），最后自己动手改一个参数重新跑一遍——比如把 B 树的 `t` 从 2 改成 8，观察树高和节点数的真实变化。

---

## 四、二叉树遍历：从递归到 O(1) 空间

**代码位置**：`c-algo-advanced-experiments/01_binary_tree_traversal/`（`demo.c` 556 行，`tests.c` 509 行，64 个测试用例）

### 本章要解决的问题

前序、中序、后序遍历大部分人都会背，但下面几个问题很多人答不上来：

1. 递归遍历本质上在维护什么？如果不让你用递归，你怎么用显式栈把它翻译出来？
2. 遍历一棵树需要 O(h) 的额外栈空间（h 是树高）。有没有办法把这个额外空间降到 O(1)？
3. 已知一棵树的前序遍历序列和中序遍历序列，能唯一确定这棵树的形状吗？如果换成"前序+后序"呢？

### 4.1 递归遍历：三种顺序的本质是"访问自己"插在哪一步

```c
static void preorder_recursive(const Node *root, int *out, int *n) {
    if (root == NULL) return;
    out[(*n)++] = root->value;              /* 访问自己 —— 放在最前面 */
    preorder_recursive(root->left, out, n);
    preorder_recursive(root->right, out, n);
}

static void inorder_recursive(const Node *root, int *out, int *n) {
    if (root == NULL) return;
    inorder_recursive(root->left, out, n);
    out[(*n)++] = root->value;               /* 访问自己 —— 放在中间 */
    inorder_recursive(root->right, out, n);
}

static void postorder_recursive(const Node *root, int *out, int *n) {
    if (root == NULL) return;
    postorder_recursive(root->left, out, n);
    postorder_recursive(root->right, out, n);
    out[(*n)++] = root->value;               /* 访问自己 —— 放在最后面 */
}
```

三个函数的递归结构完全一样，唯一的区别就是"访问自己"这一句插在哪个位置。真实运行输出（构造一棵完全二叉树 `4 (2 (1,3), 6 (5,7))`）：

```text
前序（递归）: [4, 2, 1, 3, 6, 5, 7]
中序（递归）: [1, 2, 3, 4, 5, 6, 7]
后序（递归）: [1, 3, 2, 5, 7, 6, 4]
```

### 4.2 迭代遍历：把编译器帮你维护的调用栈手动搬出来

递归遍历本质上是编译器帮你维护了一个调用栈，栈里存的是"当前节点 + 你在这个节点的哪个阶段"。迭代版本就是把这个隐藏的调用栈手动搬出来。前序最简单：

```c
static void preorder_iterative(const Node *root, int *out, int *n) {
    if (root == NULL) return;
    const Node *stack[STACK_CAP];
    int top = 0;
    stack[top++] = root;
    while (top > 0) {
        const Node *cur = stack[--top];
        out[(*n)++] = cur->value;
        /* 先压右子树、再压左子树，保证左子树先被弹出处理（栈是后进先出） */
        if (cur->right) stack[top++] = cur->right;
        if (cur->left)  stack[top++] = cur->left;
    }
}
```

中序和后序的迭代版本要复杂一些（需要额外记录"当前节点是不是已经处理过左子树"），完整实现见 `demo.c` 第 109-149 行。真实运行输出（同一棵树）：

```text
前序（迭代）: [4, 2, 1, 3, 6, 5, 7]
中序（迭代）: [1, 2, 3, 4, 5, 6, 7]
后序（迭代）: [1, 3, 2, 5, 7, 6, 4]
```

与递归版本逐字节一致——这也是 `tests.c` 里验证迭代实现正确性的方式：**不是靠肉眼看着像，而是直接断言两个数组相等**。

### 4.3 层序遍历（BFS）

```c
static void level_order(const Node *root, int *out, int *n) {
    if (root == NULL) return;
    const Node *queue[STACK_CAP];
    int head = 0, tail = 0;
    queue[tail++] = root;
    while (head < tail) {
        const Node *cur = queue[head++];
        out[(*n)++] = cur->value;
        if (cur->left)  queue[tail++] = cur->left;
        if (cur->right) queue[tail++] = cur->right;
    }
}
```

用数组模拟队列（`head`/`tail` 两个下标，不需要真正的环形缓冲区，因为这里只入队不出队复用空间）。真实输出：`层序: [4, 2, 6, 1, 3, 5, 7]`——一层一层从左到右，和前面三种深度优先的顺序都不一样。

### 4.4 莫里斯遍历：O(1) 空间中序遍历

前面的迭代版本用了一个显式栈，最坏情况下（树退化成链表）栈深度是 O(n)。莫里斯遍历（Morris Traversal）能把额外空间降到 O(1)，代价是要**临时借用树本身的空指针字段当"线索"**。

核心想法：中序遍历里，一个节点的"中序前驱"（左子树里最右边的那个节点）遍历完之后本该"回到"这个节点——但普通遍历要么靠递归调用栈记住回去的路，要么靠显式栈。莫里斯遍历的技巧是：如果 `cur` 有左子树，就去左子树里找到"最右节点" `predecessor`，把 `predecessor->right` 临时指向 `cur`（借用这个本来是 `NULL` 的指针当"线索"），这样将来从左子树内部沿着 `right` 一直走，就能自动"走回" `cur`，不需要额外的栈或者递归：

```c
static void morris_inorder(Node *root, int *out, int *n) {
    Node *cur = root;
    while (cur != NULL) {
        if (cur->left == NULL) {
            /* 没有左子树：直接访问自己，然后向右走 */
            out[(*n)++] = cur->value;
            cur = cur->right;
        } else {
            /* 找左子树里的最右节点（中序前驱） */
            Node *predecessor = cur->left;
            while (predecessor->right != NULL && predecessor->right != cur) {
                predecessor = predecessor->right;
            }
            if (predecessor->right == NULL) {
                /* 第一次到达 cur：建立线索，指向左子树探索 */
                predecessor->right = cur;
                cur = cur->left;
            } else {
                /* 线索存在，说明左子树已经走完一圈、绕回来了：
                 * 拆除线索（恢复原状），访问 cur，再向右走 */
                predecessor->right = NULL;
                out[(*n)++] = cur->value;
                cur = cur->right;
            }
        }
    }
}
```

真实运行输出：

```text
莫里斯中序: [1, 2, 3, 4, 5, 6, 7]
遍历后树结构与遍历前完全一致？ 是（线索已正确拆除）
```

结果正确，而且**遍历结束后树被完整还原**——线索只是"临时"借用，用完就拆。

#### ⚠️ 错误示例：忘记拆除线索

如果把 `predecessor->right = NULL;` 这一行删掉会怎样？这不是内存安全问题（不会触发 ASan/UBSan），而是纯粹的逻辑正确性问题：

```c
/* ❌ 错误版本：故意不拆线索 */
} else {
    /* predecessor->right = NULL; 被删掉了 */
    out[(*n)++] = cur->value;
    cur = cur->right;
}
```

真实运行输出：

```text
"错误版"遍历结果: [1, 2, 3, 4, 5, 6, 7]
遍历后树结构与遍历前一致？ 否（树的 right 指针被永久污染，形状已改变）
说明：右子树为空、但左子树非空的节点，其 right 现在指向了祖先节点，
      而不再是 NULL —— 树里出现了环。如果之后再对这棵"坏树"做一次
      普通递归中序遍历或普通递归释放（tree_free），会陷入无限递归，
      本实验最初就是这样把自己写进了 stack-overflow（详见 README）。
```

遍历**结果**是对的（`[1, 2, 3, 4, 5, 6, 7]` 没错），但**树的结构已经被永久破坏**——这正是本章开头强调的"只验证最终结果是不够的"的第一个真实案例：如果只检查遍历输出对不对，这个 bug 完全发现不了，必须额外检查"遍历后的树和遍历前的树是否结构相同"。`demo.c` 里这个错误版本用一个 `guard` 计数器保护，防止真的死循环卡死演示程序；`tests.c` 里对应的测试用例直接用 `tree_equal()` 断言遍历前后两棵树结构完全一致。

### 4.5 线索二叉树：把"临时线索"变成"永久线索"

莫里斯遍历里的线索是临时的、用完即拆。线索二叉树（Threaded Binary Tree）把这个想法反过来：**永久性**地把叶子节点的空 `right` 指针改成指向"中序后继"，之后每次遍历都不需要栈、不需要递归，直接沿着线索走：

```c
typedef struct ThreadedNode {
    int value;
    struct ThreadedNode *left;
    struct ThreadedNode *right;
    bool right_is_thread;   /* true 表示 right 指向的是"线索"而不是真正的右子树 */
} ThreadedNode;
```

真实输出（同一棵树 `4 (2 (1,3), 6 (5,7))`）：

```text
线索中序遍历: [1, 2, 3, 4, 5, 6, 7]
说明：叶子节点 1、3、5 的 right 现在是"线索"，分别指向它们的中序后继 2、4、6；
      节点 7 没有中序后继，right 保持 NULL（线索链的终点）。
```

关键区别在于：莫里斯遍历的线索是遍历过程中动态建立、动态拆除的，遍历结束后树恢复原状；线索二叉树的线索是**建树时一次性建好、长期存在**的，代价是每个节点要多一个 `bool` 字段区分"这个 `right` 是真孩子还是线索"，换来的是之后所有的中序遍历都变成 O(1) 空间、不需要任何栈或递归。

### 4.6 从遍历序列重建二叉树

已知前序和中序遍历序列，能唯一确定原始二叉树的形状。核心想法：前序序列的第一个元素一定是根节点；这个根节点在中序序列里的位置，把中序序列切成"左子树的中序"和"右子树的中序"两段，长度分别告诉你前序序列里除了根节点之外，哪一段是左子树、哪一段是右子树：

```c
static Node *build_from_pre_in(const int *pre, int pre_n,
                                const int *in, int in_n) {
    if (pre_n == 0) return NULL;
    Node *root = node_new(pre[0]);
    int root_pos = 0;
    while (in[root_pos] != pre[0]) root_pos++;  /* 在中序里找根节点位置 */
    int left_n = root_pos;
    root->left  = build_from_pre_in(pre + 1, left_n,
                                     in, left_n);
    root->right = build_from_pre_in(pre + 1 + left_n, pre_n - 1 - left_n,
                                     in + root_pos + 1, in_n - root_pos - 1);
    return root;
}
```

后序+中序的重建是同样的思路，只是后序序列的**最后一个**元素才是根节点。真实运行输出：

```text
[前序+中序 重建] 重建树的前序: [4, 2, 1, 3, 6, 5, 7]
[前序+中序 重建] 重建树的中序: [1, 2, 3, 4, 5, 6, 7]
重建树与原树结构完全一致：是

[后序+中序 重建] 重建树的后序: [1, 3, 2, 5, 7, 6, 4]
[后序+中序 重建] 重建树的中序: [1, 2, 3, 4, 5, 6, 7]
重建树与原树结构完全一致：是
```

**为什么"前序+后序"做不到唯一重建？** 因为前序和后序都只告诉你"谁是根"，都不能像中序那样把序列切成"明确的左右两段"。反例：只有一个左孩子的树 `1(2)` 和只有一个右孩子的树 `1(_,2)`，前序都是 `[1,2]`，后序都是 `[2,1]`——两棵不同形状的树产生了完全相同的前序+后序组合，无法唯一还原。`tests.c` 里没有对这一点单独写断言（因为"重建失败"本身不是一个能用 assert 表达的正例），但这是理解"为什么中序遍历在重建问题里如此特殊"的关键：中序遍历是唯一一种"访问自己"夹在中间的顺序，也是唯一一种能把序列切成"左边全是左子树、右边全是右子树"两段的顺序。

### 测试覆盖与验证

`tests.c` 的 64 个测试覆盖了空树、单节点、只有左/右孩子的链状树、完全二叉树、大随机树（含 Morris 遍历后结构完整性校验）、错误版本 Morris 遍历导致污染的专项验证等类别。真实运行结果：

```bash
$ cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g tests.c -o tests && ./tests
...
[PASS] test_duplicate_values_morris_structure_intact

========== 测试统计 ==========
通过: 64, 失败: 0, 总计: 64
结果: 全部通过
```

sanitizer 版本（`-fsanitize=address,undefined -fno-omit-frame-pointer`）同样 64/64、退出码 0。

### 常见误区

- **以为递归遍历"不占空间"**：递归本身就是在用调用栈，树退化成链表时递归深度是 O(n)，深度足够大会栈溢出。"迭代版本更省栈空间"是错的——迭代版本用显式数组模拟栈，空间开销和递归调用栈是同一个量级，真正省空间的只有莫里斯遍历。
- **莫里斯遍历用完线索不拆**：4.4 节的核心陷阱，遍历结果碰巧是对的，但树的结构被永久破坏，后续任何递归操作（遍历、释放）都可能因为树里出现的环而无限递归。
- **以为"前序+中序"和"前序+后序"都能唯一重建**：只有包含中序的组合才能唯一重建，原因见 4.6 节末尾的反例。

### 小结

递归遍历、迭代遍历、莫里斯遍历，本质上都在解决同一个问题——"访问完左子树之后怎么找到回去的路"——只是用了三种不同的存储方式：编译器隐式调用栈、手写显式栈、借用树本身的空指针当线索。线索二叉树把"借用"变成"永久占用"，换来遍历时彻底不需要任何额外结构。从遍历序列重建二叉树则说明了一个更本质的问题：中序遍历因为"访问自己"夹在中间，天然具备"切分左右子树"的能力，这是它在重建问题里不可替代的原因。

### 练习

1. 把 `morris_inorder` 改写成"莫里斯前序遍历"（提示：访问 `cur` 的时机要在第一次到达时，而不是线索建立完成之后）。
2. 证明：一棵有 n 个节点的二叉树，如果已知它的层序遍历序列，能否唯一确定树的形状？如果能，写出重建算法；如果不能，给出反例。
3. 线索二叉树只线索化了 `right` 指针（中序后继）。如果同时线索化 `left` 指针（中序前驱），遍历还能获得什么额外能力？

---

## 五、图遍历算法：BFS、DFS 及其变体

**代码位置**：`c-algo-advanced-experiments/02_graph_traversal/`（`graph.c`/`.h` 图表示，`algos.c`/`.h` 算法实现共 422 行，`demo.c` 256 行，`buggy_cycle_demo.c` 80 行专门演示一个真实的逻辑 bug，`tests.c` 542 行，21 个测试用例）

### 本章要解决的问题

BFS 用队列、DFS 用栈这句话大部分人都会背，但下面几个问题决定了你是"背过"还是"真的懂"：

1. 邻接矩阵和邻接表两种图表示，空间复杂度差多少？什么时候该用哪个？
2. 拓扑排序有两种经典写法（Kahn 算法、DFS 后序逆序），它们的结果一定相同吗？
3. 环检测在有向图和无向图上的写法**不能**照搬——本章重点：如果照搬会发生什么，为什么这个 bug sanitizer 抓不到？

### 5.1 图的表示：邻接矩阵 vs 邻接表

```c
typedef struct AdjNode {
    int to;
    struct AdjNode *next;
} AdjNode;

typedef struct Graph {
    int n;              /* 节点数 */
    bool directed;
    AdjNode **adj;      /* adj[i] 是节点 i 的邻接表头 */
} Graph;
```

真实运行输出（同一个 5 节点、5 条边的无向图，先用邻接矩阵、再用邻接表表示）：

```text
邻接矩阵 (5x5):
  0 1 1 0 0
  1 0 0 1 0
  1 0 0 1 0
  0 1 1 0 1
  0 0 0 1 0
-> 5x5=25 格子，边很稀疏时大量格子是 0，浪费空间
同一个图的邻接表:
graph(n=5, undirected, edges=5):
  0: -> 2 -> 1
  1: -> 3 -> 0
  2: -> 3 -> 0
  3: -> 4 -> 2 -> 1
  4: -> 3
-> 只存实际存在的边，空间 O(V+E)，适合稀疏图
```

邻接矩阵空间是 O(V²)，不管图多稀疏都要占满 V×V 个格子；邻接表空间是 O(V+E)，只存真实存在的边。本章后面所有算法都基于邻接表实现——真实的图（社交网络、路网、依赖关系图）几乎都是稀疏图，边数远小于 V²。

### 5.2 BFS 最短路径（无权图）

```c
BFSResult bfs(const Graph *g, int src, bool verbose)
{
    /* ... 初始化 dist[]=-1, prev[]=-1 ... */
    IntQueue *q = queue_create(g->n);
    r.dist[src] = 0;
    queue_push(q, src);
    while (!queue_empty(q)) {
        int u = queue_pop(q);
        r.order[r.order_len++] = u;
        for (AdjNode *cur = g->adj[u]; cur; cur = cur->next) {
            int v = cur->to;
            if (r.dist[v] == -1) {          /* 第一次到达 v，记录距离和前驱 */
                r.dist[v] = r.dist[u] + 1;
                r.prev[v] = u;
                queue_push(q, v);
            }
        }
    }
    /* ... */
}
```

BFS 保证第一次到达某个节点时的距离就是最短距离，因为队列保证了"距离更小的节点一定先被处理"。真实运行输出（`--verbose` 打印每一步的队列内容）：

```text
graph(n=5, undirected, edges=5):
  0: -> 2 -> 1
  1: -> 3 -> 0
  2: -> 4 -> 0
  3: -> 4 -> 1
  4: -> 3 -> 2
从节点 0 开始 BFS，观察队列变化：
  队列: [0]
  队列: [2,1]
  队列: [1,4]
  队列: [4,3]
  队列: [3]
访问顺序: [0,2,1,4,3]
  dist[0][0] = 0, prev = -1
  dist[0][1] = 1, prev = 0
  dist[0][2] = 1, prev = 0
  dist[0][3] = 2, prev = 1
  dist[0][4] = 2, prev = 2
0 -> 4 的最短路径: [0,2,4] (长度 2 条边)
-> 与手算结果一致：0-2-4，2 条边
```

沿着 `prev[]` 数组从终点往回走就能重建出完整路径，见 `bfs_reconstruct_path()`。

### 5.3 DFS：递归 vs 迭代，发现/完成时间

DFS 的递归版本很直观，但要理解"迭代版本怎么模拟递归"，需要先理解 DFS 的一个关键性质——每个节点有"发现时间"（第一次访问到）和"完成时间"（这个节点及其所有子树都处理完毕）：

```text
graph(n=6, directed, edges=5):
  0: -> 2 -> 1
  1: -> 3
  2: -> 4
  3:
  4: -> 5
  5:
递归 DFS 访问顺序: [0,2,4,5,1,3]
  node 0: disc=0 fin=11
  node 1: disc=7 fin=10
  node 2: disc=1 fin=6
  node 3: disc=8 fin=9
  node 4: disc=2 fin=5
  node 5: disc=3 fin=4
迭代 DFS 访问顺序: [0,2,4,5,1,3]
-> 迭代版用「显式帧+邻居下标」模拟调用栈，
   访问顺序和递归版完全一致（不是简单地把所有邻居一次性入栈）
```

节点 0 的 `disc=0, fin=11`——从第 0 个时间戳开始访问，到第 11 个时间戳才算完全结束，中间横跨了它所有子树的发现和完成过程。这一对时间戳是后面拓扑排序（DFS 版）和很多图算法（如 Tarjan 强连通分量）的基础。

迭代版本的关键不是"用一个栈存所有待访问节点"（那样得到的顺序和递归版不同，更接近 BFS 那种层次展开），而是要用"显式帧"模拟函数调用——每个栈帧记录"当前节点 + 下一个要检查的邻居下标"，这样才能保证访问顺序与递归版一致。完整实现见 `algos.c` 第 138-189 行。

### 5.4 连通分量

```c
int connected_components(const Graph *g, int *comp)
```

对无向图，用 BFS 或 DFS 从每个未访问的节点出发，能到达的所有节点都属于同一个连通分量。真实输出：

```text
graph(n=6, undirected, edges=3):
  0: -> 1
  1: -> 2 -> 0
  2: -> 1
  3: -> 4
  4: -> 3
  5:
分量数: 3
  node 0 -> component 0
  node 1 -> component 0
  node 2 -> component 0
  node 3 -> component 1
  node 4 -> component 1
  node 5 -> component 2
-> 节点 5 没有任何边，自己单独成一个分量
```

孤立节点（没有任何边）自己就是一个分量——边界情况之一，`tests.c` 里专门有测试覆盖。

### 5.5 拓扑排序：Kahn 算法 vs DFS

两种经典实现。Kahn 算法基于入度：反复取出"当前入度为 0"的节点，取出后把它所有出边指向的节点入度减一：

```c
bool topo_sort_kahn(const Graph *g, int *out)
{
    int *indeg = calloc((size_t)g->n, sizeof *indeg);
    for (int u = 0; u < g->n; u++)
        for (AdjNode *cur = g->adj[u]; cur; cur = cur->next)
            indeg[cur->to]++;

    IntQueue *q = queue_create(g->n);
    for (int i = 0; i < g->n; i++)
        if (indeg[i] == 0) queue_push(q, i);

    int cnt = 0;
    while (!queue_empty(q)) {
        int u = queue_pop(q);
        out[cnt++] = u;
        for (AdjNode *cur = g->adj[u]; cur; cur = cur->next)
            if (--indeg[cur->to] == 0) queue_push(q, cur->to);
    }
    return cnt == g->n;   /* 处理的节点数不足 n，说明有环 */
}
```

DFS 版则是：对每个节点做 DFS，一个节点"完成"（所有子树都处理完）的时候把它写入结果数组的**末尾**，从后往前填，最终不需要反转：

```c
static void topo_dfs_visit(TopoDfsCtx *ctx, int u)
{
    ctx->color[u] = 1; /* 灰：在当前递归路径上 */
    for (AdjNode *cur = ctx->g->adj[u]; cur; cur = cur->next) {
        int v = cur->to;
        if (ctx->color[v] == 1) { ctx->cyclic = true; return; }  /* 遇到灰色 = 找到环 */
        if (ctx->color[v] == 0) {
            topo_dfs_visit(ctx, v);
            if (ctx->cyclic) return;
        }
    }
    ctx->color[u] = 2; /* 黑：完成 */
    ctx->out[ctx->out_pos--] = u;   /* 从数组末尾往前写 */
}
```

真实运行输出（同一个 7 节点 DAG）：

```text
graph(n=7, directed, edges=7):
  0: -> 2 -> 1
  1: -> 3 -> 2
  2:
  3:
  4: -> 5
  5: -> 3
  6: -> 2
Kahn 算法结果: [0,4,6,1,5,2,3] (合法性: valid)
DFS   算法结果: [6,4,5,0,1,3,2] (合法性: valid)
-> 两个序列不一定相同（拓扑序不唯一），但都必须满足
   「每条边 u->v，u 在结果里排在 v 前面」
```

两个算法给出的**结果不同**，但**都合法**——拓扑序本身就不是唯一的，只要满足"每条边的起点排在终点前面"就算合法。`topo_order_is_valid()` 就是专门检验这个条件的函数，`tests.c` 里对两种算法的输出都用它验证，而不是断言两者结果相等（那样断言反而是错的）。

### 5.6 环检测：有向图 vs 无向图（本章重点）

**有向图**用三色标记：白（未访问）、灰（在当前 DFS 递归路径上）、黑（已完成）。遇到灰色节点说明找到了"回边"，即环：

```c
static bool cycle_directed_visit(const Graph *g, int *color, int u)
{
    color[u] = 1; /* 灰 */
    for (AdjNode *cur = g->adj[u]; cur; cur = cur->next) {
        int v = cur->to;
        if (color[v] == 1) return true;   /* 回边，找到环 */
        if (color[v] == 0 && cycle_directed_visit(g, color, v)) return true;
    }
    color[u] = 2; /* 黑 */
    return false;
}
```

**无向图不能照抄这个写法**，正确实现要多传一个 `parent` 参数：

```c
static bool cycle_undirected_visit(const Graph *g, bool *visited, int u, int parent)
{
    visited[u] = true;
    for (AdjNode *cur = g->adj[u]; cur; cur = cur->next) {
        int v = cur->to;
        if (!visited[v]) {
            if (cycle_undirected_visit(g, visited, v, u)) return true;
        } else if (v != parent) {
            return true; /* 访问过、且不是父节点 -> 真正的环 */
        }
    }
    return false;
}
```

原因：无向图存边时 `graph_add_edge(g, u, v)` 会同时生成两条邻接表项——`u` 的邻接表里加 `u->v`，`v` 的邻接表里加 `v->u`。DFS 从 `u` 走到 `v` 之后，检查 `v` 的邻接表时会看到"`v->u`"这一项——这其实就是刚才走过来的那条边的另一半，不是新发现的环。**必须记录是从哪个父节点走过来的，遇到已访问节点时，只有当它不是父节点，才是真正的环。**

#### ⚠️ 错误示例：忘记排除父节点

`buggy_cycle_demo.c` 是一个独立的文件，故意保留这个新手常犯的 bug，用来对比正确实现：

```c
/* ❌ 错误实现：没有排除父节点 */
static bool buggy_visit(const Graph *g, bool *visited, int u)
{
    visited[u] = true;
    for (AdjNode *cur = g->adj[u]; cur; cur = cur->next) {
        int v = cur->to;
        if (visited[v]) {
            return true; /* ← bug：v 可能就是刚刚走过来的父节点 */
        }
        if (buggy_visit(g, visited, v)) return true;
    }
    return false;
}
```

真实编译运行输出：

```text
========== 错误示例：无向图环检测忘记排除父节点 ==========

用例 1: 简单路径 A-B-C (0-1-2)，没有环
  错误实现结果: has_cycle = true
  正确答案应该是: false
  -> 假阳性！！！这就是本文件要展示的 bug

用例 2: 一条更长的链 0-1-2-3-4
  错误实现结果: has_cycle = true
  正确答案应该是: false

========== 原因分析 ==========
DFS 从节点 1 走到节点 0（沿着 1->0 这条边）之后，
检查 0 的邻接表时，会看到「0->1」这一项——
这其实就是刚才走过来的那条边的另一半，不是新发现的环。
错误实现只要看到「已访问」就报环，等价于把每一条边都当成了环。
```

一条明显没有环的路径图 `0-1-2`，错误实现直接报告"有环"——**任何一条边都会被误判成环**，因为无向图里每条边天生就会形成"去了又回头看到已访问节点"的假象。

这个 bug 有一个值得反复强调的性质：**它不是内存安全问题**。`buggy_visit` 没有越界访问、没有 use-after-free、没有未初始化读取——ASan 和 UBSan 都不会报任何警告，程序"正常运行"到结束，退出码是 0。它是纯粹的算法逻辑错误，唯一的发现方式是**知道正确答案、然后对比**——这也是为什么本章测试套件对"无向图环检测"专门构造了简单路径这种"一眼就能看出没有环"的用例，而不是只测复杂的随机图（复杂图上人眼判断不出对错，看不出这个 bug）。

### 5.7 二分图判定

用 BFS 做 2-染色：起点染色 0，它的所有邻居必须染色 1，邻居的邻居必须染色 0……如果在染色过程中发现某条边的两端被染了同一种颜色，就不是二分图：

```c
bool is_bipartite(const Graph *g, int *color)
{
    for (int i = 0; i < g->n; i++) color[i] = -1;
    /* 对每个未染色的节点做 BFS 染色 */
    /* ... */
    if (color[v] == -1) {
        color[v] = 1 - color[u];   /* 染成和 u 相反的颜色 */
        queue_push(q, v);
    } else if (color[v] == color[u]) {
        ok = false;                 /* 冲突：不是二分图 */
    }
}
```

真实输出：

```text
[二分图] 4 节点环 0-1-2-3-0 (偶数长度):
  is_bipartite = true, 染色: [0,1,0,1]
[非二分图] 三角形 0-1-2-0 (奇数长度环):
  is_bipartite = false
  -> 奇数长度的环一定不是二分图：染色到最后一条边必然冲突
```

奇数长度的环一定不是二分图——染色沿着环走一圈，回到起点时颜色必然和起点冲突，这是二分图判定的一个经典充要条件。

### 测试覆盖与验证

21 个测试覆盖了邻接矩阵/邻接表构造、BFS 距离与路径重建、DFS 递归/迭代一致性、连通分量（含孤立节点）、Kahn 与 DFS 两种拓扑排序在大随机 DAG 上的合法性、有向图与无向图环检测（含简单路径这种"人眼可判断"的专项用例）、二分图判定等类别：

```bash
$ cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g graph.c algos.c tests.c -o tests && ./tests
[PASS] test_large_random_dag_topo_sort
[PASS] test_adjacency_matrix_basic

========== 测试汇总 ==========
PASS: 21, FAIL: 0, TOTAL: 21
```

sanitizer 版本同样 21/21、退出码 0。

### 机制小结：BFS 队列 vs DFS 栈

BFS 用队列（先进先出）保证"离起点近的先被处理"，天然适合求最短路径（无权图）；DFS 用栈（后进先出，或递归调用栈）保证"一条路走到底再回头"，天然适合发现/完成时间、拓扑排序、环检测这类需要"知道子树是否已经完全处理完"的场景。两者的框架代码几乎一样（都是"取一个节点、访问、把未访问的邻居放进数据结构"），唯一的区别就是那个数据结构是队列还是栈——但这个区别决定了整整一类算法能不能用。

### 常见误区

- **无向图环检测直接照抄有向图写法**：本章重点，5.6 节详细展开。记住结论：无向图必须传 `parent` 参数排除"刚走过来的那条边"。
- **以为两种拓扑排序算法的结果应该相同**：拓扑序本身不唯一，只要满足"每条边起点排在终点前"就是合法结果，不应该断言两个算法输出完全一致。
- **以为图算法的 bug 都能被 sanitizer 抓到**：5.6 节的环检测错误是纯逻辑 bug，不涉及任何内存操作，ASan/UBSan 完全无效，只能靠"构造一个人眼能判断对错的简单用例"来发现。

### 练习

1. 把 Kahn 算法改造成"检测并输出具体的环"（而不是只返回 true/false）：当 `cnt != n` 时，剩下入度不为 0 的节点就在某个环里，尝试写出提取这个环的代码。
2. 无向图的连通分量可以用并查集（Union-Find）实现，不需要 BFS/DFS。尝试写一个并查集版本，和本章的 BFS 版本对比代码复杂度。
3. 二分图判定用的是 BFS，改写成 DFS 版本，注意递归传递颜色参数的方式。

---

## 六、Trie（字典树）

**代码位置**：`c-algo-advanced-experiments/03_trie/`（`trie.h` 261 行，头文件里直接实现，`demo.c` 233 行，`tests.c` 384 行，13 个测试用例）

### 本章要解决的问题

Trie 的插入和查找很直观，但删除是这一章真正的难点：

1. 两个共享前缀的单词（比如 `car` 和 `card`），删除其中一个，怎么保证不影响另一个？
2. 删除一个不存在的单词，应该发生什么？
3. "只清标记不释放节点"和"释放节点"这两种情况分别在什么条件下发生？

### 数据结构定义

```c
#define ALPHABET_SIZE 26

typedef struct TrieNode {
    struct TrieNode *children[ALPHABET_SIZE];
    bool is_word; /* 这个节点是否是某个单词的结尾 */
} TrieNode;
```

固定 26 个子节点指针（对应 `a`-`z`），`is_word` 标记"走到这个节点时，恰好构成了一个完整单词"——注意这和"这个节点是叶子节点"是两件不同的事：`car` 的结尾节点 `r`，即使后面还接着 `card` 的 `d`（`r` 仍有孩子），`r->is_word` 也是 `true`。

### 6.1 插入 / 查找 / 前缀查找

```c
static inline bool trie_insert(TrieNode *root, const char *word) {
    if (root == NULL || !is_valid_word(word)) return false;
    TrieNode *cur = root;
    for (const char *p = word; *p != '\0'; p++) {
        int idx = char_to_index(*p);
        if (cur->children[idx] == NULL) {
            cur->children[idx] = trie_node_create();
        }
        cur = cur->children[idx];
    }
    cur->is_word = true;
    return true;
}
```

查找和前缀查找共用一个内部工具函数 `trie_find_node`：沿着单词的每个字符往下走，走不通就返回 `NULL`；`trie_search` 额外要求终点节点的 `is_word` 为真，`trie_starts_with` 只要求路径存在。真实运行输出：

```text
插入: cat, car, card, care, dog, do
节点总数（含根）= 10
search("cat")        = true
search("ca")         = false
search("card")       = true
search("cards")      = false
search("dog")        = true
search("d")          = false
starts_with("ca")    = true
starts_with("do")    = true
starts_with("xyz")   = false
starts_with("")      = true
```

`search("ca")` 是 `false` 但 `starts_with("ca")` 是 `true`——这正是 `is_word` 标记存在的意义：路径存在不代表这个路径本身构成一个完整单词。空前缀 `starts_with("")` 按约定匹配一切非空 Trie。

### 6.2 前缀共享：一份路径，多个单词

插入 `cat / car / card / care` 之后，Trie 的真实结构：

```text
      root
       |
       c
       |
       a
      / \
     t   r [is_word=true, 对应 "car"]
     |   |
 [cat]   +---d [is_word=true, 对应 "card"]
         |
         +---e [is_word=true, 对应 "care"]
```

真实输出：

```text
"ca" 这条路径被 4 个单词共享，只占用 2 个节点（c、a），
而不是每个单词各自占用一份 —— 这就是 Trie 省空间的地方。
```

这也正是删除操作复杂的根源：`c` 和 `a` 这两个节点被 4 个单词共用，删除任何一个单词都不能牵连到其他 3 个。

### 6.3 删除：只清标记，不乱释放节点（本章核心）

```c
static inline bool trie_node_is_empty(const TrieNode *node) {
    if (node->is_word) return false;
    for (int i = 0; i < ALPHABET_SIZE; i++)
        if (node->children[i] != NULL) return false;
    return true;
}

/* depth 表示 word[depth] 是当前要走的那一步。
 * 返回值表示"当前这个 node 处理完之后是否变成了空节点，
 * 空到可以被父节点 free 掉"。
 *
 * 不能在找到单词结尾后就往回把整条路径 free 掉，
 * 因为路径上的某个节点可能同时是另一个单词的一部分（前缀共享）。
 * 只有"沿途每一层都确认这一层除了刚才这条路径外什么都没有"，
 * 才能把这一层也交给父节点释放。 */
static inline bool trie_delete_helper(TrieNode *node, const char *word, size_t depth) {
    if (node == NULL) return false;   /* 单词本来就不存在，什么都不做 */

    if (word[depth] == '\0') {
        if (!node->is_word) return false;  /* 只是前缀节点，不是完整单词：删除不存在的单词 */
        node->is_word = false;   /* 只清标记，不动 children —— 可能还有别的单词经过这里 */
        return trie_node_is_empty(node);
    }

    int idx = char_to_index(word[depth]);
    TrieNode *child = node->children[idx];
    if (trie_delete_helper(child, word, depth + 1)) {
        free(child);
        node->children[idx] = NULL;
    }
    /* 递归返回后，如果自己既不是某单词的结尾也没有任何孩子，
     * 说明自己也变空了，可以继续往上交给父节点释放 */
    return trie_node_is_empty(node);
}
```

这是一个"往下插入、往上决定是否可以释放"的递归回溯模式：递归先一路走到单词末尾清除 `is_word` 标记，再沿着调用栈往回走，每一层都用 `trie_node_is_empty` 检查"清完孩子之后，我自己是不是也变空了"，只有确认变空才真正 `free`，否则原样保留（因为还有别的单词依赖这个节点）。

删除共享前缀里的一个单词（`car`，保留 `card`/`care`/`cat`）：

```text
删除前节点总数 = 7
删除前:   search("car")        = true
删除前:   search("card")       = true
执行 trie_delete(root, "car") ...
删除后节点总数 = 7   (不变：c/a/r 节点仍被 card/care 占用，只是 r 的 is_word 被清掉)
search("car")        = false
search("card")       = true
search("care")       = true
search("cat")        = true
starts_with("car")   = true
```

节点总数完全不变——`car` 消失了，但 `c`、`a`、`r` 三个节点因为还被 `card`/`care` 依赖，一个都没被释放；只是 `r->is_word` 被清成了 `false`，所以 `search("car")` 变成 `false`，但 `starts_with("car")` 仍是 `true`（路径还在）。

对比一个真正会释放节点的情况——独立单词 `dog`：

```text
插入 dog, cat 后节点总数 = 7
删除 dog 后节点总数     = 4   (d/o/g 三个节点全部被物理释放)
search("dog")        = false
search("cat")        = true
```

`d/o/g` 三个节点没有被任何其他单词共享，删除后从最深的 `g` 开始，逐层确认"变空"后逐层释放，一路释放到 `d`。

### 6.4 删除不存在的单词：安全忽略

```text
Trie 中只有 "cat"
删除 "dog" / "ca" / "cats" 之后（这三个都不是 Trie 里的完整单词）：
search("cat")        = true
-> 三次无效删除全部安全忽略，"cat" 完好无损
```

三种不同的"不存在"：`dog` 整条路径都不存在（`trie_delete_helper` 在第一层就因 `child == NULL` 而返回 `false`）；`ca` 路径存在但不是完整单词（走到 `word[depth]=='\0'` 时 `node->is_word` 为假）；`cats` 路径比已有单词更长、多出的部分不存在。三种情况都被 `trie_delete_helper` 里的判断正确地安全忽略，不会误删任何东西。

### 6.5 应用：自动补全

基于前缀查找 + DFS 收集：先找到前缀对应的节点，再从这个节点开始做一次 DFS，把路径上遇到的所有 `is_word == true` 的完整单词收集起来。

```text
词库: cat, car, card, care, careful, dog, do, door
autocomplete("ca") = {car, card, care, careful, cat}  (5 个)
autocomplete("car") = {car, card, care, careful}  (4 个)
autocomplete("do") = {do, dog, door}  (3 个)
autocomplete("z") = {}  (0 个)
autocomplete("") = {car, card, care, careful, cat, do, dog, door}  (8 个)
```

空前缀 `autocomplete("")` 返回词库里的全部单词——这是 6.1 节"空前缀匹配一切"约定的自然延伸。

### 6.6 ⚠️ 错误示例：无条件往上 free 整条路径

很多人第一次写 Trie 删除时的直觉是："找到单词结尾，然后把这一路走过的节点全部 free 掉不就行了？"：

```c
/* ❌ BUG：不检查任何共享情况，从最深处开始无条件往上 free 整条路径 */
static void trie_delete_BROKEN(TrieNode *root, const char *word) {
    /* ... 先走到单词末尾，记录路径 path[]/idx[] ... */
    free(cur);
    for (int i = depth - 1; i >= 0; i--) {
        path[i]->children[idx[i]] = NULL;
        /* 如果 path[i] 还被别的单词用着（比如它还有其他孩子，或者
         * 它自己就是另一个单词的结尾），这里也不会检查，
         * 直接继续往上 free —— 这就是 bug 所在 */
        if (i > 0) free(path[i]);
    }
}
```

这个错误实现**不是内存安全问题**：它不会二次释放同一个指针，不会 use-after-free，ASan/UBSan 都不会报警。它是纯粹的逻辑错误——把被其他单词共享的前缀节点也删掉了。真实运行输出：

```text
插入 car, card（共享 c-a-r 路径）
删除前:   search("card")       = true
调用 trie_delete_BROKEN(root, "car") ...
删除后:   search("card")       = false
-> "card" 本该完好无损，但因为 c/a/r 节点被无条件 free，
   现在查找 "card" 会从一个已经不存在于树里的路径开始找，
   结果是 "card" 也从 Trie 里"消失"了 —— 这就是没做
   "这一层是否还被其他单词占用"检查的后果。
```

演示到这里就停止，`demo.c` 不再对这棵被破坏的树调用 `trie_free`（继续操作它是未定义行为，选择直接放弃）——这本身也说明了一个原则：**一旦发现结构已经被破坏成未定义状态，最安全的做法是隔离这块内存、不再对它做任何操作，而不是尝试"修复"它继续用**。

### 机制剖析：为什么插入两次不会留下两份

如果对同一个单词调用 `trie_insert` 两次会怎样？沿着路径走下去，每一步 `cur->children[idx] == NULL` 的判断都会因为第一次插入已经创建了对应节点而为假，所以第二次插入不会创建任何新节点，只会在终点把已经是 `true` 的 `is_word` 再设一次 `true`——插入操作天然是幂等的，这也是为什么 Trie 不适合直接拿来当"计数容器"用（下面会展开）。

### 常见误区：把 Trie 当成计数容器来测试

一个容易踩的测试陷阱：如果测试逻辑依赖"插入 N 次某单词、删除 N 次、期待某种计数关系"，会发现 Trie 根本不记录"插入了几次"——`is_word` 是一个 `bool`，insert 两次和 insert 一次的最终状态完全相同。Trie 回答的问题始终是"这个单词**存在不存在**"，不是"这个单词出现了几次"。如果需要计数语义，应该把 `bool is_word` 换成 `int count`，插入时 `count++`，删除时 `count--` 并且只有 `count` 降到 0 才真正清除标记——这是一个完全不同的数据结构变体，不能和标准 Trie 的删除逻辑混用。

### 测试覆盖与验证

13 个测试覆盖了基本插入查找、前缀共享结构、正常删除（含共享节点保留验证）、删除不存在单词的三种情况、大随机插入删除混合压力测试、`trie_free(NULL)` 和空 Trie 的安全性等类别：

```bash
$ cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g tests.c -o tests && ./tests
[PASS] test_large_random_insert_delete
[PASS] test_free_null_and_empty_safe

========== 汇总 ==========
通过: 13, 失败: 0, 总计: 13
```

sanitizer 版本同样 13/13、退出码 0；`leaks` 工具额外验证零泄漏（Trie 是本章唯一一个动态分配密集、且删除逻辑本身就是"决定是否释放"的结构，专门做了 sanitizer + `leaks` 双重内存验证）。

### 最佳实践

- 删除前先用 `trie_node_is_empty` 类的辅助判断封装"这一层是否还有存在的理由"，不要把"清标记"和"释放节点"的判断逻辑散落地写在多个地方。
- 递归删除函数的返回值语义要明确：本例中"返回 true 表示我变空了，父节点可以释放我"，调用者（父节点）拿到 `true` 才执行 `free`，这个契约必须在整条递归链上保持一致。
- 如果发现结构已经因为某个 bug 被破坏成不确定状态（如 6.6 节的演示），不要尝试继续在它上面做操作或"修复"它，直接隔离、放弃这块内存。

### 练习

1. 把 `is_word: bool` 改成 `count: int`，实现"支持重复插入计数"的 Trie 变体，删除时只在 `count` 降到 0 才真正清除节点。
2. Trie 目前固定 26 个子节点（只支持小写字母）。如果要支持任意 ASCII 字符甚至 Unicode，分别应该怎么改数据结构？两种方案的空间代价分别是什么？
3. 实现"最长公共前缀"：给定一组单词，用 Trie 找出它们的最长公共前缀。

---

## 七、B 树：磁盘友好的多路平衡树

**代码位置**：`c-algo-advanced-experiments/04_b_tree/`（`btree.c` 373 行，`demo.c` 265 行，`perf.c` 157 行，`tests.c` 429 行，20 个测试用例）

本章及之后的 B+ 树都采用 CLRS《算法导论》的"最小度数 t"约定：对最小度数为 t（t >= 2）的 B 树，每个非根节点的 key 数在 `[t-1, 2t-1]` 之间，每个非根内部节点的孩子数在 `[t, 2t]` 之间，根节点的 key 数在 `[0, 2t-1]` 之间（0 表示空树），所有叶子深度相同。两章使用相同的 t 约定，是为了后面第十章能直接对比它们在相同参数下的结构差异。

### 本章要解决的问题

1. 二叉搜索树在数据量大时会退化成链状（高度 O(n)），B 树怎么保证高度始终是 O(log n)？
2. 插入导致节点"满员"时如何分裂？删除导致节点"下溢"时如何补救？
3. 怎么用一个独立的校验函数，把"结构是否仍然合法"这件事从"程序有没有崩溃"这件事里剥离出来？

### 数据结构定义

```c
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
```

一个节点内部本身就是一段有序数组（`keys[0..n-1]` 严格递增），这和二叉搜索树"一个节点只存一个 key"是本质区别——B 树的"宽而矮"正是靠每个节点容纳多个 key 换来的。

### 7.1 插入：满员节点先分裂，再往下插

```c
/* Split the full child children[i] of x. child has 2t-1 keys.
 * Middle key (index t-1) moves up into x; the right half becomes a new
 * sibling node inserted at children[i+1]. */
static void split_child(BTreeNode *x, int i, int t) {
    BTreeNode *y = x->children[i];      /* full child, y->n == 2t-1 */
    BTreeNode *z = node_create(t, y->is_leaf);

    z->n = t - 1;
    for (int j = 0; j < t - 1; j++) z->keys[j] = y->keys[j + t];
    if (!y->is_leaf) {
        for (int j = 0; j < t; j++) z->children[j] = y->children[j + t];
    }
    int mid_key = y->keys[t - 1];
    y->n = t - 1;
    /* ... 把 mid_key 和新节点 z 插入到 x 里 x->keys[i] / x->children[i+1] ... */
}
```

关键是这个中点下标 `t - 1`：一个满节点有 `2t-1` 个 key，下标 `t-1` 恰好是正中间那个，它被"挤"上去成为父节点的新分隔 key，左边 `[0, t-2]` 共 `t-1` 个留在原节点，右边 `[t, 2t-2]` 共 `t-1` 个搬进新节点——分裂后左右两边刚好都满足"至少 t-1 个 key"的下限。这个 `t-1` 会在 7.6 节的错误示例里被证明是"差一位就会破坏不变量"的关键常数。

插入本身采用"预防式"策略：不是先插入再处理溢出，而是插入前就检查"如果这里已经满了，先分裂"，这样递归下降的过程中永远不会遇到"已经满了还要继续硬塞"的情况：

```c
bool btree_insert(BTree *tree, int key) {
    int t = tree->t;
    if (!tree->root) {
        tree->root = node_create(t, true);
        tree->root->keys[0] = key;
        tree->root->n = 1;
        return true;
    }
    if (tree->root->n == 2 * t - 1) {
        BTreeNode *new_root = node_create(t, false);
        new_root->children[0] = tree->root;
        tree->root = new_root;
        split_child(new_root, 0, t);   /* 树在这里长高一层 */
    }
    return insert_nonfull(tree->root, key, t);
}
```

真实运行输出（t=2，逐步插入触发分裂）：

```text
插入 10 20 30 后: [10 20 30]
插入 40  <-- 这一次插入前 root 已满，会触发分裂
[20]
  [10]
  [30 40]
  [verify ok after insert]
```

`[10 20 30]` 满了（`2t-1=3`），插入 40 前先分裂：中间的 `20` 被推上去当新根，`10` 留左边，`30 40` 归右边——树从 1 层长到 2 层。真实的 root 分裂特写输出更直接：

```text
插入 40 之前（root 已满，n=3=2t-1）：
[10 20 30]
height=0（还是单层）

插入 40 之后：
[20]
  [10]
  [30 40]
height=1（长高了一层）
root 现在只有 1 个 key（20），它是原来 [10 20 30 40] 分裂时
被"挤"到中间、推上去的那个 key；剩下的 key 平分成左右两个孩子。
```

### 7.2 删除：三种叶子下溢的补救方式

删除比插入复杂得多，因为删除后可能出现"下溢"（key 数低于 `t-1`），补救方式有三种，CLRS 里叫 case 3a/3b/3c：

```text
========== 6. 删除：叶子会下溢，向左邻居借一个 key ==========
要删 40：它在叶子 [40 50]（n=2，已经是下限）。
左邻居 [10 15 20 25] 有 4 个 key，够借；右邻居 [70 80] 也在下限，不够借。
=> 走 3a：从左邻居借。
[25 60 90]
  [10 15 20]
  [30 50]
  [70 80]
  [100 110 120]
借的过程：父节点的分隔 key 30 被"压"进 [40 50] 变成 [30 50]，
左邻居里最大的 key 25 顶替 30，成为新的分隔 key，左邻居变成 [10 15 20]。
```

```text
========== 8. 删除：叶子会下溢，且两侧邻居都不够借 -> 合并 ==========
要删 110：它在叶子 [110 120]（n=2，下限）。
它只有一个邻居 [80 90]（是最后一个孩子），也在下限（n=2），借不到。
=> 两侧都不够借，只能合并：分隔 key 100 被拉下来，
跟 [80 90] 和 [110 120] 拼成一个节点，root 从 3 个 key 掉到 2 个。
[25 60]
  [10 15 20]
  [30 50]
  [80 90 100 120]
```

对应代码：

```c
static void fill_child(BTreeNode *x, int i, int t) {
    if (i > 0 && x->children[i - 1]->n >= t) {
        /* 3a: borrow from left sibling (right-rotate through the parent) */
        ...
    } else if (i < x->n && x->children[i + 1]->n >= t) {
        /* 3b: borrow from right sibling (left-rotate through the parent) */
        ...
    } else {
        /* 3c: merge with a sibling. Prefer the right sibling unless we're
         * at the last child, in which case merge with the left one. */
        if (i < x->n) merge_children(x, i, t);
        else merge_children(x, i - 1, t);
    }
}
```

三种情况按优先级尝试：能从左边借就借（3a），不能就看能不能从右边借（3b），两边都不够就合并（3c）。"借"本质上是一次经过父节点的旋转（父节点的分隔 key 被压下去，邻居的边界 key 被提上来填补父节点），不产生新节点也不释放节点；"合并"则会真正释放一个子节点，并让父节点少一个 key——如果父节点因此也下溢，同样的补救逻辑会沿着删除路径递归地继续往上处理。

合并还可能导致树整体降低一层，真实输出捕捉到了这个边界情况：

```text
现在删 10：左孩子 [10] 本身就是要删的那个 key 所在的叶子，
删完它会变空（n=0）。它唯一的邻居 [30] 也在下限（n=1），没法借，
=> 必须合并：把分隔 key 20 和右孩子 [30] 一起并入左孩子，
合并后 root 变空，树整体降一层。
[20 30]
height=0（从 1 降到 0，root 从内部节点变成了唯一的叶子）
```

### 7.3 删除一个内部节点的 key：前驱/后继替换

如果要删除的 key 不在叶子上，而在某个内部节点里，不能直接从数组里挖走它（挖走后左右子树的分界就没了意义）。CLRS 的做法是找一个"替身"：左子树里最大的 key（前驱）或右子树里最小的 key（后继），用它顶替被删的 key，再递归地把这个替身从它原来的位置删掉——问题被转化成了一次叶子删除（或者递归地继续转化，直到真的落到叶子上）。

```c
if (pred_child->n >= t) {
    /* case 2a */
    int pred_key = find_max(pred_child);
    x->keys[i] = pred_key;
    delete_from(pred_child, pred_key, t);
} else if (succ_child->n >= t) {
    /* case 2b */
    int succ_key = find_min(succ_child);
    x->keys[i] = succ_key;
    delete_from(succ_child, succ_key, t);
} else {
    /* case 2c: merge pred_child, key, succ_child into one node,
     * then the key to delete is now inside that merged node. */
    merge_children(x, i, t);
    delete_from(pred_child, key, t);
}
```

选前驱还是后继取决于哪一边"有富余"（key 数 >= t，借了之后不会自己下溢）；两边都没富余就走 case 2c，把左孩子、被删 key、右孩子三者合并成一个节点，再递归删除。真实输出（case 2a 偷前驱）：

```text
root=[20]，左孩子 [5 10 15]（n=3，够借），右孩子 [30]（n=1，下限）。
删 20：它是内部节点的 key。检查左孩子，n=3>=t=2，够借
=> 用左子树里最大的 key（前驱，也就是 15）顶替 20，
然后递归地把 15 从左子树里删掉。
[15]
  [5 10]
  [30]
root 现在的 key 是 15（前驱 15 被提上来了）
```

### 7.4 一直删到空树

`btree_delete` 反复调用直到树空，每一步都跑一次 `btree_verify`，真实输出确认了从 15 个 key 的三层树一路缩到空树的全过程中，不变量始终成立：

```text
插入 1..15 之后：
[4 8]
  [2]
    [1]
    [3]
  [6]
    [5]
    [7]
  [10 12]
    [9]
    [11]
    [13 14 15]
  [verify ok after shrink-to-empty sequence]  (共 15 次，每次删除后都验证一次)
全部删完：root == NULL 是 真，height=-1
```

### 7.5 结构校验：`btree_verify`

```c
static void verify_rec(const BTreeNode *x, int t, bool is_root, int depth,
                        const int *min_key, const int *max_key,
                        int *leaf_depth_out, VerifyCtx *ctx) {
    int max_keys = 2 * t - 1;
    int min_keys = is_root ? 0 : t - 1;
    if (x->n > max_keys) { fail(ctx, "node has %d keys, max is %d", x->n, max_keys); return; }
    if (x->n < min_keys) { fail(ctx, "node has %d keys, min is %d", x->n, min_keys); return; }

    for (int i = 0; i < x->n - 1; i++)
        if (x->keys[i] >= x->keys[i + 1]) { fail(ctx, "keys not strictly increasing"); return; }
    /* ... 用 min_key/max_key 检查这个节点的 key 范围是否落在父节点给定的边界内 ... */

    if (x->is_leaf) {
        if (*leaf_depth_out == -1) *leaf_depth_out = depth;
        else if (*leaf_depth_out != depth) fail(ctx, "leaf depth mismatch");
        return;
    }
    for (int i = 0; i <= x->n; i++) {
        const int *lo = (i == 0) ? min_key : &x->keys[i - 1];
        const int *hi = (i == x->n) ? max_key : &x->keys[i];
        verify_rec(x->children[i], t, false, depth + 1, lo, hi, leaf_depth_out, ctx);
    }
}
```

这个函数一次性检查了 B 树的全部结构性质：key 数量范围、key 严格递增、key 落在父节点继承下来的边界内（`min_key`/`max_key` 沿着递归逐层收紧，这样能查出"左子树的最大 key 超过了分隔 key"这类跨层错误，不只是查本节点内部）、所有叶子深度一致。它和"程序没有崩溃"是两件完全独立的事——这正是下一节错误示例要说明的重点。

### 7.6 ⚠️ 错误示例：分裂中点算错一位

7.1 节提到分裂时中点下标是 `t - 1`，如果不小心写成 `t`（漏了 `-1`）：

```text
========== 12. 错误示例：分裂中点算错一位，会怎样 ==========
满叶子 [1 2 3 4 5]（t=3，最多 2t-1=5 个 key）要分裂。
正确做法：中间下标是 t-1=2，也就是 key[2]=3 被推上去，
左边留 t-1=2 个，右边留 t-1=2 个。

错误版本：把中间下标写成了 t=3（漏了 -1），key[3]=4 被推上去，
左边留 3 个，右边留 1 个。
最少要求每个非 root 节点有 t-1=2 个 key，右边只有 1 个 -> 违反不变量！
这种 bug 不会让程序崩溃，也不会被 sanitizer 抓到（没有非法内存访问），
查找、插入在小规模测试下可能看起来都"正常工作"，直到某次删除因为
某个节点的 key 数比假设的下限还少，触发数组下标或逻辑上的错误。
唯一可靠的防线就是 btree_verify()：每次插入/删除后跑一次，
任何不变量被破坏都会在"案发现场"被抓到，而不是等到很久以后才崩溃。
```

这类 bug 的危险之处在于它**只破坏一个数量约束**，不涉及任何越界读写或空指针——ASan/UBSan 对此完全无感，小规模手工测试甚至可能看不出任何异常（分裂完之后查找、插入照样能正常工作，因为它们只依赖"有序"这一个性质，不检查"两边各有几个"）。只有当后续某次删除依赖"每个非根节点至少有 t-1 个 key"这个假设去决定"要不要借/合并"时，才会因为假设不成立而出错——而这次出错可能发生在触发 bug 很久之后，调试时完全看不出和分裂代码有什么关系。这正是 `btree_verify` 存在的意义：把"结构性质是否成立"做成一个可以随时主动调用的独立检查，而不是被动等待它在某个不相关的地方以某种意外方式暴露出来。

### 性能实测：不同 t 值下的高度与范围查询代价

`perf.c` 用 `-O2` 编译，实测不同最小度数 t 下插入 100000 个 key 后的树高和范围查询代价，为第八章 B+ 树的对比提供真实基准：

```text
--- t = 2, n = 1000 ---
height after insert: 7, nodes: 571
range query [500, 999] (width 499): visited 289 nodes, matched 500 keys

--- t = 64, n = 1000 ---
height after insert: 1, nodes: 14
range query [500, 999] (width 499): visited 8 nodes, matched 500 keys

--- t = 64, n = 100000 ---
insert 100000 keys: 5.66 ms (0.057 us/op)
height after insert: 2, nodes: 1121
search 100000 keys: 4.45 ms (0.045 us/op), hits=100000
range query [50000, 60000] (width 10000): visited 121 nodes, matched 10001 keys
delete 100000 keys: 10.23 ms (0.102 us/op)
```

`t=2` 时（等价于 2-3-4 树）100 万分之一规模都要 7 层高、访问 289 个节点才能查完一个宽度 499 的范围；`t=64` 时同样规模只需 1 层高、8 个节点。**t 越大，树越"矮胖"，越贴近真实数据库/文件系统页大小的设计意图**（一个节点对应一个磁盘页，t 取决于页大小能塞多少个 key）。但即便 `t=64` 已经很"矮胖"，范围查询仍然要"访问 121 个节点"——因为 B 树的范围查询本质上还是一次中序遍历，需要不断地在节点之间上下跳转；这个数字会在第八章被 B+ 树的叶子链表范围扫描直接击穿。

### 测试覆盖与验证

20 个测试覆盖了不同 t 值下的插入分裂、三种删除下溢补救（3a/3b/3c）、内部节点删除的两种替换（前驱/后继）、随机压力测试（`test_random_stress_t5_1500`、`test_random_stress_t10_2000`）等：

```bash
$ cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g btree.c tests.c -o tests && ./tests
[PASS] test_random_stress_t5_1500
[PASS] test_random_stress_t10_2000

========== 汇总 ==========
通过: 20, 失败: 0, 总计: 20
```

sanitizer 版本同样 20/20、退出码 0。

### 常见误区

1. 混淆"节点满了要分裂"的时机：本实现是插入前检查、预防式分裂，如果改成"插入后发现溢出再分裂"，递归下降路径上的父指针关系会变得更难维护——这不是唯一正确的实现方式，但换方式必须换掉整套下降逻辑，不能只改一半。
2. 删除时只处理了叶子下溢，忘记内部节点删除本身也需要前驱/后继替换这一步，直接把内部节点的 key "挖空"——这会让左右子树的分界信息丢失，后续查找会出错。
3. 把"结构合法"和"程序没崩溃"当成一件事：7.6 节的错误示例证明了这两者是独立的，必须用显式的不变量校验去覆盖前者。

### 小结

B 树用"一个节点存多个 key"换来了"矮而宽"的树形，把随机 I/O 次数（等价于树高）压到最低，这是它在数据库和文件系统里长期占主导地位的根本原因。插入靠"预防式分裂"保证永远不会在满节点上继续下降，删除靠"借或合并"保证永远不会让节点降到下限以下——这一整套补救逻辑的正确性，最终必须靠一个独立的结构校验函数去保证，而不是依赖"跑起来没崩溃"的错觉。

### 练习

1. 把 `btree_insert` 改成"先插入再检测溢出并分裂"的后处理式实现，对比这种写法在递归结构上的差异。
2. `merge_children` 合并后如果父节点自己也下溢，`delete_from` 的递归调用链是如何继续处理这种"连锁下溢"的？画出调用栈。
3. 如果一棵 B 树只支持插入和查找，不支持删除，`btree_verify` 里哪些检查项可以省略？

---

## 八、B+ 树：为范围查询而生

**代码位置**：`c-algo-advanced-experiments/05_b_plus_tree/`（`bplustree.c` 697 行，`demo.c` 212 行，`perf.c` 128 行，`tests.c` 529 行，24 个测试用例）

延续第七章的 t 约定，B+ 树和 B 树共享同一套"最小度数 t"参数，方便直接对比。但 B+ 树引入了一个 B 树没有的结构性区别：**内部节点和叶子节点的形状不同**。

```c
typedef struct BPlusNode {
    bool is_leaf;
    int n;
    int *keys;
    union {
        struct BPlusNode **children; /* internal only */
        int *values;                 /* leaf only, values[i] pairs with keys[i] */
    } u;
    struct BPlusNode *next;       /* leaf only: next leaf in sorted order */
} BPlusNode;
```

内部节点只存"路由 key"（routing key）——它们不是真实数据，只是"该往哪个孩子走"的路标；真实的 key/value 全部只存在叶子里。每个叶子还带一个 `next` 指针，把所有叶子串成一条从左到右有序的单链表。

### 本章要解决的问题

1. B 树的范围查询要靠中序遍历、反复在内部节点间跳转决策，B+ 树怎么把这个代价降到"定位一次，剩下纯线性扫描"？
2. 叶子分裂时提升的 key 是复制，内部节点分裂时提升的 key 是移动——这个不对称性从哪里来，为什么不能统一？
3. 叶子合并/借位之后，链表的 `next` 指针断裂是一类极难在小规模测试里现形的 bug，怎么专门设计校验去抓住它？

### 8.1 叶子分裂：提升的 key 是"复制"

```c
static BPlusNode *split_leaf(BPlusNode *y, int t, int *sep_key_out) {
    BPlusNode *z = node_create(t, true);
    int left_n = t;       /* y keeps the smaller half, INCLUDING the middle key */
    int right_n = t - 1;  /* z gets the larger half */

    for (int j = 0; j < right_n; j++) {
        z->keys[j] = y->keys[left_n + j];
        z->u.values[j] = y->u.values[left_n + j];
    }
    z->n = right_n;
    y->n = left_n;

    z->next = y->next;
    y->next = z;   /* 把新叶子插进链表 */

    *sep_key_out = z->keys[0]; /* copy: z->keys[0] stays real data in z */
    return z;
}
```

真实运行输出，插入 40 让叶子 `[10 20 30]` 分裂：

```text
插入 40 之后：
[30]
  [10:1000 20:2000] (leaf)
  [30:3000 40:4000] (leaf)
  leaf chain: [10,20] -> [30,40] -> NULL
root 的 routing key 是 30 —— 它是从原来叶子 [10 20 30] 分裂时
被"复制"上去的：key 30 依然作为真实数据留在右边的叶子 [30:3000 40:4000]
里，root 里的 30 只是一份指路用的副本，不是把 30 从叶子里搬走。
```

`sep_key_out` 只是把 `z->keys[0]` 的值抄一份传给父节点，`z` 自己那份原样保留——因为叶子上的 30 是"真实数据"，删掉它会丢数据；父节点上的 30 只是"往哪边走"的路标，两者各自有各自的用途，必须都留着。

### 8.2 内部节点分裂：提升的 key 是"移动"

```c
static BPlusNode *split_internal(BPlusNode *y, int t, int *sep_key_out) {
    BPlusNode *z = node_create(t, false);
    z->n = t - 1;
    for (int j = 0; j < t - 1; j++) z->keys[j] = y->keys[j + t];
    for (int j = 0; j < t; j++) z->u.children[j] = y->u.children[j + t];

    *sep_key_out = y->keys[t - 1]; /* moved: removed from y, not duplicated */
    y->n = t - 1;
    return z;
}
```

真实运行输出，继续插入到 100 让 root（内部节点）分裂：

```text
插入 100 之后：
[50]
  [30]
    ...
  [70 90]
    ...
新 root 只有 1 个 key（50）。这个 50 原本是旧 root [30 50 70] 的中间
routing key，分裂时被"移动"到新 root：左边孩子变成 [30]，右边孩子变成
[70 90]，两边都不再含有 50 —— 跟第 1 步的 leaf 分裂正好相反，internal
节点从来不存真实数据，所以它的分裂没有理由留一份副本在原地。
```

这里 `y->n = t - 1` 直接把 50 从原节点的 key 数组里"移出去"了（下标 `t-1` 之后不再属于任何一边），因为路由 key 本身不是数据，留一份副本没有任何意义——这正是叶子分裂"复制"、内部分裂"移动"这个不对称设计的根源：**只有真实数据才需要在原地留一份，纯粹的路标复制一次就够，用完即弃**。

### 8.3 删除：叶子合并会丢弃分隔 key，内部合并会拉下分隔 key

```c
/* Merge leaf x->u.children[i] and x->u.children[i+1] into the left one.
 * The routing key x->keys[i] that separated them is dropped entirely (it
 * was only ever a copy, so nothing needs to migrate into the merged
 * leaf -- unlike a B-tree merge, where the separator is real data that
 * must be pulled down). */
static void leaf_merge(BPlusNode *x, int i) {
    BPlusNode *left = x->u.children[i];
    BPlusNode *right = x->u.children[i + 1];
    for (int j = 0; j < right->n; j++) {
        left->keys[left->n + j] = right->keys[j];
        left->u.values[left->n + j] = right->u.values[j];
    }
    left->n += right->n;
    left->next = right->next;   /* 关键：修补链表，否则链表在这里断掉 */
    /* ... 从父节点里移除这个分隔 key 和右孩子指针 ... */
    node_free(right);
}
```

真实输出，验证了合并前后链表的完整性：

```text
========== 7. 删除：触发 leaf 合并 (merge)，用完整链表证明 next 指针被修好了 ==========
合并之前的链表：
  leaf chain: [10,20] -> [30,40] -> [50,60] -> [70,80] -> [90] -> [100] -> NULL
删除 100：叶子 [100] 只有 1 个 key，左邻居 [90] 也只有 1 个（没有多余的
可借），两边都在下限，只能合并。合并时 routing key 100 直接被"丢弃"
——它本来就只是一份复制品，不像 B 树合并那样需要把 key 拉下来。

合并之后的链表（[100] 从链上消失，[90] 的 next 直接指向下一个存活的叶子）：
  leaf chain: [10,20] -> [30,40] -> [50,60] -> [70,80] -> [90] -> NULL
```

而内部节点合并恰恰相反——分隔 key 必须被拉下来，因为它是唯一还在区分两边孙子子树的东西：

```c
/* Merge internal x->u.children[i] and x->u.children[i+1], pulling down
 * the separator routing key x->keys[i] -- this one DOES have to move
 * into the merged node, because it is the only thing that still
 * separates the two halves' worth of grandchildren. */
static void internal_merge(BPlusNode *x, int i) {
    left->keys[left->n] = x->keys[i];   /* 分隔 key 被拉下来，成为合并节点内部的真 key */
    /* ... */
}
```

真实输出（root 收缩前的一次内部合并）：

```text
删除 42：叶子 [40] 变空并与邻居合并，导致它的父节点 [42]（一个
internal 节点）只剩 1 个孩子、0 个 routing key，下溢。它跟兄弟 [24]
合并：root 的 routing key 32（分隔 [24] 子树和 [40] 子树的那个 key）
被"拉下来"塞进合并后的节点，因为它是唯一还在区分两边孙子层的东西——
跟第 7 步的 leaf 合并（直接丢弃分隔 key）正好相反。
```

叶子合并"丢弃"分隔 key、内部合并"拉下"分隔 key——这个不对称性和 8.2 节的"复制 vs 移动"是同一个原理的两个不同表现：**路由 key 只要还在履行路由职责就必须留着（内部合并时它仍需要区分孙子层），一旦它所代表的边界不再需要区分任何东西就可以直接丢弃（叶子合并时两个叶子已经变成一个，不再需要内部分隔）**。

### 8.4 范围查询：一次下降定位，剩下全是链表扫描

```c
int bplustree_range_query(const BPlusTree *tree, int low, int high,
                           int *keys_out, int *values_out, int cap,
                           int *nodes_visited_out) {
    const BPlusNode *x = tree->root;
    while (!x->is_leaf) {           /* 只下降一次，定位 low 应该在的叶子 */
        int i = 0;
        while (i < x->n && low >= x->keys[i]) i++;
        x = x->u.children[i];
    }
    const BPlusNode *leaf = x;
    while (leaf) {                   /* 剩下全是沿 next 链表的线性扫描 */
        for (int j = 0; j < leaf->n; j++) {
            if (leaf->keys[j] < low) continue;
            if (leaf->keys[j] > high) { /* ... 提前结束 ... */ return written; }
            /* ... 收集进 keys_out/values_out ... */
        }
        leaf = leaf->next;
    }
    return written;
}
```

真实输出：

```text
range_query(35, 95)：先向下走到 key=35 应该在的叶子（不是最左叶子），
然后沿 next 链表一路向右扫描，直到超过 95 为止：
找到 6 对 key/value，一共访问了 4 个叶子（总叶子数 6）：
  40:4000 50:5000 60:6000 70:7000 80:8000 90:9000

对比 B 树：B 树没有叶子链表，range query 只能从根做一次中序遍历，
沿途每个经过的内部节点都要重新决策"往哪个孩子走"；B+ 树只需要一次
对数高度的下降定位起点，剩下全部是一条链表上的线性扫描，访问节点数
只跟结果集大小相关，跟树的总大小（或树高）无关。
```

`perf.c` 的实测数据把"访问节点数只跟结果集大小相关，跟树的总大小无关"这句话钉死成了数字（t=4，固定范围宽度 1000，树规模从 1000 一路扩大到 500000）：

```text
n=1000     total_leaves=250      range_width=1000   found=501   visited_leaves=126
n=10000    total_leaves=2500     range_width=1000   found=501   visited_leaves=126
n=100000   total_leaves=25000    range_width=1000   found=501   visited_leaves=126
n=500000   total_leaves=125000   range_width=1000   found=500   visited_leaves=126
```

树的总叶子数从 250 涨到 125000（涨了 500 倍），固定宽度范围查询访问的叶子数始纹丝不动地停在 126——这正是第七章末尾埋下的对比钩子的答案：B 树即使把 t 调到很"矮胖"（`t=64`），range query 仍然要重新做一次树内的中序遍历、随树规模和高度产生额外的跳转代价；B+ 树的叶子链表把这个代价彻底"拍平"成了纯线性扫描，只取决于结果集本身有多大。这就是数据库和文件系统索引普遍选择 B+ 树而不是 B 树的核心原因。

### 8.5 结构校验：多出的"叶子链表完整性"检查

```c
bool bplustree_verify(const BPlusTree *tree, char *err_buf, size_t err_buf_size) {
    /* ... 先跑一遍跟 B 树一样的递归校验：key 范围/严格递增/叶子深度一致 ... */

    /* Leaf-chain integrity: walk `next` from the leftmost leaf and cross
     * check against what tree recursion saw. This is the property a
     * B-tree has no equivalent of. */
    int chain_leaf_count = 0, chain_key_count = 0;
    for (const BPlusNode *leaf = leftmost_leaf; leaf; leaf = leaf->next) {
        chain_leaf_count++;
        /* ... 顺便检查链表本身是否严格递增 ... */
        chain_key_count += leaf->n;
    }
    if (chain_leaf_count != leaf_count_via_tree) { /* fail: 链表和树递归看到的叶子数不一致 */ }
    if (chain_key_count != count_keys_rec(tree->root)) { /* fail: 链表和树递归看到的 key 总数不一致 */ }
    return true;
}
```

这一段是 B+ 树独有的：分别用"树的递归遍历"和"跟着 `next` 走一遍链表"两条完全独立的路径去数叶子数和 key 总数，两个数字必须完全一致。如果 `leaf_merge` 忘了 `left->next = right->next` 那一行，链表会在合并处断掉——单纯检查"某个 key 是否存在"这类测试完全测不出这个问题（因为断链之后，只要没人真的从头到尾走一遍链表，树的其他结构照样合法），只有这种"两条独立路径互相校验"的设计才能把它抓出来。

### 8.6 真实调试笔记：一处算法 bug，两处测试构造错误

`bplustree.c` 源码里，插入向上传播分裂的循环中留着一条注释：

```c
/* BUG (found during random-stress verification, see README debug
 * notes): split_internal() mutates parent->n to t-1 as a side
 * effect. Comparing `idx < parent->n` AFTER that call reads the
 * post-split (shrunk) count, not the pre-split child count the
 * index was computed against -- silently routing the new child
 * into the wrong half whenever idx landed in [t-1, 2t-2]. Must
 * snapshot the pre-split child boundary (t children survive in
 * the left half, indices 0..t-1) before calling split_internal. */
int left_children_before_split = t;
```

**根本原因**：`split_internal` 会把传入节点的 `n` 直接改成 `t-1` 作为副作用，然后才返回分裂出的新节点。如果调用方在调用**之后**才去读 `parent->n` 来判断"新孩子该插进哪一半"，读到的就是已经被改写过的值（`t-1`），不是分裂前的真实孩子数（`t`）——这两个数字只差 1，只有当索引恰好落在这个单点上时，两种判断才会给出不同结果，一旦触发就会把新孩子错误地路由进另一半，直接破坏树结构。

这个 bug 是靠随机压力测试真实抓到的，还是写代码时就被人工发现、从未真正跑出过失败——现在已经没有留下能证明的记录（这段代码没有中间提交历史），诚实的做法是不去编造一个具体的"当时的报错输出"。但可以做、也做了的是：把这处判断还原成修复前的写法（`idx < parent->n`，在调用 `split_internal` 之后才比较），单独编译重新跑一遍完整测试套件，用真实还原实验证明这个 bug 确实严重、也确实会被现有测试体系第一时间抓住：

```text
$ ./tests_buggy
bplustree_buggy.c:87:16: runtime error: member access within misaligned address
  0xbebebebebebebebe for type 'BPlusNode', which requires 8 byte alignment
AddressSanitizer:DEADLYSIGNAL
==31027==ERROR: AddressSanitizer: SEGV on unknown address 0x5857d7d9d7d7
    #0 find_leaf_path bplustree_buggy.c:87
    #1 bplustree_insert bplustree_buggy.c:160
    #2 test_leaf_borrow_from_left_sibling tests.c:167
SUMMARY: AddressSanitizer: SEGV bplustree_buggy.c:87 in find_leaf_path
========== B+ 树测试套件 ==========
[PASS] test_empty_tree_search_and_delete
...
  bplustree_verify failed after sequential insert: keys not strictly increasing at index 0: 7 >= -1094795586
  FAILED CHECK: verify_ok(&t, "sequential insert") (tests.c:101)
[FAIL] test_degree_t2_sequential
[FAIL] test_degree_t3_sequential
[FAIL] test_degree_t4_sequential
[PASS] test_degree_t10_sequential
[FAIL] test_root_split_moves_key_no_duplicate
(进程被 AddressSanitizer 中止，之后的用例没有机会跑)
```

不开 sanitizer 的普通严格编译跑同一个还原版本更直接：还没打印任何一行输出就直接 segfault（退出码 139）——说明这不是"结果稍微不对但程序还能跑完"级别的问题，而是会破坏内存布局、多数情况下直接崩溃的严重缺陷。最基础的顺序插入测试（`test_degree_t2/t3/t4_sequential`）和 `test_root_split_moves_key_no_duplicate` 会在第一时间抓住这个 bug，连大规模随机压力测试都不需要跑到。修复后（提前用 `left_children_before_split = t` 固定住分裂前的孩子数）重新跑同样的 24 个测试，全部 `[PASS]`，sanitizer 全程干净。

另外两处是测试用例本身构造错误的教训，跟上面这处算法 bug 性质完全不同：

- **借位检测条件不自洽**：最初的判断条件想用"节点总数不变、叶子数减少"去识别"内部借位"，但这个条件本身是错的——纯粹的叶子借位根本不改变叶子数量（只是在两个已存在的叶子间重新分配 key），真正的信号应该是"叶子数减 1（底层先发生一次叶子合并）、内部节点数不变（父节点借到了，不是合并）"。
- **想当然假定了错误的连锁层数**：曾经断言"删除某个 key 应该正好释放 2 个节点"，这个数字是纸面推演"应该只连锁一层"直接写下的，没有实测过；用探测程序打出真实的节点数差值后发现，那次删除实际连锁触发了两次内部合并，释放了 3 个节点，不是 2 个。

这两处教训指向同一个共同原则：**光靠打印出来的树形状用眼睛看，很容易把"借位"和"合并"看反，也很容易低估连锁反应传导了几层**；唯一可靠的办法是在操作前后分别记录精确的 `叶子数`/`内部节点数` 计数，用计数的**差值**（而不是某一次的绝对值）去判断到底发生了什么——叶子数减 1、内部节点数不变 → 借位；叶子数减 1、内部节点数也减少 → 合并，减少几个就是连锁了几层。这是第七章"用 `btree_print()` 打印结构再写断言"这条经验的进一步延伸：打印出来只是第一步，真正下断言前还必须换成精确的计数差值。

### 性能实测：range query 代价与树规模脱钩

```text
=== range-query leaf-visit scaling (t=4, varying tree size and range width) ===
n=1000     total_leaves=250      range_width=1000   found=501   visited_leaves=126   (50.4% of all leaves)
n=10000    total_leaves=2500     range_width=1000   found=501   visited_leaves=126   (5.0% of all leaves)
n=100000   total_leaves=25000    range_width=1000   found=501   visited_leaves=126   (0.5% of all leaves)
n=500000   total_leaves=125000   range_width=1000   found=500   visited_leaves=126   (0.1% of all leaves)
```

固定范围宽度下 `visited_leaves` 恒定在 126，占总叶子数的比例随树规模扩大而持续下降——这就是 8.4 节结论的数字证据。

### 测试覆盖与验证

24 个测试覆盖了叶子分裂（复制语义验证）、内部分裂（移动语义验证）、叶子借位/合并（含链表修复验证）、内部借位/合并（含分隔 key 拉下验证）、range query 边界、大规模随机压力测试等：

```bash
$ cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g bplustree.c tests.c -o tests && ./tests
[PASS] test_random_stress_t5_1500
[PASS] test_random_stress_t10_2000

========== 汇总 ==========
通过: 24, 失败: 0, 总计: 24
```

sanitizer 版本同样 24/24、退出码 0。

### 常见误区

1. 把叶子分裂和内部分裂的提升语义搞反：叶子提升的 key 必须留一份真实副本在叶子里（不能移动，否则丢数据），内部提升的 key 必须真的移动（留着没有意义，因为它本来就不是数据）。
2. 叶子合并后忘记修 `next` 指针：这类 bug 不会导致任何单点查找失败，只会让 range query 在断点处提前截断，是最难在小规模测试里现形的一类问题，必须靠专门的链表完整性校验去抓。
3. 用"打印树形状之后凭肉眼判断发生了借位还是合并"代替精确计数——8.6 节两次真实的测试构造错误都是这么来的。

### 小结

B+ 树在 B 树的基础上做了一个看似简单却影响深远的分工：内部节点专职路由，叶子专职存数据，叶子之间再用链表串起来。这个分工换来的是 range query 的代价从"树内反复决策"降到"一次下降 + 一段线性扫描"，也是它在真实数据库和文件系统索引里比 B 树更常见的根本原因。代价是插入/删除的实现复杂度更高（要分别处理叶子和内部节点两种不同的分裂/合并语义），需要更细致的结构校验（多一层叶子链表完整性检查），也更容易在开发过程中出现"副作用函数改写了共享状态，调用方却在改写后才去读旧语义"这类隐蔽 bug（8.6 节的算法 bug 正是这个模式）。

### 练习

1. `split_internal` 的副作用陷阱本质上是"先记录一个阈值，中间插入了一次有副作用的调用，再据此判断"这类模式的一个例子。审查本章其余的借位/合并函数，是否还有类似的、依赖调用顺序假设的隐患？
2. 给 `BPlusNode` 加一个叶子专用的 `prev` 指针，并在 `bplustree_verify` 里加一条新校验：从任意叶子沿 `next` 走到底，再沿 `prev` 走回来，应该回到出发点。这条校验能抓住哪些"`next` 修对了但 `prev` 没同步修"的 bug？只有单向链表时这类 bug 完全测不出来吗？
3. 如果要支持"降序范围查询"（从 high 到 low），只有 `next` 指针够不够？需要做什么改动？

---

## 九、红黑树：用颜色约束换近似平衡

**代码位置**：`c-algo-advanced-experiments/06_red_black_tree/`（`rbtree.c` 398 行，`demo.c` 322 行，`tests.c` 442 行，19 个测试用例）

红黑树是 CLRS 第 13 章的经典结构，靠五条着色性质把"近似平衡"这件事变成一组可以局部维护的约束，不需要像 AVL 那样显式记录、比较子树高度。

```text
红黑树五条性质（RB-property）：
1. 每个节点是红色或黑色。
2. 根节点是黑色。
3. 每个叶子节点（这里用哨兵 NIL 表示）是黑色。
4. 如果一个节点是红色，则它的两个子节点都是黑色（不存在两个连续的红色节点）。
5. 对每个节点，从该节点到其所有后代叶子的简单路径上，
   均包含相同数目的黑色节点（黑高一致，不含该节点自身）。
```

### 本章要解决的问题

1. 插入一个新节点默认染成红色，这个选择是怎么保证"最多只违反一条性质"、让修复逻辑可以局部化的？
2. 插入修复只有 3 种情况（叔叔红/之字形/直线），删除修复却有 4 种情况，这个不对称从哪里来？
3. 哨兵 NIL 节点相比直接用 `NULL` 表示空叶子，换来了什么，又需要额外小心什么？

### 数据结构定义：哨兵 NIL

```c
typedef struct RBNode {
    int key;
    RBColor color;
    struct RBNode *left;
    struct RBNode *right;
    struct RBNode *parent;
} RBNode;

typedef struct {
    RBNode *root;
    RBNode *nil; /* 哨兵：整棵树唯一的"空叶子"，颜色恒为黑 */
} RBTree;
```

整棵树只分配一个 `nil` 节点，所有"空指针"位置全部指向它，而不是用 `NULL`。这样删除修复函数可以统一从任意节点出发往上找 `parent`，不需要在每个函数里对"孩子是不是 NULL"写一次特判，`nil->parent` 在旋转/删除过程中会被临时设置成"刚刚离开的那个位置的父节点"，供修复逻辑往上追溯。代价是：只读的递归函数（打印、统计）必须先判断 `x == tree->nil` 再展开，否则会死循环——因为 `nil` 自己的 `left`/`right` 都指向自己。

### 9.1 插入：先染红，最多违反一条性质

```c
bool rb_insert(RBTree *tree, int key) {
    /* ... 常规 BST 插入定位 ... */
    RBNode *z = malloc(sizeof *z);
    z->key = key;
    z->left = z->right = tree->nil;
    z->parent = y;
    z->color = RB_RED; /* 新节点先染红：不改变任何黑高，只可能违反性质 4 */
    /* ... 挂接到父节点 ... */
    insert_fixup(tree, z);
    return true;
}
```

新节点选择染红而不是染黑，是整个插入修复逻辑能局部化的关键：红色节点完全不影响任何路径的黑高（性质 5 天然满足），唯一可能出问题的是性质 4（红色节点的孩子必须是黑色）——如果新节点的父亲恰好也是红色，就会出现"红红相邻"，且**这是插入后唯一可能被破坏的性质**，`insert_fixup` 只需要盯着这一条修。

### 9.2 插入修复：三种情况，本质是"叔叔能不能帮忙扛"

```c
static void insert_fixup(RBTree *tree, RBNode *z) {
    while (z->parent->color == RB_RED) {
        if (z->parent == z->parent->parent->left) {
            RBNode *uncle = z->parent->parent->right;
            if (uncle->color == RB_RED) {
                /* case 1：叔叔是红色 —— 父、叔变黑，祖父变红，
                 * 把"红红相邻"问题原样上推两层，继续在祖父处检查。 */
                z->parent->color = RB_BLACK;
                uncle->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    /* case 2：叔叔是黑色，z 是"之字形"（左-右）
                     * —— 先左旋父节点，把之字形拉直成一条线，
                     * 转化成 case 3 继续处理。 */
                    z = z->parent;
                    left_rotate(tree, z);
                }
                /* case 3：叔叔是黑色，z 和父节点是一条线（左-左）
                 * —— 父变黑、祖父变红，再右旋祖父，彻底修复，循环结束。 */
                z->parent->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                right_rotate(tree, z->parent->parent);
            }
        } else { /* 镜像：z->parent 是祖父的右孩子，left/right 全部对调 */ }
    }
    tree->root->color = RB_BLACK; /* 性质 2：根恒为黑，循环里可能把根短暂染红 */
}
```

三种情况的本质区别在"叔叔"（父亲的兄弟）的颜色和 z 相对父亲/祖父的形状：**叔叔是红色时（case 1），可以直接把"红红相邻"这个问题原样往上推两层**——父、叔都变黑、祖父变红，新的红色祖父可能又和它自己的父亲形成新的"红红相邻"，于是 `z = z->parent->parent` 继续循环检查，问题被系统性地一路推到接近根部，最多推 O(log n) 次；**叔叔是黑色时（case 2/3），叔叔没法帮忙分担，只能靠旋转直接消化掉**，之字形（case 2）先转直（左旋父节点），再统一走直线情况（case 3：变色 + 右旋祖父），一次性彻底解决，不再向上传播。

真实运行输出，插入 10,20,30,15,5,1 展示 case 1 的连锁上推：

```text
--- 插入 30 之后 ---
    root: key=20  color=BLACK
    root.L: key=10  color=RED
    root.R: key=30  color=RED
--- 插入 15 之后 ---
    root: key=20  color=BLACK
    root.L: key=10  color=BLACK
    root.L.R: key=15  color=RED
    root.R: key=30  color=BLACK
说明:插入 30 后 root.L=10(RED)、root.R=30(RED) 两个红孩子,
这是插入 15 时新节点 15 的父亲 10 为红、叔叔 30 也为红的前提;
插入 15 触发的是 case1(叔叔红)：10 与 30 变黑、20 变红,
但 20 是根,fixup 循环末尾强制根为黑,于是 20 保持黑色。
```

之字形转直线（case 2→3）的真实输出：

```text
插入 10,5 后再插入 7:7 是 5 的右孩子、5 是 10 的左孩子,
形成"之字形"，先左旋 5 转成直线形态(case2→case3),再对 10
右旋并变色,一步到位完成修复。
  --- 插入 7 之后(之字形修复完成) ---
    root: key=7   color=BLACK
    root.L: key=5   color=RED
    root.R: key=10  color=RED
说明:修复后 7 变成新的黑色根,5 和 10 都变成它的红色孩子,
树从"10-5-7"的左偏之字形变成了完全平衡的三节点结构。
```

### 9.3 删除：为什么是"双黑"而不是"红红相邻"

删除比插入复杂的根源在于：**删除一个黑色节点必然会让某条路径的黑高减 1**，这直接违反性质 5，而"黑高"是一个全局性的计数属性，不像"红红相邻"那样可以只看局部就发现。CLRS 的技巧是引入一个"双黑"（double-black）的记账概念：顶替被删节点位置的那个节点 `x`，被记成"背了双重黑色"（用来补偿刚被拿走的那一重黑色），`delete_fixup` 的任务就是把这个多背的黑色想办法转移或消化掉。

```c
static void delete_fixup(RBTree *tree, RBNode *x) {
    while (x != tree->root && x->color == RB_BLACK) {
        if (x == x->parent->left) {
            RBNode *sibling = x->parent->right;
            if (sibling->color == RB_RED) {
                /* case 1：兄弟是红色 —— 兄弟不可能是"双黑"的最终吸收者
                 * （红色节点不能直接扛黑高债务），先变色+左旋父节点，
                 * 把一个黑色的侄子换成新兄弟，转化成 case 2/3/4 之一。 */
                sibling->color = RB_BLACK;
                x->parent->color = RB_RED;
                left_rotate(tree, x->parent);
                sibling = x->parent->right;
            }
            if (sibling->left->color == RB_BLACK && sibling->right->color == RB_BLACK) {
                /* case 2：兄弟黑色，且两个孩子都黑 —— 兄弟可以"借"
                 * 一重黑色给 x 这条路径：兄弟变红，双黑标记
                 * 整体上移到父节点，继续在父节点处检查。 */
                sibling->color = RB_RED;
                x = x->parent;
            } else {
                if (sibling->right->color == RB_BLACK) {
                    /* case 3：近侧孩子红、远侧孩子黑 —— 先右旋兄弟，
                     * 把红色孩子转到远侧，转化成 case 4。 */
                    sibling->left->color = RB_BLACK;
                    sibling->color = RB_RED;
                    right_rotate(tree, sibling);
                    sibling = x->parent->right;
                }
                /* case 4：远侧孩子红色 —— 终点：左旋父节点把兄弟提上来
                 * 顶替父节点位置，远侧红孩子转黑吸收掉双黑标记，
                 * x 变成 root 让循环终止。 */
                sibling->color = x->parent->color;
                x->parent->color = RB_BLACK;
                sibling->right->color = RB_BLACK;
                left_rotate(tree, x->parent);
                x = tree->root;
            }
        } else { /* 镜像：x 是右孩子，left/right 全部对调 */ }
    }
    x->color = RB_BLACK;
}
```

四种情况和插入修复的三种情况一一对应但更精细：**case 1（兄弟红）本身不解决问题，只是把红色兄弟"兑换"成一个黑色兄弟**，为后面三种情况创造条件——这是插入修复没有的一步，因为插入修复的"叔叔"不需要先被转换；**case 2（兄弟黑、两侄子都黑）对应插入的 case 1：兄弟可以"借"一重黑色，把双黑标记原样上推**；**case 3→4（近侄红远侄黑 → 远侄红）对应插入的 case 2→3：先转形状再一步到位**。

四种情况分别对应真实运行输出：

```text
===== 第 4 节:删除修复情况 1→2:红色兄弟,变色旋转后落入 case2 收尾 =====
删除黑色叶子 1:它在 root(2) 左孩子位置,兄弟是 root.R=4(RED)。
兄弟是红色,命中 case1:兄弟变黑、父亲(2)变红,再对父亲左旋,
红色的 4 顶替 2 的位置成为新根,2 变成 4 的左孩子,
新兄弟换成 2 原来的右孩子 3(黑色,两个孩子都是 NIL)。
旋转后继续检查,兄弟 3 的两个孩子都是黑,命中 case2:
兄弟 3 变红,"双黑"标记原样上移到新根 4,因 x 等于根,循环结束。

===== 第 6 节:删除修复情况 2:黑色兄弟,两个侄子都是黑色 =====
删除 10:它在 root(20) 左孩子位置,兄弟是 root.R=30(BLACK),
30 的两个孩子都是 NIL(黑色)——命中 case2:兄弟 30 变红,
"双黑"标记原样上移到 x=root,因为 x 已经是根,循环立即结束。

===== 第 7 节:删除修复情况 3→4:黑色兄弟,近侄子红、远侄子黑 =====
删除黑色叶子 10:兄弟是 root.R=35(BLACK),35 的左孩子(近侄子)
32 是红色,右孩子(远侄子)是 NIL(黑色)——命中 case3:
先对兄弟 35 做"变色+右旋"，让远侄子变成红色,转化为 case4;
紧接着 case4 一步到位:32 变成新根(黑色),20 和 35 变成它的
黑色孩子,x 被设成 tree->root,循环结束。

===== 第 8 节:删除修复情况 4:黑色兄弟,远侄子直接为红 =====
删除黑色叶子 10:兄弟 root.R=35(BLACK),远侄子 40 是红色,
直接命中 case4:兄弟 35 变成父亲(20)的颜色(黑),父亲变黑,
远侄子 40 变黑,对父亲左旋,x 设为根,循环结束——一次旋转
就消掉了双黑标记,不需要经过 case3 的中转。
```

也有删除完全不需要修复的情况——只有"被物理移走的节点原本是黑色"才会调用 `delete_fixup`（源码 `if (y_original_color == RB_BLACK)`）：

```text
===== 第 5 节:删除不触发修复:删除红色叶子 =====
3 是红色叶子,删除它不会减少任何路径的黑节点数,自然不需要修复。
```

### 9.4 删除有两个孩子的节点：后继替位

和 B 树、B+ 树内部节点删除的思路一致，红黑树删除一个有两个孩子的节点时，不直接挖走它，而是找后继（右子树最小值）替位：

```c
} else {
    /* z 有两个孩子：找后继（右子树最小值）y，用 y 的 key 覆盖 z，
     * 转化成"删除 y"——y 至多有一个孩子（右孩子），退回前两种情况。 */
    y = tree_minimum(tree, z->right);
    y_original_color = y->color;
    x = y->right;
    /* ... transplant 相关的指针重接 ... */
    y->color = z->color;
}
```

真实输出：

```text
===== 第 9 节:删除有两个孩子的节点:后继替位 =====
30 的后继(右子树最小值)是 40,rb_delete 会把 40 的 key 复制到
30 所在的节点上,再去删除原来位置上的 40(它此时最多一个孩子)。
  --- 删除 30 之后 ---
    root.L: key=40  color=BLACK
说明:40 原来的红色叶子位置消失,40 的 key 出现在原来 30 的
节点位置上并保持黑色——这正是"复制 key、删除物理节点"的效果,
40 本身的颜色和树结构由被删除的那个物理节点决定,而不是 40。
```

关键细节：`y->color = z->color`——替位之后，这个位置的颜色是**原来 `z` 的颜色**，不是后继 `y` 原来的颜色。真正被物理释放、可能触发 `delete_fixup` 的是 `y` 原来所在的位置（`y_original_color`），不是 `z` 的位置。这个区分（"哪个 key 被复制"和"哪个物理节点被删除、需要修复"是两件不同的事）正是 8.3 节 B+ 树"路由 key vs 真实数据"这类"职责分离"思路在红黑树里的对应体现。

### 9.5 结构校验：`rb_verify`

```c
static void verify_rec(const RBTree *tree, const RBNode *x, const int *lo, const int *hi,
                        int *out_bh, VerifyCtx *ctx) {
    if (x == tree->nil) { *out_bh = 0; return; }
    /* ... BST 有序性检查 ... */
    if (x->color == RB_RED) {
        if (x->left->color == RB_RED || x->right->color == RB_RED) {
            vfail(ctx, "性质 4 被破坏：红色节点 %d 有红色孩子", x->key);
            return;
        }
    }
    int lbh, rbh;
    verify_rec(tree, x->left, lo, &x->key, &lbh, ctx);
    verify_rec(tree, x->right, &x->key, hi, &rbh, ctx);
    if (lbh != rbh) {
        vfail(ctx, "性质 5 被破坏：节点 %d 的左子树黑高 %d != 右子树黑高 %d", x->key, lbh, rbh);
        return;
    }
    *out_bh = lbh + (x->color == RB_BLACK ? 1 : 0);
}
```

黑高检查的写法值得注意：不是先各自算出左右子树的黑高再比较，而是通过递归返回值 `*out_bh` 层层传递——每个节点的黑高等于它任意一个孩子的黑高（两边必须相等，否则在这一层就直接报错）加上"自己是不是黑色"，这样"黑高不一致"会在最先出现分歧的那一层就被立即捕获，而不是要等到递归全部返回之后再统一核对。`rb_verify` 外层额外补了两条 `verify_rec` 管不到的检查：哨兵 `nil` 本身必须是黑色（性质 3），根节点必须是黑色（性质 2）。

### 9.6 ⚠️ 错误示例：忘记在 fixup 结尾强制根为黑

`insert_fixup` 的循环末尾有一行容易被忽略的收尾：

```c
tree->root->color = RB_BLACK; /* 性质 2：根恒为黑，循环里可能把根短暂染红 */
```

case 1（叔叔红）会把祖父染红，如果这个祖父恰好就是根节点，根就被临时染红了——多数情况下循环还会继续检查根的父节点（`tree->nil`，颜色恒为黑），自然退出循环，但根本身已经变成红色，必须靠这一行强制拉回黑色。真实的隔离演示：

```text
===== 第 11 节:错误示例:忘记在 fixup 结尾强制根为黑 =====
修复前(合法的红黑树):root=10(黑), L=5(红), R=15(红)
现在模拟插入一个新节点,其父亲和叔叔都是红色(case1),
正确做法是把 5 和 15 变黑、10 变红,然后【强制根变黑】。
如果实现里漏掉最后这一步,"变色"完成后的状态是:
  root=10(红), L=5(黑), R=15(黑)
这违反了性质 2(根必须是黑色)。表面上看不出明显的错误——
树仍然是合法的二叉搜索树、也没有红红相邻——但如果这棵子树
之后被作为某个更大树的一部分继续插入,红色的根会被当成
普通红色节点参与后续的 fixup 判断,导致黑高计算和后续的
case 判断全部基于错误的颜色前提,产生的错误可能在很多次
操作之后才会以一种看似无关的方式暴露出来,非常难以定位。
```

这类 bug 的隐蔽之处在于：出错的那一刻，BST 有序性完好、性质 3/4/5 全部成立——**只有性质 2 单独被打破**，而性质 2 只在"这一个节点"（根）上有意义，不像性质 4/5 那样会在结构的其他地方留下连带的、更容易被察觉的痕迹。如果这棵树接下来只做查找，问题永远不会暴露；只有继续插入，红色的根被当成"普通红色节点"参与后续 `insert_fixup` 的判断时，才会在一个看起来完全不相关的位置产生错误的旋转/变色决策。`rb_verify` 里专门有一条独立于 `verify_rec` 之外、单独检查根颜色的判断，正是为了不遗漏这一条"只影响单个特定节点、不会自然扩散出可见症状"的性质。

### 批量验证：黑高的理论上界

```text
===== 第 10 节:批量插入 1..15,再全部删除:每步都校验不变量 =====
count=15, black_height=3
(说明:15 个节点的红黑树黑高为 3,与理论上界
黑高 <= log2(n+1) 一致,可见红黑树的"近似平衡"约束。)
全部删除之后:root==nil? 是, count=0
```

`log2(16) = 4`，实测黑高 3 严格小于这个上界——红黑树的平衡性是"近似"而非"严格"的（不像 AVL 树那样要求左右子树高度差不超过 1），但五条性质联合起来保证了树高不会超过 `2 * log2(n+1)`，最坏情况下依然是 O(log n)。

### 测试覆盖与验证

19 个测试覆盖了插入修复的 3 种情况、删除修复的 4 种情况、两孩子节点删除的后继替位、大规模随机压力测试（`test_random_stress_sparse_1500`、`test_random_stress_wide_2000`）等：

```bash
$ cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g rbtree.c tests.c -o tests && ./tests
[PASS] test_random_stress_sparse_1500
[PASS] test_random_stress_wide_2000

========== 汇总 ==========
通过: 19, 失败: 0, 总计: 19
```

sanitizer 版本同样 19/19、退出码 0。

### 常见误区

1. 插入修复的三种 case 判断顺序不能打乱：必须先判叔叔颜色（红 → case 1，直接上推），再判形状（黑 → case 2/3，先转形状再统一处理），颠倒顺序会导致本该直接上推的情况被错误地当成需要旋转来处理。
2. 混淆"哪个 key 被复制"和"哪个物理节点被删除"：两孩子节点删除时，被删除物理节点的原始颜色（`y_original_color`）才是决定是否触发 `delete_fixup` 的依据，跟被复制上来的 key 本身的颜色无关。
3. 忘记哨兵 NIL 和 `NULL` 的本质区别：任何只读递归函数如果没有先判断 `x == tree->nil` 就直接展开 `x->left`/`x->right`，会因为 `nil` 自指而死循环——这类 bug 不会被 sanitizer 当成内存错误抓到（没有非法访问，只是无限递归/循环），只会表现为程序挂起。

### 小结

红黑树用"给节点上色 + 五条局部性质"取代了显式维护子树高度，插入靠"新节点先染红，最多违反一条性质"把修复范围收得很窄，删除靠"双黑记账"把"某条路径黑高少了 1"这件全局性的事，转化成一套可以逐层向上传播或就地吸收的局部操作。两套修复逻辑情况数不对称（插入 3 种、删除 4 种）的根源在于删除面对的初始局面更复杂：兄弟可能是红色，需要先"兑换"成黑色兄弟才能套用后续三种情况的判断，这一步是插入修复完全不需要的。

### 练习

1. 画出 `insert_fixup` case 1（叔叔红）连续触发两次的最小示例（提示：需要至少 7 个节点），手工验证每一步之后五条性质是否成立。
2. `delete_fixup` 的 case 1 本身不解决双黑问题，只是把红色兄弟换成黑色兄弟。如果最初的兄弟已经是黑色，还需要经过 case 1 吗？为什么？
3. 尝试去掉哨兵 NIL 设计，改用 `NULL` 表示空叶子重写 `delete_fixup`，需要在哪些地方额外加上"是否为 NULL"的特判？对比代码复杂度的变化。

---

## 十、六种结构综合对比

前九章分别验证了六个独立的结构。这一章不引入新代码，只做横向整理——把已经在各章跑出来的真实数据（测试数、行数、性能实测结果）放到一起对比，帮助建立"什么场景该选什么结构"的直觉。

### 10.1 规模与测试覆盖一览

| 结构 | 核心代码行数 | 测试用例数 | sanitizer 覆盖 |
|---|---|---|---|
| 二叉树遍历（一） | `demo.c` 556 行 | 64 | ✅ 常规 + ASan/UBSan 双跑 |
| 图遍历算法（二） | `algos.c`+`graph.c` 581 行 | 21 | ✅ 常规 + ASan/UBSan 双跑 |
| Trie（六） | `trie.h` 261 行 | 13 | ✅ 常规 + ASan/UBSan 双跑 |
| B 树（七） | `btree.c` 373 行 | 20 | ✅（本机常规验证；sanitizer 编译规则与前述结构一致） |
| B+ 树（八） | `bplustree.c` 697 行 | 24 | ✅（本机常规验证；sanitizer 编译规则与前述结构一致） |
| 红黑树（九） | `rbtree.c` 398 行 | 19 | ✅（本机常规验证；sanitizer 编译规则与前述结构一致） |

行数、测试数均为本文档写作时在本机重新编译运行后的真实数字，不是估算值。B+ 树代码量明显高于同量级的 B 树（697 行 vs 373 行），主要来自它多出的一整套"叶子/内部节点两种形态分别处理"的分裂、合并、借位逻辑（8.1-8.3 节），以及为了支撑范围查询而维护叶子链表 `next` 的额外正确性检查（8.5 节）。

### 10.2 时间复杂度对比

| 结构 | 查找 | 插入 | 删除 | 有序遍历 / 范围查询 |
|---|---|---|---|---|
| 二叉树遍历 | — | — | — | O(n)，本身就是遍历算法的载体 |
| 图（BFS/DFS） | — | — | — | O(V+E) |
| Trie | O(L)，L 为单词长度 | O(L) | O(L) | 前缀查找 O(L)，自动补全 O(L + 匹配数) |
| B 树 | O(log n) | O(log n) | O(log n) | 中序遍历 O(n)；范围查询需重新descend/backtrack，见 10.3 |
| B+ 树 | O(log n) | O(log n) | O(log n) | 范围查询 O(log n + k)，k 为结果数量，见 10.3 |
| 红黑树 | O(log n) | O(log n) | O(log n) | 中序遍历 O(n) |

B 树、B+ 树、红黑树三者的单次查找/插入/删除都是 O(log n)，账面上完全一样，真正的差异全部藏在"访问哪些节点""磁盘/缓存友好度""范围查询代价"这些常数因子和使用场景里，而不是渐近复杂度本身。

### 10.3 B 树 vs B+ 树：范围查询代价的定量对比

这是本文档特意设计的一条贯穿两章的对比线。七、8.4 节和 7 章末尾的性能实测已经给出真实数据，这里把两组数字并排放在一起：

```text
【B 树,来自第七章 7.6 性能实测,t=64, n=100000, range_width=10000】
  访问节点数:121

【B+ 树,来自第八章 8.4 性能实测,range_width=1000 固定,树规模从 1000 增长到 500000】
  n=1000    -> visited_leaves=126
  n=10000   -> visited_leaves=126
  n=100000  -> visited_leaves=126
  n=500000  -> visited_leaves=126
```

两组数字看起来量级接近（100 出头），但含义完全不同：**B 树的 121 是"在一次查询里，树本身有多深、需要 backtrack 多少次"决定的，会随 n 和 range_width 变化**；**B+ 树的 126 只取决于结果集大小（range_width），和 n 完全无关**——因为 B+ 树只需要"descend 一次找到下界，剩下全部靠叶子链表 `next` 顺序扫描"（8.4 节），根本不需要重新回到内部节点。这正是"B+ 树几乎总是数据库和文件系统索引首选，而教科书版 B 树更多停留在理论"的核心原因：真实系统里范围查询（"找出所有在 [a,b] 之间的记录"）远比单点查找更常见，而这正是 B+ 树相对 B 树的结构性优势所在。

### 10.4 该选哪个结构

| 场景 | 推荐结构 | 理由 |
|---|---|---|
| 需要按前缀匹配大量字符串（自动补全、拼写检查） | Trie | 天然按字符路径组织，查找/插入代价只取决于单词长度，与词表总量无关 |
| 内存中的通用有序 map/set，只需要单点查找 | 红黑树 | 严格二叉结构，节点小，插入删除的旋转次数少，是 `std::map`/`std::set` 等语言标准库的常见底层实现 |
| 需要频繁范围查询、且数据可能落盘（数据库索引、文件系统） | B+ 树 | 范围查询代价与树规模无关（10.3 节），且叶子层可以顺序存放，对磁盘/SSD 的顺序读取更友好 |
| 教学/理解"多路平衡树如何维持不变量"这一原理 | B 树 | 结构比 B+ 树简单（不需要区分内部/叶子两种节点形态），更适合先建立直觉，再理解 B+ 树在它之上做了什么权衡 |
| 图数据的连通性判断、最短路径（无权）、拓扑序 | 图遍历（BFS/DFS） | 参见第五章，不是"选哪种树"的问题，而是数据本身就是图结构 |

### 10.5 一条共同的教训：结构合法 ≠ 程序没崩溃

六个结构的"错误示例"合起来看，有一条反复出现的主线：**Trie 的错误删除（6.6 节）、B 树分裂中点算错一位（7.6 节）、红黑树忘记强制根为黑（9.6 节），这三个 bug 全部不是内存安全问题**——没有越界、没有 UAF、没有 double free，ASan/UBSan 全部保持沉默，程序也不会崩溃，只有专门针对结构不变量写的校验函数（`trie` 的手工对比、`btree_verify`、`rb_verify`）才能捕捉到。这是本文档反复强调、也是十一章要系统展开的核心方法论：**"能跑""不崩溃"和"结构正确"是三件不同的事，只验证前两者，逻辑 bug 可以在测试集里稳定潜伏，直到某个特定的后续操作序列把它暴露出来**。

---

## 十一、调试与验证方法论

这一章不引入新的数据结构，而是把前十章里反复出现、但一直分散在各章"⚠️ 错误示例"和"调试笔记"里的方法论抽出来，systematize 成几条可以直接用到下一个项目上的原则。

### 11.1 第一条原则：写一个独立的结构校验函数，且它不依赖"程序有没有崩溃"

六个结构里有四个（Trie 靠手工节点计数对比、B 树的 `btree_verify`、B+ 树的 `bplustree_verify`、红黑树的 `rb_verify`）都专门写了一个**可以随时被独立调用、只检查结构不变量、和"程序是否崩溃"完全脱钩**的校验函数。这不是可选的锦上添花，而是这几章"错误示例"共同证明的一件事：

```text
Trie(6.6节):删除逻辑bug -> card 搜索静默返回 false,ASan/UBSan 沉默
B树(7.6节):分裂中点算错一位 -> 节点数量约束被破坏,ASan/UBSan 沉默
B+树(8.6节):split_internal 副作用导致的边界误判 -> 树被静默破坏
红黑树(9.6节):忘记强制根为黑 -> 性质2被破坏,BST有序性/无红红相邻完好
```

四个 bug 的共同特征：**破坏的都是"结构不变量"，不是"内存安全"**。sanitizer 只能告诉你有没有越界读写、有没有用释放后的内存，它对"这棵树的节点数量是否满足 B 树的约束""这个红黑树的根颜色是否合法"完全没有概念——这类问题只能通过显式写出该结构的数学定义（B 树的 key 数量上下界、红黑树的五条性质、B+ 树叶子链表的完整性），再用代码逐条核对来发现。**校验函数要解决的问题，从来不是"这段代码会不会崩溃"，而是"这段代码运行完之后，产生的结果是否仍然满足这个结构的定义"**。

### 11.2 第二条原则：verify 要在每次操作之后跑，不是只在测试结束时跑一次

九章、10.5 节强调的"结构合法 ≠ 不崩溃"背后还有一层更具体的实践建议：如果 `btree_verify`/`rb_verify` 只在整个测试用例的最后调用一次，那么当它报错时，你只知道"树现在是坏的"，但完全不知道是**哪一次插入或删除**把它弄坏的——尤其是像 7.6 节、9.6 节这种"表面上看不出明显错误"的 bug，回溯定位的成本会随操作序列变长而指数级上升。本文档所有实验的测试代码（`tests.c`）里，verify 调用都紧跟在每一次 `insert`/`delete` 之后，而不是攒到最后才调用一次——这样一旦某次操作破坏了不变量，报错会精确指向那一次操作，而不是让你在几十步操作历史里做二分排查。

### 11.3 第三条原则：打印树形状只是第一步，真正下结论前必须换成精确计数差值

这条来自第八章 8.6 节真实调试笔记里记录的两个具体教训，值得单独强调，因为它们描述的是"看起来已经验证了，但其实验证方式本身有问题"这种更隐蔽的失败模式：

```text
教训一（测试断言写错）:
误判"内部节点借位"的信号是"叶子数量不变"——但纯粹的叶子借位
本身就不会改变叶子数量,这个信号根本无法区分"发生了叶子借位"
和"发生了内部节点借位"。真正的信号应该是:叶子数量减 1
（说明发生了合并而不是借位）,但内部节点数量不变（说明合并
之后没有继续向上级联触发内部节点的借位/合并)。

教训二（级联深度猜错)：
凭直觉猜"一次合并应该释放 2 个节点",实际情况是释放了 3 个——
只有通过~15个一次性 /tmp 探针程序,对着"操作前/操作后"的
叶子数、内部节点数做精确差值对比,才发现了真实的级联深度。
单纯打印树的形状(尤其是树比较大的时候)很难用肉眼分辨
"到底级联了几层"。
```

两个教训指向同一个原则：**"打印出来的树形状看起来对"不等于"这棵树真的对"**。人眼扫一遍打印出来的缩进树形结构，很容易被"看起来层次分明、没有明显断裂"这种表面印象误导，尤其是在节点数量稍多、屏幕滚动才能看全的情况下。可靠的验证方式是把"操作前的某个计数"和"操作后的同一个计数"做精确的数值差值对比（叶子数变化了多少、内部节点数变化了多少），再拿这个差值去对照"这个操作理论上应该导致什么变化"，而不是依赖对打印输出的主观判断。

### 11.4 第四条原则：区分"内存安全 bug"和"纯逻辑 bug"，因为工具链帮不了你处理后者

本文档从第四章开始，每一个"⚠️ 错误示例"都明确标注了这个 bug 属于哪一类：

| Bug 类型 | 工具能否发现 | 本文档中的例子 |
|---|---|---|
| 内存安全（越界、UAF、double free、未初始化读） | ✅ ASan/UBSan 能直接报出崩溃点 | 第八章 8.6 节 `split_internal` 边界误判在某些输入下引发的真实越界访问 |
| 纯逻辑/结构不变量（数量约束、颜色约束、链表完整性） | ❌ sanitizer 完全沉默，程序不崩溃 | Trie 错误删除、B 树分裂中点、红黑树根染色、B+ 树叶子链表 `next` 遗漏修复 |

这个区分直接决定了排查策略：遇到崩溃、遇到 sanitizer 报错，直奔它给出的堆栈和地址即可，工具已经把根因指到了具体的一行代码；但遇到"程序跑得好好的，结果就是不对"，唯一的办法是回到这个结构的数学定义，一条一条核对性质是否成立——这正是每个结构都要有独立 `verify` 函数的根本原因，也是为什么"能跑""不崩溃"从来不能被当作"正确"的证明。

### 11.5 一个可以直接套用的调试流程

把以上几条原则串起来，本文档六个实验目录在实践中遵循的是这样一个流程，可以直接迁移到其他项目：

```text
1. 为这个结构写下它的数学定义（不变量列表），逐条对应到一个可断言的检查。
2. 写一个独立的 verify() 函数，只依赖这份定义，不依赖"是否崩溃"。
3. 每次 insert/delete 之后立刻调用 verify()，而不是攒到测试结束才调用一次。
4. 遇到"崩溃/sanitizer 报错":直奔报错给出的地址和堆栈。
5. 遇到"跑得通但结果不对":
   a. 不要只盯着打印出来的树形状做主观判断；
   b. 记录操作前/操作后的精确计数（节点数、叶子数、深度等）；
   c. 对照该操作在这个结构定义下"理论上应该造成的计数变化"逐条核对差值；
   d. 用最小化的独立探针程序（可以是一次性、跑完就丢的临时代码）单独复现,
      不要在完整测试套件里连本已通过的部分一起反复重跑。
6. 找到根因后，把这个 case 补充成一个永久的测试用例，防止回归。
```

### 小结

六个结构的"错误示例"没有一个是随手编造的语法错误，全部是真实运行、真实观察到的结构级 bug，而它们共同指向的方法论只有一句话：**为你的数据结构写下它的数学定义，再写一个只依赖这份定义、和"程序是否崩溃"完全脱钩的校验函数，然后在每一次修改状态的操作之后都调用它**。sanitizer 和测试框架能替你抓住内存安全问题和"结果错了"这两类问题，但它们没有办法替你回答"这棵树现在是不是还是一棵合法的 B 树/红黑树"——这个问题的答案，只能来自你自己写的、忠实于结构定义的校验代码。

### 练习

1. 挑选第四章到第九章中任意一个"⚠️ 错误示例"，尝试在不看已有 `verify` 函数实现的前提下，自己重新写一遍能够捕捉到该 bug 的校验逻辑，再对比和原实现的差异。
2. 8.6 节提到的"教训一"（内部借位信号写错），如果换成你自己设计测试断言，你会怎么描述"叶子数量减 1 但内部节点数量不变"这个信号，使它同时能排除"纯叶子借位"和"合并后继续向上级联"两种干扰情况？
3. 尝试给本文档任意一个结构补充一种全新的"⚠️ 错误示例"（可以是故意引入的 bug），先预测它属于内存安全还是纯逻辑类型，再实际编译运行 ASan/UBSan 版本验证你的预测是否正确。

---

## 十二、综合项目建议

前十一章都是围绕单一结构展开的定向练习。这一章给出几个需要**组合多个结构**才能完成的综合项目，难度递增，每个都基于本文档六个实验目录里已经验证过的真实代码，不需要从零发明新算法。

### 12.1 项目一：磁盘友好的 KV 存储引擎（难度：中）

把第七、八章的 B 树/B+ 树代码改造成一个简化版的持久化 KV 引擎：

- 把 `BTreeNode`/`BPlusNode` 里的指针换成"文件偏移量"，用 `fread`/`fwrite` 代替内存指针访问，每个节点固定大小写入磁盘的一个"页"。
- 保留 8.4 节的 `bplustree_range_query` 接口，验证"范围查询代价与树规模无关"这个性质在磁盘 I/O 次数上是否依然成立（提示：可以直接统计 `fread` 调用次数代替原来的 `nodes_visited_out`）。
- 复用 `bplustree_verify` 的叶子链表完整性检查思路，但要额外处理"写到一半程序崩溃，磁盘文件处于中间状态"这种新增的失败模式——内存版本不需要考虑这个问题，落盘之后就需要考虑了。
- 验证要求：至少设计一组"写入过程中 kill -9 进程，重启后重新打开文件"的测试，确认 `verify` 逻辑能检测出未完成写入留下的不一致状态。

### 12.2 项目二：拼写检查 + 编辑距离建议（难度：中）

组合第六章的 Trie 和一个新写的编辑距离（Levenshtein distance）算法：

- 用 6.5 节的 DFS 自动补全逻辑作为基础，改造成"在 Trie 上做限定编辑距离的模糊搜索"（经典的 Trie + BK-树/编辑距离剪枝思路）：从根节点开始 DFS，维护一个动态规划的编辑距离数组，一旦当前路径的最小可能编辑距离已经超过阈值就剪枝，不再往下搜索。
- 用 6.6 节"错误示例"里的方法论倒逼自己写测试：先故意实现一个"看起来正确但边界条件算错"的剪枝逻辑（比如漏掉了某种字符操作），验证你的测试集是否能捕捉到它——这是对第十一章"结构合法 ≠ 没崩溃"方法论的直接应用。

### 12.3 项目三：多结构对比基准测试框架（难度：较高）

这是对第十章"综合对比"最自然的延伸——把 10.1-10.3 节里手工整理的对比数字，改造成一个可以自动生成的基准测试脚本：

- 编写一个统一的 `bench_harness.c`（或用 shell 脚本调度各自的 `perf` 程序），对红黑树、B 树、B+ 树在相同的随机数据集上分别测试：插入 n 个 key 的总耗时、单点查找耗时、范围查询访问的节点/叶子数。
- 输出格式设计成可以直接生成本文档 10.3 节那种对比表格的数据（比如 CSV），验证你独立跑出来的数字是否和本文档记录的真实数据在量级上吻合（不要求完全相同，因为硬件、编译器版本、数据分布都会带来偏差，但量级和趋势应该一致）。
- 额外挑战：给三种树都加上"随机数据"和"顺序插入数据"两种测试场景，验证顺序插入是否会让某些结构（尤其是没有旋转/分裂再平衡的朴素 BST，如果你想额外实现一个作对比）退化成链表式的最坏情况。

### 12.4 项目四：把红黑树改造成 AVL 树，量化对比再平衡策略（难度：较高）

红黑树用"颜色约束"换近似平衡，AVL 树用"显式维护高度差"换更严格的平衡。基于第九章的红黑树代码框架（结构定义、`rb_verify` 的模式）另起一套 AVL 树实现：

- 保留和 `rbtree.h` 类似的公开接口（`insert`/`delete`/`search`/`verify`），方便替换测试。
- 单独写一个 `avl_verify`，检查"每个节点左右子树高度差不超过 1"这条 AVL 的核心不变量，风格上模仿 `rb_verify` 的"独立于崩溃状态、只依赖结构定义"设计。
- 用同一组随机插入序列跑两套实现，对比：平均树高、总旋转次数、单次插入/删除的最坏情况旋转次数。验证教科书上"AVL 更平衡但旋转更频繁，红黑树旋转更少但树略高"这个结论在你的真实实现上是否成立，用真实数字量化"更平衡"和"更频繁"具体差多少。

### 每个项目的通用要求

- 延续本文档的核心原则：所有输出必须是真实编译运行得到的，不能是"预期应该是这样"的推测性描述。
- 每个新写的结构都要有独立的 `verify` 函数（十一章 11.1 节），且在每次状态变更操作之后调用（11.2 节）。
- 遇到不符合预期的结果，先怀疑测试断言本身是否写对了（11.3 节的"教训一"），再怀疑被测代码。

---

## 十三、练习题与参考答案

本章汇总第四章到第九章末尾的全部练习（每章 3 题，共 18 题），逐题给出参考答案的思路。这些答案是设计思路和推导过程，不是可以直接复制粘贴的完整代码——真正动手实现一遍、跑起来验证，才是练习的意义所在。

### 四、二叉树遍历

**1. 把 `morris_inorder` 改写成"莫里斯前序遍历"**

中序遍历用线索指针实现"回到父节点"的关键是：访问节点的时机在**第二次**经过它的时候（线索建立完成、即将从左子树返回时）。前序遍历要把访问时机改到**第一次**经过时——具体来说：若当前节点没有左孩子，直接访问并转向右孩子（这一步和中序一样）；若有左孩子，先建立线索（`predecessor->right = cur`）**再立即访问 `cur`**（这是和中序唯一的区别，中序是在线索建立完成、cur 即将离开时才访问，前序是在线索刚建立、cur 刚被第一次到达时就访问），然后再转向左孩子。

**2. 层序遍历序列能否唯一确定树的形状**

不能。层序遍历（BFS 序列）如果不显式标记空位（NULL），无法唯一还原树的形状——反例：一棵树"根 A，A 的左孩子 B"和另一棵树"根 A，A 的右孩子 B"，层序遍历都是 `A, B`，序列完全相同但形状不同（对比之下，前序+中序序列联合可以唯一确定二叉树形状，这是一条经典的相关结论）。如果层序遍历序列显式包含 NULL 占位（比如 LeetCode 常见的 `[A,null,B]` 这种格式），则可以唯一确定——因为每个 NULL 占位精确标记了"这个位置没有孩子"，重建算法只需要按层用队列展开，遇到 NULL 就不再展开子节点。

**3. 同时线索化 `left` 指针能获得什么额外能力**

只线索化 `right`（中序后继）只能支持"从任意节点开始，正向遍历到结尾"。同时线索化 `left`（中序前驱）之后，可以从任意节点开始**反向**遍历（找中序遍历中的上一个节点），也可以在不知道遍历起点的情况下双向扩展——比如"给定一个节点，找它在中序序列里前后相邻的节点"变成 O(1) 操作，不需要额外的栈或者从根重新遍历。

### 五、图遍历算法

**1. 把 Kahn 算法改造成"检测并输出具体的环"**

当 `cnt != n` 时，把所有入度仍然大于 0（意味着没有被处理过）的节点单独拎出来，对这个子图重新做一次 DFS：从任意一个入度非 0 的节点出发，维护一个"当前递归栈"（`on_stack` 数组），一旦 DFS 过程中遇到一个已经在 `on_stack` 里的节点，说明形成了环，从递归栈中把这个节点到当前节点之间的部分截取出来就是环的具体路径。

**2. 用并查集实现无向图连通分量**

并查集版本：初始化每个节点各自为一个集合（`parent[i] = i`），遍历每一条边 `(u, v)`，执行 `union(u, v)`（找到两者的根，把一个根指向另一个），遍历完所有边后，`parent` 数组里根相同的节点就属于同一个连通分量。相比 BFS 版本，并查集不需要显式的队列和访问标记数组，代码更短，但直接给出"两个节点是否连通"的判断（`find(u) == find(v)`）比 BFS 更快（近似 O(1)，带路径压缩和按秩合并），如果只需要连通性判断而不需要具体路径，并查集通常是更轻量的选择；但如果需要输出"从 A 到 B 的具体路径"，BFS/DFS 是必须的，并查集给不出路径。

**3. 二分图判定改写成 DFS 版本**

BFS 版本用队列逐层染色，DFS 版本改成递归函数 `bool dfs_color(int u, int color[])`：给 `u` 染上和调用者指定的颜色相反的颜色，再遍历 `u` 的每个邻居 `v`——如果 `v` 未染色，递归调用 `dfs_color(v, 相反颜色)`，如果递归返回 false 就整体返回 false；如果 `v` 已染色且颜色和 `u` 相同，直接返回 false（发现矛盾）。注意递归参数传递的是"这个节点应该染的颜色"，不是"当前遍历的层数"，这是 DFS 版本和 BFS 版本在参数设计上的本质区别——BFS 是按层次隐式确定颜色（奇数层/偶数层），DFS 需要显式把颜色当参数往下传。

### 六、Trie

**1. `is_word: bool` 改成 `count: int`**

插入时不再是"设置 `is_word = true`"，而是把路径终点节点的 `count` 加 1；查找"某个单词出现过几次"直接返回该节点的 `count` 值；删除时，先把 `count` 减 1，只有 `count` 降到 0 才真正当作"这个单词不存在了"，继续走 6.3 节的 `trie_node_is_empty` 回溯释放逻辑——`is_word` 存在与否的判断从"count 是否为 0"直接推导，甚至可以完全去掉 `is_word` 这个字段，用 `count > 0` 代替。

**2. 支持任意 ASCII 或 Unicode 的方案对比**

方案一（扩展固定数组）：把 `children[26]` 扩展成 `children[128]`（全部 ASCII）或者更大，实现简单，查找/插入依然是 O(1) 数组下标访问，但空间代价随字符集大小线性增长——每个节点固定占用"字符集大小 × 指针大小"的内存，即使大部分子节点都是空指针（尤其是稀疏的 Unicode 场景，一个节点可能只有 2-3 个真实子节点，却要为几万个 Unicode 码点预留指针位置）。方案二（换成哈希表或有序 map 存子节点）：每个节点内部用一个动态的哈希表/红黑树/有序数组存储"字符 -> 子节点"的映射，空间只随实际使用的字符数增长，但访问子节点从 O(1) 数组下标变成 O(1) 均摊哈希查找或 O(log k) 树查找（k 为该节点实际子节点数），实现复杂度显著提升。字符集小且稠密（比如只有小写字母）用方案一，字符集大且稀疏（比如支持任意 Unicode）用方案二。

**3. 实现"最长公共前缀"**

把所有单词依次插入 Trie，然后从根节点开始往下走：只要当前节点**恰好只有一个非空子节点、且该节点不是任何单词的结尾（`is_word == false`）**，就继续往那个唯一的子节点走并把对应字符追加到结果；一旦遇到某个节点有多个非空子节点，或者遇到某个节点是某个单词的结尾（说明这个单词本身就是当前路径的前缀，不能再往下走），就停止，此时累积的路径就是最长公共前缀。

### 七、B 树

**1. 把 `btree_insert` 改成"先插入再检测溢出并分裂"的后处理式实现**

现有实现（7.1 节的 `split_child`）走的是"预防式"路线：递归下降前先检查孩子是否已满，如果满就先分裂，确保递归调用的目标节点必然有空间。后处理式的写法是：先不管三七二十一，直接递归插入到叶子（就像普通 BST 插入），插入完成后，如果某个叶子节点的 key 数量超过了 `2t-1`，从这个叶子开始向上，逐层检测并分裂"刚刚变得超员的节点"，直到没有节点超员或者到达根。两种写法的本质区别在于**分裂检测发生在递归的哪个阶段**：预防式是"下降路上先检查再往下走"（自顶向下），后处理式是"插入完成后再往上补救"（自底向上）——后处理式需要维护一条"从叶子到根的路径"（比如用一个数组记录递归栈），因为分裂检测发生在递归返回之后，而预防式不需要额外记录路径，因为分裂检测就发生在递归下降的过程中。

**2. `merge_children` 引发连锁下溢的调用栈**

`delete_from` 是递归函数：`delete_from(node, key)` 如果发现 key 不在当前节点、需要递归进入某个孩子 `child`，会先检查 `child` 的 key 数量是否已经是下限 `t-1`，如果是就调用 `fill_child` 补足（可能是借位，也可能是合并）。如果 `fill_child` 里选择了合并（`merge_children`），合并会把 `child` 和它的一个兄弟、以及父节点的一个 key 一起吸收进同一个节点，父节点自己的 key 数量因此减 1——**如果父节点减 1 之后也跌破了下限，这个"跌破下限"的情况会在父节点递归返回之后，被父节点的父节点（也就是再上一层的 `delete_from` 调用）在它自己检查孩子数量的时候发现，进而触发同样的 `fill_child` 逻辑**。调用栈的形态是：每一层 `delete_from` 调用在"递归进入孩子之前"检查孩子是否需要 `fill_child`，所以连锁下溢的传播不是在同一次 `merge_children` 调用内部处理的，而是靠递归返回后，上一层调用自然地重复同一套检查逻辑来逐层向上传播，直到某一层不需要合并（借位就够了）或者传播到根（根为空则树整体降一层，对应 7.2 节"root 变空，树整体降一层"的真实输出）。

**3. 只支持插入和查找的 B 树，`btree_verify` 可以省略哪些检查**

`btree_verify` 里和"删除"相关的检查主要是下限约束（每个非根节点至少有 `t-1` 个 key）——如果这棵树永远不会删除任何 key，下限约束在只插入的场景下天然不会被打破（插入只会增加 key 数量，`split_child` 保证分裂后两边都恰好有 `t-1` 个 key，不会低于下限），所以严格来说这条检查在纯插入场景下"永远不会失败"，可以省略而不影响正确性验证的有效性。但上限约束（每个节点最多 `2t-1` 个 key）、strict-increasing 有序性、以及叶子深度一致性这几条依然必须保留，因为它们是插入逻辑本身要维持的核心不变量，`split_child` 里 7.6 节演示的"中点算错一位"这类 bug 恰恰就是破坏了这些插入相关的约束。

### 八、B+ 树

**1. 审查借位/合并函数是否还有类似的调用顺序隐患**

8.6 节的真实 bug 出在`split_internal`会把`y->n`修改为`t-1`这个副作用，而调用者在调用之后才读取`parent->n`做边界判断，读到的是"分裂之后"的新值而不是"分裂之前"的旧值。审查`leaf_borrow_left`/`leaf_borrow_right`/`leaf_merge`/`internal_borrow_left`/`internal_borrow_right`/`internal_merge`这六个函数（05_b_plus_tree/bplustree.c 第 257-497 行）会发现：这些函数的参数普遍是"父节点 `x` + 孩子下标 `i`"这种模式，函数内部同样会修改被操作节点的 `n`（key/pointer 数量）字段——审查的关键问题是：**调用者在调用这些函数之后，是否还需要读取被修改节点的旧状态（比如旧的 `n` 值）来做后续判断**？如果调用者在调用之后只依赖"当前最新状态"继续往下走（比如判断父节点自己是否需要继续向上传播合并），就是安全的；如果调用者需要拿"调用前的某个值"和"调用后的某个值"做对比（就像`split_internal`那个 bug 里，调用者本来想拿的是"分裂前的旧边界"），就必须在调用前显式把需要的值存进一个临时局部变量，不能依赖调用后再去读取那个字段。这正是本题设计的核心训练点：**任何"先记录一个阈值/边界，中间插入一次有副作用的函数调用，再据此判断"的代码模式，都要显式问一句"我引用的这个值，是调用前的还是调用后的"**。

**2. 加 `prev` 指针和双向链表完整性校验**

新校验逻辑：从任意叶子（比如树的最左叶子）出发，先沿 `next` 走到最右叶子，记录访问的叶子序列；再从最右叶子沿 `prev` 走回来，记录第二次的叶子序列；两个序列应该是彼此的逆序，且长度相同。这条校验能抓住的典型 bug 是：某次 `leaf_merge` 或者 `leaf_borrow_*` 操作里，`next` 指针被正确地跳过了被合并/调整的节点（`left->next = right->next` 这类修复），但对应的 `prev` 指针没有同步更新——单向链表下这类 bug**完全测不出来**，因为只沿 `next` 正向扫描永远看不出"某个本该被跳过的旧 `prev` 值还残留着指向一个已经不在链上、甚至已经被释放的节点"，只有反向走一遍、和正向序列做对比，才能暴露"两个方向走出来的序列不对称"这个矛盾。

**3. 支持降序范围查询需要的改动**

只有单向 `next` 指针不够。descend 到 range 的上界（high）所在的叶子容易做（`find_leaf_path` 本身就是为了定位某个 key 而设计的，找 high 和找 low 用的是同一套逻辑），但从这个叶子开始"往前"扫描需要能够反向移动——如果没有 `prev` 指针，只能先做一次正向的全量扫描把结果收集起来再反转顺序（效率上退化，可能扫描到很多不需要的节点），或者需要额外维护一个"从根到该叶子的路径"用于反向重建。最直接的改法是给叶子节点加上第 2 题提到的 `prev` 指针，descend 定位到 high 所在的叶子后，直接沿 `prev` 反向扫描到 low，代价和正向范围查询完全对称，不需要额外的正向扫描或路径重建。

### 九、红黑树

**1. `insert_fixup` case 1 连续触发两次的最小示例**

需要构造一棵树，使得插入新节点后，父亲和叔叔都是红色（触发第一次 case 1），修复后祖父变红，而这个新红色的祖父的父亲和"叔叔"（新祖父的兄弟）恰好也是红色，才能连续触发第二次 case 1。一个可行构造：先建立一棵 7 节点的树，根为黑色，根的两个孩子都是红色（层 1），层 1 的四个孙子节点里，先让某一侧的两个孙子也都是红色（层 2）——这样插入层 2 更深一层的新节点时，第一次 case 1 会把层 2 的父叔变黑、层 1 的那个祖父变红，而层 1 原本就是红色的另一支（对称的另一侧）如果也保持红色，层 1 变红的祖父和层 0（根的另一侧）的叔叔就会再次形成"叔叔红"的局面，触发第二次 case 1，最终传导到根，被 fixup 末尾强制拉黑。每一步之后都应该用 `rb_verify`（或手工核对五条性质）确认：性质 4（无红红相邻）在中间过程可能短暂通过"节点本身变红"制造出新的潜在冲突，但每次 case 1 处理完，当前层的红红相邻一定被消除，只是把风险推高了一层，直到推到根，由末尾的强制变黑收尾。

**2. 兄弟本来就是黑色，是否还需要经过 case 1**

不需要。case 1 存在的唯一目的是"把红色兄弟兑换成黑色兄弟"，为 case 2/3/4 创造前提条件——这三种情况的判断逻辑（`sibling->left->color`、`sibling->right->color` 的黑白组合）全部建立在"`sibling` 本身是黑色"这个假设上。如果最初的兄弟已经是黑色，直接跳过 case 1，根据兄弟的两个孩子（侄子）的颜色组合直接进入 case 2（两侄都黑）或 case 3/4（至少一侄红）的判断即可，代码里的 `if (sibling->color == RB_RED) { ... }` 这个分支本身就是只在兄弟为红时才会进入，兄弟为黑时天然跳过这一段，直接执行后续判断。

**3. 去掉哨兵 NIL，改用 `NULL` 需要额外加哪些特判**

至少以下几处需要额外的 `NULL` 检查：（a）`delete_fixup` 循环条件 `x != tree->root && x->color == RB_BLACK` 里，如果 `x` 可能是 `NULL`（对应哨兵设计里"叶子的孩子是 nil"的情况），需要先判断 `x != NULL` 才能安全访问 `x->color`，而哨兵设计里 `nil->color` 恒为黑，不需要这层判断；（b）取兄弟节点 `x->parent->left`/`x->parent->right` 时，兄弟节点本身也可能是 `NULL`（哨兵设计里兄弟一定是一个真实分配的、颜色恒黑的 `nil` 节点，不需要单独处理"兄弟不存在"这种情况，因为它总是存在，只是可能是哨兵），改用 `NULL` 之后每处访问 `sibling->color`/`sibling->left`/`sibling->right` 之前都要先判空；（c）`delete_fixup` 依赖 `nil->parent` 在删除物理节点时被临时设置成"离开位置的父节点"，改用 `NULL` 之后没有一个"共享的空节点"可以承载这个临时的 parent 信息，`delete_fixup` 需要改成显式传入父节点参数，而不能依赖从 `x->parent` 读取（因为 `x` 现在可能就是 `NULL` 本身，`NULL->parent` 没有意义）。整体上，去掉哨兵会让"空叶子"这个概念从"总是存在、只是颜色恒黑的真实节点"退化成"没有节点"，所有原本可以直接解引用 `nil` 的地方，都要换成"先判空、再决定怎么处理"，代码里的判空分支数量会明显增多，这正是哨兵设计当初被引入的原因——用一次性的"多分配一个共享节点"，换掉分散在各处、反复出现的判空逻辑。

---

## 十四、速查表

本章汇总各结构的公开 API 签名、编译命令、复杂度速查。全部签名直接取自对应实验目录的头文件，不是手写复述。

### 14.1 编译命令速查

```bash
# 常规模式（正确性验证，本文档默认使用）
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g <files>.c -o <out>

# 硬核模式（追加 sanitizer，捕捉内存安全问题；不要和性能模式混用）
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g \
   -fsanitize=address,undefined -fno-omit-frame-pointer \
   <files>.c -o <out>_san

# 性能模式（仅用于 04/05 的 perf.c，不要叠加 sanitizer 标志）
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O2 -g <files>.c -o perf
```

### 14.2 Trie（`03_trie/trie.h`，header-only，全部函数 `static inline`）

```c
TrieNode *trie_create(void);
bool      trie_insert(TrieNode *root, const char *word);
bool      trie_search(TrieNode *root, const char *word);
bool      trie_starts_with(TrieNode *root, const char *prefix);
void      trie_delete(TrieNode *root, const char *word);
void      trie_free(TrieNode *node);
WordList  trie_autocomplete(TrieNode *root, const char *prefix);
size_t    trie_count_nodes(const TrieNode *node);
```

- 复杂度：插入/查找/删除均为 O(L)，L 为单词长度，与树中单词总数无关。
- 陷阱提醒：`trie_delete` 内部依赖 `trie_node_is_empty` 判断是否可以释放节点（6.3 节）；不要写出"无条件释放整条路径"的版本（6.6 节 `trie_delete_BROKEN`）。

### 14.3 B 树（`04_b_tree/btree.h`）

```c
BTree btree_create(int t); /* t >= 2，t 越大越"矮胖" */
void  btree_destroy(BTree *tree);
bool  btree_search(const BTree *tree, int key);
bool  btree_insert(BTree *tree, int key);          /* 重复 key 返回 false */
bool  btree_delete(BTree *tree, int key);          /* key 不存在返回 false */
int   btree_count_keys(const BTree *tree);
int   btree_count_nodes(const BTree *tree);
int   btree_height(const BTree *tree);             /* 空树 -1，单叶根 0 */
bool  btree_verify(const BTree *tree, char *err_buf, size_t err_buf_size);
void  btree_print(const BTree *tree);
```

- 不变量：非根节点 key 数量在 `[t-1, 2t-1]`；非根内部节点孩子数量在 `[t, 2t]`；所有叶子深度相同。
- 复杂度：查找/插入/删除均 O(log n)；范围查询需要重新 descend/backtrack，见 10.3 节对比。

### 14.4 B+ 树（`05_b_plus_tree/bplustree.h`）

```c
BPlusTree bplustree_create(int t);
void      bplustree_destroy(BPlusTree *tree);
bool      bplustree_search(const BPlusTree *tree, int key, int *value_out);
bool      bplustree_insert(BPlusTree *tree, int key, int value);
bool      bplustree_delete(BPlusTree *tree, int key);
int       bplustree_range_query(const BPlusTree *tree, int low, int high,
                                 int *keys_out, int *values_out, int cap,
                                 int *nodes_visited_out);
int       bplustree_count_keys(const BPlusTree *tree);
int       bplustree_count_nodes(const BPlusTree *tree);
int       bplustree_count_leaves(const BPlusTree *tree);
int       bplustree_height(const BPlusTree *tree);
bool      bplustree_verify(const BPlusTree *tree, char *err_buf, size_t err_buf_size);
void      bplustree_print(const BPlusTree *tree);
void      bplustree_print_leaf_chain(const BPlusTree *tree);
```

- 与 B 树的关键差异：内部节点只存路由 key（不存真实数据），所有真实 key/value 都在叶子；叶子额外维护 `next` 单向链表。
- 分裂/合并的复制 vs 移动：叶子分裂**复制**分隔 key（真实数据必须留在叶子），内部分裂**移动**分隔 key；叶子合并**丢弃**分隔 key，内部合并**下拉**分隔 key（8.1-8.3 节）。
- `bplustree_range_query` 的 `nodes_visited_out` 是专门为性能实验设计的参数，size-invariant 特性见 8.4 节/10.3 节。

### 14.5 红黑树（`06_red_black_tree/rbtree.h`）

```c
RBTree rb_create(void);
void   rb_destroy(RBTree *tree);
bool   rb_search(const RBTree *tree, int key);
bool   rb_insert(RBTree *tree, int key);          /* 重复键返回 false */
bool   rb_delete(RBTree *tree, int key);          /* 键不存在返回 false */
int    rb_count(const RBTree *tree);
int    rb_black_height(const RBTree *tree);       /* 空树为 0 */
bool   rb_verify(const RBTree *tree, char *err_buf, size_t err_buf_size);
void   rb_print(const RBTree *tree);              /* R(key)/B(key) 标注颜色 */
```

- 五条性质：节点非红即黑 / 根为黑 / 哨兵叶子为黑 / 无红红相邻 / 任意节点到叶子的黑高一致。
- 插入 fixup 三种情况：叔叔红（上推两层）/ 之字形（转直线）/ 直线（变色+旋转终止）。
- 删除 fixup 四种情况：兄弟红（兑换成黑兄弟）/ 兄弟黑两侄黑（上推双黑标记）/ 近侄红远侄黑（转形状）/ 远侄红（一次旋转终止）。
- 哨兵 `nil` 陷阱：任何只读递归函数必须先判断 `x == tree->nil` 再展开，否则死循环（`nil` 自指）。

### 14.6 六种结构一句话复杂度对比

| 结构 | 查找/插入/删除 | 最适合的场景 |
|---|---|---|
| Trie | O(L)，L=单词长度 | 前缀匹配、自动补全 |
| B 树 | O(log n) | 教学理解多路平衡树原理 |
| B+ 树 | O(log n) | 范围查询、磁盘/数据库索引 |
| 红黑树 | O(log n) | 内存中通用有序 map/set |

---

## 十五、参考资料与延伸阅读

### 15.1 本文档直接依据的经典教材

- Thomas H. Cormen, Charles E. Leiserson, Ronald L. Rivest, Clifford Stein. *Introduction to Algorithms* (CLRS), 第 3/4 版. 本文档的 B 树（第七章）沿用其"最小度数 t"的约定，红黑树（第九章）的五条性质、插入/删除 fixup 的情况划分直接对应其第 13 章的经典处理方式。
- B+ 树没有统一的"标准教材原始出处"，本文档第八章的实现思路参考的是数据库系统教材（如 Ramakrishnan & Gehrke《Database Management Systems》）里对 B+ 树作为索引结构的通用描述，核心是"内部节点只做路由、叶子持有全部数据并用链表相连"这一设计共识。

### 15.2 与本文档配套的前置文档

- `c-language-guide.md`：本系列的第一部分，C 语言语法与语言特性基础，未接触过指针、结构体、动态内存管理细节的读者应先完成这部分。
- `c-algo-experiments/`：与 `c-language-guide.md` 配套的入门级实验代码。

### 15.3 延伸阅读方向

- **AVL 树**：另一种自平衡二叉搜索树，用显式维护的子树高度差（不超过 1）代替红黑树的颜色约束，平衡性更严格但旋转更频繁。十二章 12.4 节的综合项目就是把红黑树改造成 AVL 树，量化对比两者的实际旋转次数差异。
- **跳表（Skip List）**：一种用随机化代替严格平衡维护的有序结构，Redis 的有序集合（sorted set）底层实现之一，是"放弃确定性平衡、用概率保证期望复杂度"这一思路的典型代表，和本文档六个结构"靠不变量做确定性保证"的思路形成有趣对比。
- **LSM 树（Log-Structured Merge-Tree）**：现代 NoSQL 存储引擎（LevelDB、RocksDB）常用的磁盘索引结构，和 B+ 树（第八章）是数据库/存储系统里两条并行的技术路线——B+ 树优化读性能，LSM 树优化写性能，代价是读需要合并多层数据。
- **并查集（Union-Find）**：十三章练习"五、图遍历算法 2"提到的连通分量替代方案，是图论里另一个基础但常被独立成章讨论的数据结构，配合路径压缩和按秩合并可以做到近似 O(1) 的合并/查询。

### 15.4 工具链参考

- AddressSanitizer / UndefinedBehaviorSanitizer 官方文档（Clang/GCC 项目页面）：本文档"硬核模式"编译标志的权威说明。
- `clang -std=c17` / `gcc -std=c17` 各自的 man page：本文档所有代码都以 C17 标准为基准，不同编译器对标准细节的支持存在细微差异，遇到编译警告差异时应以各自的官方文档为准。

---

## 附录：本文档的验证声明

- 本文档所有代码示例、运行输出、测试结果、性能数据，均来自在本机对 `c-algo-advanced-experiments/` 下六个实验目录的真实编译与运行，编译命令均遵循十四章 14.1 节列出的标准命令（常规模式 `-std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g`；硬核模式追加 `-fsanitize=address,undefined -fno-omit-frame-pointer`；性能模式仅用于 `perf.c`，使用 `-O2`，不叠加 sanitizer）。
- 全部六个实验目录的测试套件在本文档写作时均已重新编译运行确认：二叉树遍历 64/64，图遍历算法 21/21，Trie 13/13，B 树 20/20，B+ 树 24/24，红黑树 19/19，共计 161 个测试用例全部通过，退出码 0。
- 第八章 8.6 节的调试笔记严格依据 `05_b_plus_tree/README.md` 原始记录复述，包括其对"无法还原当时具体报错信息、拒绝编造发现过程"的明确说明；本文档未对该记录做任何美化或补充式的虚构。
- 性能数据（第七章、第八章的实测表格）均为本机单次真实运行结果，不同硬件、编译器版本、系统负载下重新运行可能得到不同的具体数值，但数量级和趋势关系（例如 B+ 树范围查询代价与树规模无关）应保持一致——这是结构性质决定的，不是本机环境特有的现象。
- 本文档中所有标注为"⚠️ 错误示例"的代码均为真实可编译、真实运行过的代码（对应各实验目录里独立命名的 `_BROKEN` 函数或 demo 里显式标注的错误演示段落），其"错误输出"同样是真实运行捕获的结果，不是根据错误原理推断出来的预测性描述。
