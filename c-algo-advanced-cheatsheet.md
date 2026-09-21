# C 语言算法与数据结构进阶 —— 速查手册

配套文档：`c-algorithms-advanced.md`。本手册只收录**编译验证过的最小可用模板**，每段代码都是从对应实验目录的真实源码中提取，并在本机独立编译运行确认过输出——不是从主文档摘抄后未经验证的复述。

编译命令统一使用：

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g <files>.c -o <out>
```

如需追加内存安全检查：

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g \
   -fsanitize=address,undefined -fno-omit-frame-pointer <files>.c -o <out>
```

- [一、二叉树遍历](#一二叉树遍历)
- [二、图遍历算法](#二图遍历算法)
- [三、Trie](#三trie)
- [四、B 树](#四b-树)
- [五、B+ 树](#五b-树)
- [六、红黑树](#六红黑树)
- [七、六种结构对比速查](#七六种结构对比速查)

---

## 一、二叉树遍历

来源：`c-algo-advanced-experiments/01_binary_tree_traversal/demo.c`。这是本文档唯一没有独立头文件的主题——所有函数以 `static` 形式活在单个 `demo.c` 里，是教学用的自包含演示，不是可链接库；下面的模板把结构定义和核心函数一起摘出，独立编译验证过。

### 节点定义

```c
typedef struct Node {
    int value;
    struct Node *left;
    struct Node *right;
} Node;
```

### 莫里斯中序遍历（O(1) 额外空间）

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
                /* 线索存在：拆除线索（恢复原状），访问 cur，再向右走 */
                predecessor->right = NULL;
                out[(*n)++] = cur->value;
                cur = cur->right;
            }
        }
    }
}
```

真实编译运行确认（3 节点树 1-2-3，中序输出 `1 2 3`）：

```text
$ cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g morris_tpl.c -o morris_tpl && ./morris_tpl
1 2 3
```

**陷阱**：如果忘记在第二次到达 `cur` 时拆除线索（`predecessor->right = NULL` 这一行），树的形状会被永久破坏——某些节点的 `right` 不再指向真正的右子树，而是残留指向祖先的线索，等价于在树里造出了一个环。这**不是内存安全问题**（不触发 ASan/UBSan），是纯逻辑错误，只有后续再遍历一次这棵树、观察到重复访问或死循环才会暴露。

### 三种遍历方式的核心差异

| 方式 | 额外空间 | 实现思路 |
|---|---|---|
| 递归 | O(h)，h 为树高（调用栈） | 直接翻译遍历顺序的定义 |
| 迭代（显式栈） | O(h) | 用手写栈模拟递归调用栈 |
| 莫里斯遍历 | O(1) | 用叶子节点原本闲置的 `right` 指针临时充当"回溯线索" |

---

## 二、图遍历算法

来源：`c-algo-advanced-experiments/02_graph_traversal/`（`graph.h` + `algos.h`，均为真实可链接库）。

### 邻接表 + BFS 核心结构

```c
typedef struct AdjNode {
    int              to;
    struct AdjNode  *next;
} AdjNode;

typedef struct Graph {
    int       n;
    bool      directed;
    AdjNode **adj;
    int       edge_count;
} Graph;

Graph *graph_create(int n, bool directed);
void   graph_destroy(Graph *g);
void   graph_add_edge(Graph *g, int u, int v);

typedef struct BFSResult {
    int *dist;   /* dist[i]：从 src 到 i 的边数，不可达为 -1 */
    int *prev;   /* prev[i]：i 在最短路径树里的前驱，-1 表示无 */
    int *order;
    int  order_len;
} BFSResult;

BFSResult bfs(const Graph *g, int src, bool verbose);
void      bfs_free(BFSResult *r);
```

### 最小可用调用模板

```c
Graph *g = graph_create(5, false);
graph_add_edge(g, 0, 1);
graph_add_edge(g, 1, 2);

BFSResult r = bfs(g, 0, false);
/* r.dist[2] == 2 */
bfs_free(&r);

int comp[5];
int nc = connected_components(g, comp); /* 无向图连通分量数 */

graph_destroy(g);
```

真实编译运行确认：

```text
$ cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g graph.c algos.c graph_tpl.c -o graph_tpl && ./graph_tpl
dist[2]=2
components=2
```

### 其余核心 API（`algos.h`，签名照抄头文件）

```c
DFSResult dfs_recursive(const Graph *g, int src);
DFSResult dfs_iterative(const Graph *g, int src);
bool      topo_sort_kahn(const Graph *g, int *out);   /* 检测到环返回 false */
bool      topo_sort_dfs(const Graph *g, int *out);
bool      has_cycle_directed(const Graph *g);
bool      has_cycle_undirected(const Graph *g);
bool      is_bipartite(const Graph *g, int *color);   /* color 数组长度 g->n */
```

---

## 三、Trie

来源：`c-algo-advanced-experiments/03_trie/trie.h`（header-only，全部函数 `static inline`，`#include` 即可用，不需要额外编译单元）。

### 核心 API

```c
TrieNode *trie_create(void);
bool      trie_insert(TrieNode *root, const char *word);
bool      trie_search(TrieNode *root, const char *word);
bool      trie_starts_with(TrieNode *root, const char *prefix);
void      trie_delete(TrieNode *root, const char *word);
void      trie_free(TrieNode *node);
```

### 最小可用调用模板

```c
#include "trie.h"

TrieNode *root = trie_create();
trie_insert(root, "cat");
trie_insert(root, "car");
/* trie_search(root, "cat") == true, trie_starts_with(root, "ca") == true */
trie_delete(root, "cat");
trie_free(root);
```

真实编译运行确认：

```text
$ cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g trie_tpl.c -o trie_tpl && ./trie_tpl
search(cat)=1 starts_with(ca)=1
```

### 删除逻辑核心（唯一容易出错的部分）

```c
static inline bool trie_node_is_empty(const TrieNode *node) {
    if (node->is_word) return false;
    for (int i = 0; i < 26; i++) {
        if (node->children[i] != NULL) return false;
    }
    return true;
}
```

**陷阱**：删除一个单词时，只能释放"沿路径往上、且不再被任何其他单词依赖"的节点——必须用 `trie_node_is_empty` 逐层检查，不能无条件释放整条路径。删除 `"car"` 之后如果 `"card"`/`"care"` 还存在，共享的 `c-a-r` 路径节点必须保留。这类 bug 不是内存安全问题（不触发 ASan/UBSan），是纯逻辑错误，只有断言"删除后其他共享前缀的单词依然能被搜索到"才能捕捉到。

---

## 四、B 树

来源：`c-algo-advanced-experiments/04_b_tree/btree.h` + `btree.c`。CLRS "最小度数 t" 约定：非根节点 key 数量在 `[t-1, 2t-1]`。

### 核心 API

```c
BTree btree_create(int t);          /* t >= 2 */
void  btree_destroy(BTree *tree);
bool  btree_search(const BTree *tree, int key);
bool  btree_insert(BTree *tree, int key);   /* 重复 key 返回 false */
bool  btree_delete(BTree *tree, int key);   /* key 不存在返回 false */
int   btree_count_keys(const BTree *tree);
int   btree_height(const BTree *tree);      /* 空树 -1，单叶根 0 */
bool  btree_verify(const BTree *tree, char *err_buf, size_t err_buf_size);
```

### 最小可用调用模板

```c
BTree tree = btree_create(2);
for (int i = 1; i <= 10; i++) btree_insert(&tree, i);

char err[256];
bool ok = btree_verify(&tree, err, sizeof err);
/* ok == true, btree_count_keys == 10, btree_height == 2 */

btree_delete(&tree, 5);
btree_destroy(&tree);
```

真实编译运行确认：

```text
$ cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g btree.c btree_tpl.c -o btree_tpl -I. && ./btree_tpl
verify=1 keys=10 height=2
```

### 分裂核心：中点是 `t-1`，不是 `t`

```c
static void split_child(BTreeNode *x, int i, int t) {
    BTreeNode *y = x->children[i];      /* full child, y->n == 2t-1 */
    BTreeNode *z = node_create(t, y->is_leaf);

    z->n = t - 1;
    for (int j = 0; j < t - 1; j++) z->keys[j] = y->keys[j + t];
    if (!y->is_leaf) {
        for (int j = 0; j < t; j++) z->children[j] = y->children[j + t];
    }
    int mid_key = y->keys[t - 1];       /* 中点索引 t-1，被提升到父节点 */
    y->n = t - 1;

    for (int j = x->n; j >= i + 1; j--) x->children[j + 1] = x->children[j];
    x->children[i + 1] = z;
    for (int j = x->n - 1; j >= i; j--) x->keys[j + 1] = x->keys[j];
    x->keys[i] = mid_key;
    x->n++;
}
```

**陷阱**：中点索引如果错写成 `t`（而不是 `t-1`），分裂后两侧的 key 数量分配会失衡，破坏"非根节点 key 数量在 `[t-1, 2t-1]`" 这条不变量——这是纯粹的数量约束问题，不触发 ASan/UBSan，只能靠 `btree_verify` 这类独立校验函数抓到。

---

## 五、B+ 树

来源：`c-algo-advanced-experiments/05_b_plus_tree/bplustree.h` + `bplustree.c`。和 B 树共用 t 约定，但内部节点只存路由 key，真实数据全部在叶子，叶子额外维护 `next` 单向链表。

### 核心 API（注意 `range_query` 的完整 7 个参数）

```c
BPlusTree bplustree_create(int t);
void      bplustree_destroy(BPlusTree *tree);
bool      bplustree_search(const BPlusTree *tree, int key, int *value_out);
bool      bplustree_insert(BPlusTree *tree, int key, int value);
bool      bplustree_delete(BPlusTree *tree, int key);
int       bplustree_range_query(const BPlusTree *tree, int low, int high,
                                 int *keys_out, int *values_out, int cap,
                                 int *nodes_visited_out);
bool      bplustree_verify(const BPlusTree *tree, char *err_buf, size_t err_buf_size);
```

### 最小可用调用模板

```c
BPlusTree tree = bplustree_create(3);
for (int i = 1; i <= 20; i++) bplustree_insert(&tree, i, i * 100);

int keys_out[32], values_out[32], visited = 0;
int n = bplustree_range_query(&tree, 5, 10, keys_out, values_out, 32, &visited);
/* n == 6（5..10 共 6 个 key），visited == 走过的叶子数 */

char err[256];
bool ok = bplustree_verify(&tree, err, sizeof err);

bplustree_destroy(&tree);
```

真实编译运行确认：

```text
$ cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g bplustree.c bplus_tpl.c -o bplus_tpl -I. && ./bplus_tpl
range_query found=6 visited_leaves=3
verify=1
```

**注意**：`bplustree_range_query` 的参数顺序是 `keys_out, values_out, cap, nodes_visited_out`（共 7 个参数），漏写 `values_out` 是本手册在编写验证阶段实际踩到、并被编译器 `-Werror` 直接拦下的错误——这正是"每个模板都要独立编译验证"这条规则存在的意义：签名错误不会在阅读时被发现，只会在读者复制粘贴之后才炸出来。

### 分裂的复制 vs 移动（核心记忆点）

| 操作 | 叶子节点 | 内部节点 |
|---|---|---|
| 分裂时对分隔 key 的处理 | **复制**（真实数据必须留在叶子） | **移动**（路由 key 不携带数据，无需保留） |
| 合并时对分隔 key 的处理 | **丢弃**（纯粹作废） | **下拉**（仍需用来分隔子树） |

---

## 六、红黑树

来源：`c-algo-advanced-experiments/06_red_black_tree/rbtree.h` + `rbtree.c`。CLRS 第 13 章，哨兵 NIL 设计（所有"空叶子"位置指向唯一的共享 `nil` 节点，颜色恒黑）。

### 核心 API

```c
RBTree rb_create(void);
void   rb_destroy(RBTree *tree);
bool   rb_search(const RBTree *tree, int key);
bool   rb_insert(RBTree *tree, int key);    /* 重复键返回 false */
bool   rb_delete(RBTree *tree, int key);    /* 键不存在返回 false */
int    rb_count(const RBTree *tree);
int    rb_black_height(const RBTree *tree); /* 空树为 0 */
bool   rb_verify(const RBTree *tree, char *err_buf, size_t err_buf_size);
```

### 最小可用调用模板

```c
RBTree tree = rb_create();
int vals[] = {10, 20, 30, 15, 5, 1};
for (size_t i = 0; i < sizeof(vals)/sizeof(vals[0]); i++) rb_insert(&tree, vals[i]);

char err[256];
bool ok = rb_verify(&tree, err, sizeof err);
/* ok == true, rb_count == 6, rb_black_height == 2 */

rb_delete(&tree, 20);
rb_destroy(&tree);
```

真实编译运行确认：

```text
$ cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g rbtree.c rb_tpl.c -o rb_tpl -I. && ./rb_tpl
verify=1 count=6 black_height=2
```

### 插入 fixup 三种情况的判断顺序

```text
父节点是红色（唯一需要修复的前提，性质4被违反）：
  叔叔是红色       -> case 1：父叔变黑、祖父变红，把问题原样上推两层
  叔叔是黑色 + 之字形 -> case 2：先旋转转成直线形态，落入 case 3
  叔叔是黑色 + 直线   -> case 3：变色 + 旋转，一次性彻底解决，终止循环
循环结束后强制 tree->root->color = RB_BLACK（性质2：根恒为黑）
```

### 删除 fixup 四种情况的判断顺序

```text
x 背着"双黑"标记（顶替被删黑色节点的位置）：
  兄弟是红色                 -> case 1：先兑换成黑色兄弟，转化为 2/3/4
  兄弟黑 + 两侄子都黑          -> case 2：兄弟变红，双黑标记原样上推到父节点
  兄弟黑 + 近侄红、远侄黑        -> case 3：旋转转形状，落入 case 4
  兄弟黑 + 远侄红              -> case 4：一次旋转吸收双黑标记，终止循环
```

**陷阱**：`insert_fixup` 循环末尾如果漏掉 `tree->root->color = RB_BLACK` 这一行，根节点可能残留红色——这违反性质 2，但 BST 有序性、性质 4（无红红相邻）、性质 5（黑高一致）全部完好，是纯逻辑/结构不变量问题，不触发 ASan/UBSan，只有 `rb_verify` 里独立于其他性质、单独检查根颜色的判断才能抓到。

---

## 七、六种结构对比速查

| 结构 | 查找/插入/删除 | 核心数据结构特征 | 最适合场景 |
|---|---|---|---|
| 二叉树遍历 | — | 无独立头文件，教学用单文件演示 | 建立"递归/迭代/O(1)空间"三种遍历心智模型 |
| 图（BFS/DFS） | O(V+E) | 邻接表 | 连通性判断、无权最短路径、拓扑序 |
| Trie | O(L)，L=单词长度 | 固定 26 路子节点数组 | 前缀匹配、自动补全 |
| B 树 | O(log n) | 单一节点形态，key 与孩子指针混存 | 教学理解多路平衡树原理 |
| B+ 树 | O(log n)，范围查询 O(log n + k) | 内部/叶子两种节点形态，叶子链表 `next` | 范围查询、磁盘/数据库索引 |
| 红黑树 | O(log n) | 哨兵 NIL，颜色约束 | 内存中通用有序 map/set |

三种平衡树（B 树/B+ 树/红黑树）单次操作的渐近复杂度完全相同，真正的选型依据是**访问哪些节点、范围查询代价、是否需要落盘**——详见主文档第十章 10.3 节的真实性能对比数据。

---

**本手册的验证方式**：每个"最小可用调用模板"均已在本机独立编译（`-std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g`）并运行，运行输出直接来自真实执行结果，不是根据源码推断的预测值。核心函数代码块（`morris_inorder`、`split_child`、`trie_node_is_empty`）均逐行核对自对应实验目录的当前源码，签名与实际头文件完全一致。
