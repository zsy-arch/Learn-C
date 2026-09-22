# 03_trie —— 前缀树：为什么删除单词不能直接 free？

## 本章问题

Trie（也叫前缀树、字典树）的插入和查找都很直观：一个字符一层，走到头打个标记。
真正让新手卡住的是**删除**：

1. 如果 `"car"` 和 `"card"` 共享 `c -> a -> r` 这条路径，删除 `"car"` 的时候，
   能不能把这条路径上的节点直接 `free` 掉？—— 不能，因为 `"card"` 还在用它们。
2. 那怎么知道"这个节点还有没有被别的单词用"？
3. 如果两次插入同一个单词，会不会在树里留下两份？删除一次之后，另一份还在吗？

这一章用真实编译运行的代码回答这三个问题，并且专门做了一个**错误实现**，
让你亲眼看到"不检查共享"的删除会把无关单词一起删没。

## 学习目标

1. 说清楚 Trie 节点的"共享"是怎么发生的：多个单词的共同前缀只占一份节点。
2. 独立写出正确的递归删除：回溯时用返回值告诉父节点"我这个子树空了，你可以把我也删了"。
3. 理解 Trie 的存在性是**按字符串内容**，不是**按插入次数**——插入两次等于插入一次，删除一次就彻底没了。
4. 会用 `assert` 和节点计数（`trie_count_nodes`）验证"删除该释放的节点释放了，该保留的一个没少"。
5. 分清各个工具到底验证了什么：ASan/UBSan 查越界和未定义行为，泄漏要靠 `leaks`
   （arm64 macOS 上 LeakSanitizer 不受支持），并且知道"工具没报错"和"工具检查过了"是两件事。

## 数据结构定义

```c
#define ALPHABET_SIZE 26
#define TRIE_MAX_WORD_LEN 255   /* 单词长度上限，不含结尾 '\0' */

typedef struct TrieNode {
    struct TrieNode *children[ALPHABET_SIZE]; /* 26 个指针，对应 a~z */
    bool is_word;                              /* 这个节点是否是某个单词的结尾 */
} TrieNode;
```

`TRIE_MAX_WORD_LEN` 不是凭空定的容量上限，它是为了修一个真实缺陷才引入的，
详见下面"真实缺陷：能插进去，但枚举不出来"一节。

**设计取舍**：用固定大小的 26 元素数组存子节点，查子节点是 `O(1)` 数组下标访问，
代价是每个节点固定占用 `26 * 8 = 208` 字节（64 位指针）的子节点数组，
不管这个节点实际有几个孩子。算上 `is_word` 和对齐填充，
实测 `sizeof(TrieNode) == 216`，而 macOS 的 malloc 按 16 字节粒度向上取整，
所以**一个节点的实际堆开销是 224 字节**——一个 3 字母的单词要 3 个节点，
将近 700 字节，这是 Trie 用空间换时间的真实价码。如果字符集很大（比如要支持 Unicode），
工业界通常会换成哈希表或有序数组存子节点，牺牲一点查找速度换取空间。
本章只讨论小写英文字母，固定数组是最直观的教学选择。

## 文件

| 文件 | 作用 |
|---|---|
| `trie.h` | 核心实现：插入/查找/前缀/删除/释放/自动补全，`demo.c` 和 `tests.c` 共用 |
| `demo.c` | 7 个分节演示，包括一个⚠️故意错误的删除实现 |
| `tests.c` | 15 组测试用例 |

## 编译与运行

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g demo.c -o demo
./demo

cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g tests.c -o tests
./tests

# sanitizer 版本
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer demo.c -o demo_san
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer tests.c -o tests_san
```

两次编译都是**零警告**（`-Werror` 下都能过），真实终端记录：

```text
$ cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g demo.c -o demo
$ echo "exit=$?"
exit=0

$ cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g tests.c -o tests
$ echo "exit=$?"
exit=0
```

## 1. 基本插入 / 查找 / 前缀查找

真实运行输出（`./demo` 第一节）：

```text
========== 1. 基本插入 / 查找 / 前缀查找 ==========
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

注意 `search("ca")` 是 `false` 但 `starts_with("ca")` 是 `true`——
`"ca"` 只是路径上的一个节点，不是任何单词的结尾（`is_word == false`），
但它确实是 `"cat"`/`"car"`/`"card"`/`"care"` 共同的前缀。
`search` 和 `starts_with` 检查的是完全不同的东西：

```c
static inline bool trie_search(TrieNode *root, const char *word) {
    TrieNode *node = trie_find_node(root, word);
    return node != NULL && node->is_word;   /* 多一个 is_word 检查 */
}

static inline bool trie_starts_with(TrieNode *root, const char *prefix) {
    ...
    return trie_find_node(root, prefix) != NULL;  /* 路径存在就行，不管 is_word */
}
```

`starts_with("")` 是 `true`：空串是每个字符串的前缀，所以按定义它匹配任何 Trie——
包括**一个单词都没插入的空 Trie**（`test_empty_trie` 里就断言了这一条）。
代码里判的是"`root` 指针非 NULL"，不是"树里有单词"。这是很多人会忽略的边界情况。

## 2. 前缀共享：cat / car / card / care

```text
========== 2. 前缀共享：cat / car / card / care ==========
  插入前缀相关的四个单词后，Trie 结构大致如下：

        root
         |
         c
         |
         a
        / \
       t   r  [is_word=true, 对应 "car"]
       |       \
       |        +---d  [is_word=true, 对应 "card"]
       |        |
       |        +---e  [is_word=true, 对应 "care"]
       |
  [is_word=true, 对应 "cat"，t 是叶子，没有孩子]

  "ca" 这条路径被 4 个单词共享，只占用 2 个节点（c、a），
  而不是每个单词各自占用一份 —— 这就是 Trie 省空间的地方。
```

`tests.c` 里的 `test_shared_prefix_nodes` 把这个图变成了断言：
四个单词总长度是 `3+3+4+4=14` 个字符，但 `trie_count_nodes(root)` 只有 `7`
（`root, c, a, t, r, d, e`），因为 `c -> a` 这两层被四个单词共用。

## 3. 删除：只清标记，不乱释放节点

这是本章的核心。递归删除函数的返回值约定是：

> "我这个子树现在是不是完全空的、可以被父节点 `free` 掉？"

```c
static inline bool trie_delete_helper(TrieNode *node, const char *word, size_t depth) {
    if (node == NULL) {
        return false; /* 单词本来就不存在，什么都不做 */
    }

    if (word[depth] == '\0') {
        if (!node->is_word) {
            return false; /* 删除不存在的单词：这是前缀节点，不是完整单词 */
        }
        node->is_word = false; /* 只清标记，不动 children */
        return trie_node_is_empty(node);
    }

    int idx = char_to_index(word[depth]);
    TrieNode *child = node->children[idx];
    if (trie_delete_helper(child, word, depth + 1)) {
        free(child);
        node->children[idx] = NULL;
    }

    return trie_node_is_empty(node); /* 我自己现在空不空，交给上一层判断 */
}
```

真实运行输出：

```text
========== 3. 删除 "car"：card / care / cat 必须不受影响 ==========
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

节点总数**删除前后完全一样**（都是 7）——这一行数字就是"删除没有误伤共享节点"最直接的证据。
`starts_with("car")` 删除后仍是 `true`，因为 `"card"` 和 `"care"` 都以 `"car"` 开头，
这条路径依然实实在在地存在于树里，只是它自己不再是一个完整单词的终点。

对比一个**会真正释放节点**的例子（`"dog"` 不和任何其他单词共享路径）：

```text
========== 4. 删除会真正释放节点的情况：独立单词 "dog" ==========
  插入 dog, cat 后节点总数 = 7
  删除 dog 后节点总数     = 4   (d/o/g 三个节点全部被物理释放)
  search("dog")        = false
  search("cat")        = true
```

这里节点数从 `7` 掉到 `4`，因为 `d/o/g` 三个节点在删除 `"dog"` 后既不是任何单词的结尾，
也没有任何孩子（`trie_node_is_empty` 返回 `true`），递归回溯时被逐层 `free`。
`"cat"` 走的是完全不同的路径（`c -> a -> t`），不受任何影响。

## 4. 删除不存在的单词：安全忽略

```text
========== 5. 删除不存在的单词：不能崩溃，不能误删 ==========
  Trie 中只有 "cat"
  删除 "dog" / "ca" / "cats" 之后（这三个都不是 Trie 里的完整单词）：
  search("cat")        = true
  -> 三次无效删除全部安全忽略，"cat" 完好无损
```

三种"不存在"分别对应三条不同的提前返回路径：

- `"dog"`：走到 `d` 这一步 `node->children['d'-'a']` 就是 `NULL`，`trie_delete_helper` 传入 `NULL` 直接返回 `false`。
- `"ca"`：路径存在（`c -> a` 节点都在），但走到头时 `node->is_word == false`（`"ca"` 只是前缀），也返回 `false`。
- `"cats"`：`"cat"` 存在，但 `"cats"` 会在 `t` 之后继续找 `s`，而 `t` 节点没有 `s` 孩子，同样提前返回。

## 5. 应用：自动补全

```text
========== 6. 应用：自动补全 ==========
  词库: cat, car, card, care, careful, dog, do, door
  autocomplete("ca") = {car, card, care, careful, cat}  (5 个)
  autocomplete("car") = {car, card, care, careful}  (4 个)
  autocomplete("do") = {do, dog, door}  (3 个)
  autocomplete("z") = {}  (0 个)
  autocomplete("") = {car, card, care, careful, cat, do, dog, door}  (8 个)
```

实现思路：先沿着前缀走到对应节点（复用 `trie_find_node`），
然后从这个节点开始做一次 DFS，凡是 `is_word == true` 的节点就把当前拼出来的字符串收进结果列表。
这就是输入法、搜索框"猜你想输入"功能的基本原理。

## 6. ⚠️ 错误示例：无条件往上 free 整条路径

新手写删除时最容易出现的直觉是："找到单词结尾，把这一路走过的节点全部 `free` 不就行了？"
下面这个 `trie_delete_BROKEN`（只存在于 `demo.c` 里，**不是** `trie.h` 的正式 API）就是这个直觉的代码化：

```c
/* ⚠️ 危险示例，仅用于教学，禁止在正式代码中使用 */
static void trie_delete_BROKEN(TrieNode *root, const char *word) {
    if (root == NULL || !is_valid_word(word)) return;  /* 和正式 API 一致的入口校验 */

    TrieNode *path[TRIE_MAX_WORD_LEN];   /* 容量由同一个常量推导，不是魔法数字 64 */
    int idx[TRIE_MAX_WORD_LEN];
    int depth = 0;

    TrieNode *cur = root;
    for (const char *p = word; *p != '\0'; p++) {
        int i = char_to_index(*p);
        if (cur->children[i] == NULL) return;
        path[depth] = cur;
        idx[depth] = i;
        cur = cur->children[i];
        depth++;
    }
    if (!cur->is_word) return;

    /* BUG：不检查任何共享情况，从最深处开始无条件往上 free 整条路径 */
    free(cur);
    for (int i = depth - 1; i >= 0; i--) {
        path[i]->children[idx[i]] = NULL;
        if (i > 0) free(path[i]);
    }
}
```

前两行（入口校验 + 容量常量）和本节的教学要点无关，是后来补上的——原来那版有两处
和"共享前缀"完全无关的缺陷，留着只会让读者在抄这段代码时踩到别的坑：

| 原来的写法 | 触发条件 | 实测结果 |
|---|---|---|
| `path[64]` / `idx[64]`，不检查 `depth` | 对一个长度 > 64 的已存在单词调用 | UBSan: `index 64 out of bounds for type 'TrieNode *[64]'`；ASan: `stack-buffer-overflow`, `WRITE of size 8` |
| 不校验字符就 `char_to_index` | 传入含大写字母的单词，如 `"Car"` | `'C' - 'a' == -30` → `children[-30]`；UBSan: `index -30 out of bounds`；ASan: `BUS` |

`is_valid_word` 允许 255 字符的单词，而这里的数组只有 64 项——**两个上限对不上**，
和下面"真实缺陷：能插进去，但枚举不出来"讲的是同一类问题，只是这次会直接踩坏栈。

真实运行输出：

```text
========== 7. ⚠️ 错误示例：无条件往上 free 整条路径 ==========
  插入 car, card（共享 c-a-r 路径）
  删除前:   search("card")       = true
  调用 trie_delete_BROKEN(root, "car") ...
  删除后:   search("card")       = false
  -> "card" 本该完好无损，但因为 c/a/r 节点被无条件 free，
     root 到这条路径的指针也被一路清成了 NULL，
     结果是 "card" 也从 Trie 里"消失"了 —— 这就是没做
     "这一层是否还被其他单词占用"检查的后果。
  节点总数 = 1（只剩 root），而 "card" 的 d 节点既没被 free、
     也再没有任何指针指向它 —— 它泄漏了。
  注意 root 本身并没有变成"损坏的树"：它的 26 个孩子全是 NULL，
     是一棵合法的空 Trie，继续调用任何 API 都是安全的（已用
     ASan/UBSan 验证）。这个错误实现丢掉的是数据，不是内存安全。
```

主要是**逻辑正确性 bug**：`trie_delete_BROKEN` 把仍被 `"card"` 使用的 `c/a/r`
三个节点全部释放，导致 `"card"` 这个完全没被要求删除的单词也从树里消失了。
这正是为什么正确实现必须让每一层自己判断"我删完这个孩子之后，我自己是不是也空了"，
而不能"一旦找到目标就无脑往上删"。

但它造成的损害到底是什么，得说准确，这里很容易说错（本文档早先的版本就说错了）：

| 说法 | 成立吗 | 依据 |
|---|---|---|
| 有 use-after-free | ❌ 不成立 | 循环里先把 `path[i]->children[idx[i]]` 置 NULL **再** `free(path[i])`，而 free 顺序是从深到浅，每次写的都是一个还没被 free 的节点 |
| 有 double free | ❌ 不成立 | 每个指针只被 free 一次 |
| **有内存泄漏** | ✅ **成立** | `free(cur)` 时 `cur` 还挂着 `"card"` 的 `d` 节点，父节点一消失，那棵子树就再也无人指向 |
| root 变成了结构不一致的树，继续调用 API 是 UB | ❌ 不成立 | 实测 `root->children` 全部为 NULL，是一棵**合法的空 Trie** |

所以"这不是内存安全 bug"的说法是不对的——**泄漏本身就是一种内存安全问题**。
实测（`leaks --atExit -- ./demo`，不带 sanitizer 的普通二进制）：

```text
Process 56689: 2 leaks for 448 total leaked bytes.
STACK OF 1 INSTANCE OF 'ROOT LEAK: <calloc in trie_node_create>':
    1 (224 bytes) ROOT LEAK: <calloc in trie_node_create 0x7ab5004c40> [224]
STACK OF 1 INSTANCE OF 'ROOT LEAK: <calloc in trie_node_create>':
    1 (224 bytes) ROOT LEAK: <calloc in trie_node_create 0x7ab5004000> [224]
```

两个 224 字节的节点：一个是无法补救的 `d` 节点，另一个是 **`root` 自己**——
因为早先这里"故意不调用 `trie_free(root)`"，理由写的是"树已损坏、继续操作是 UB"。
探针程序（ASan+UBSan 下运行）证明这个理由是错的：

```text
after : nodes=1 card=0
root has 0 non-NULL children  -> root is a VALID EMPTY trie
search("car")=0 starts_with("c")=0 insert("new")=1
trie_free(root) after BROKEN: completed without crash
```

`trie_delete_BROKEN` 返回后，`root` 是一棵完全合法的空 Trie：查找、前缀、
重新插入、删除、`trie_free` 全都正常，ASan/UBSan 零报告。不调用 `trie_free`
不但没有避免什么风险，反而让 `root` 自己也跟着泄漏，把 1 个泄漏节点变成了 2 个。
现在代码正常调用 `trie_free(root)`，泄漏降到 **1 leak / 224 bytes**，
剩下那一个就是这个错误实现真正的、无法补救的代价：

```text
Process 64224: 1 leak for 224 total leaked bytes.
```

（`224` 而不是 `sizeof(TrieNode) == 216`，是因为 macOS 的 malloc 按 16 字节粒度
向上取整分配。）

## 机制剖析：为什么插入两次不会留下两份

Trie 的存在性状态挂在 `is_word` 这一个 `bool` 上，不是计数器。
插入同一个单词两次，第二次插入只是把已经存在的路径又走了一遍，
最后把已经是 `true` 的 `is_word` 再设成 `true`，什么都没变：

```c
static inline bool trie_insert(TrieNode *root, const char *word) {
    TrieNode *cur = root;
    for (const char *p = word; *p != '\0'; p++) {
        int idx = char_to_index(*p);
        if (cur->children[idx] == NULL) {
            cur->children[idx] = trie_node_create(); /* 已存在就不会新建 */
        }
        cur = cur->children[idx];
    }
    cur->is_word = true; /* 幂等：设置多少次结果一样 */
    return true;
}
```

**这个"幂等"特性直接决定了删除的行为**：调用一次 `trie_delete(root, "x")`，
不管 `"x"` 之前被插入过一次还是一百次，都会把它彻底从树里移除，
不存在"删一次还剩九十九次"的计数语义——这一点在写随机测试时特别容易踩坑
（见下面"常见错误"一节），因为它和很多人对"计数容器"的直觉不一样。

## 常见错误：把 Trie 当成计数容器来测试

写 `tests.c` 里的大规模随机测试（`test_large_random_insert_delete`）时，
第一版测试代码假设："如果单词 `"jnfb"` 在数组的第 504 位和第 4679 位都出现过，
只删除第 504 位对应的那次插入，第 4679 位那次应该还'活着'"。

第一次运行的真实输出：

```text
[PASS] test_empty_trie
...
[PASS] test_invalid_inputs_rejected
    删除后状态不一致: "jnfb" 期望 存在 实际 不存在
[FAIL] test_large_random_insert_delete
[PASS] test_free_null_and_empty_safe

========== 汇总 ==========
通过: 12, 失败: 1, 总计: 13
```

**排查过程**：先怀疑是 `trie_delete_helper` 的 bug，写了一个最小复现：

```c
TrieNode *root = trie_create();
trie_insert(root, "jnfb");
trie_insert(root, "jnfb");           /* 插入两次 */
trie_delete(root, "jnfb");           /* 只删一次 */
printf("%d\n", trie_search(root, "jnfb")); /* 输出 0 */
```

结果是 `0`（查不到），这其实是**正确行为**：Trie 没有"插入次数"的概念，
`is_word` 只有 `true`/`false` 两种状态，插入两次和插入一次对树的影响完全一样，
删除一次就彻底清空了这个状态。

真正的问题在测试代码：它把数组下标当成了"这个单词的第 N 份拷贝"，
但 Trie 里从来不存在"拷贝"，只有"这个字符串存在"或"不存在"两种状态。
**修复方式**是让期望值的计算方式匹配 Trie 的真实语义——
一个 distinct 字符串最终是否存在，取决于它是否在任何一次 `trie_delete` 调用中被点过名，
和它在数组里出现了多少次、哪些下标出现，没有关系：

```c
/* 修复后：只要 words[i] 曾经在任意一个偶数下标（会被 delete 的下标）出现过，
 * 这个 distinct 字符串就该彻底不存在，不管它是否也出现在别的下标 */
bool ever_targeted_by_delete = false;
for (int j = 0; j < RANDOM_WORD_COUNT; j += 2) {
    if (strcmp(words[j], words[i]) == 0) {
        ever_targeted_by_delete = true;
        break;
    }
}
bool should_exist = !ever_targeted_by_delete;
```

修复后重新编译运行，当时的 13 项全部通过（后来新增了 2 项边界测试，
现在是 15 项，见下方"测试"一节的真实输出）。

这个故事的教训：**测试代码本身的假设也需要被验证**，尤其是在写"对拍"式测试时，
先想清楚被测数据结构的真实语义（这里是"按内容去重的存在性状态"），
再决定预期值怎么算，不要想当然地套用别的数据结构（比如带计数的 multiset）的直觉。

## 常见错误之二：随机测试只覆盖了一半的输入

上面那个 bug 在预期值的算法里，这一个更隐蔽：它在**随机数生成器**里，
症状是测试看起来跑了 5000 个样本，实际只覆盖了一半的长度。

`gen_random_word` 原来是这么写的：

```c
static void gen_random_word(char *buf, unsigned *state) {
    int len = 3 + (int)(*state % (RANDOM_WORD_MAXLEN - 2));   /* 先取 len */
    for (int i = 0; i < len; i++) {
        *state = (*state) * 1103515245u + 12345u;             /* 再 advance */
        buf[i] = (char)('a' + (*state >> 16) % 26);
    }
    buf[len] = '\0';
}
```

长度取自**进入函数时**的 `*state`，而且取的是最低几位。把 5000 个词的长度打成直方图：

```text
length histogram (len: count):
   3: 1
   4: 1629
   6: 1660
   8: 1710
```

**长度 5 和 7 一次都没出现过。** 原因是这个 LCG 的乘数 `1103515245` 和增量 `12345`
都是奇数，所以 `state` 的奇偶性每走一步就翻转一次，于是长度和奇偶性锁成了一个自洽的循环：

| 进入时 state | `3 + state % 6` | 循环走的步数 | 出去时 state |
|---|---|---|---|
| 偶 | 3 / 5 / 7（**奇数**） | 奇数步 | → 奇 |
| 奇 | 4 / 6 / 8（**偶数**） | 偶数步 | → **仍然是奇** |

一旦 `state` 变成奇数就永远是奇数，长度就永远只能落在 `{4, 6, 8}`。
种子 `42` 是偶数，所以只有第 0 个词拿到了长度 3，之后立刻锁死——
那个孤零零的 `3: 1` 就是这么来的。

两处都要改：

```c
*state = (*state) * 1103515245u + 12345u;                        /* 先 advance */
int len = 3 + (int)((*state >> 16) % (RANDOM_WORD_MAXLEN - 2));  /* 再取高位 */
```

- **先 advance 再取长度**，打破"长度决定步数、步数决定下一个长度"的自锁。
- **取高位（`>> 16`）而不是低位**：LCG 的低位周期极短（最低位周期只有 2），
  拿 `state % 6` 这种低位运算当随机数是经典陷阱。生成字符的那一行本来就是
  `(*state >> 16) % 26`，长度这一行只是漏了同样的处理。

修正后的直方图，以及这个测试真正想考的那个场景的密度：

```text
FIXED length histogram:
   3: 823
   4: 819
   5: 860
   6: 809
   7: 814
   8: 875
entries at BOTH even and odd indices: 32      （修正前是 6）
  example: "lnj" at index 1048 (even) and 73 (odd)
```

注意最后一行：**"同一个单词同时出现在偶数和奇数下标"正是上一节那个 bug 的触发条件**。
修正生成器之后这种情况从 6 个涨到 32 个，也就是说上一节的回归保护反而变强了。
（上一节引用的 `"jnfb"` 在 504 / 4679 下标那组数字来自**修正前**的生成器，
保留原样是为了忠实记录当时的排查过程；现在的生成器会给出不同的单词和下标，
但"存在这种词"这个前提更成立了。）

教训：**随机测试的"随机"本身也是被测代码的一部分**。5000 个样本和
"5000 个覆盖了输入空间的样本"是两件事，跑一次直方图就能知道是哪一种——
而这件事不会有任何测试失败来提醒你，因为生成器"能跑"，测试也"全绿"。

## 真实缺陷：能插进去，但枚举不出来

上面那个是测试代码的 bug。这一个是**实现本身**的缺陷，而且更隐蔽：
它不崩溃、不报错、不触发任何 sanitizer，只是安静地少给你几个结果。

`trie_autocomplete` 用一个固定大小的栈缓冲区拼接单词：

```c
char buf[256];                      /* 修复前 */
```

而 `trie_collect` 往下递归时，缓冲区放不下就直接跳过这棵子树：

```c
for (int i = 0; i < ALPHABET_SIZE; i++) {
    if (node->children[i] != NULL && depth + 1 < buf_size) {   /* 修复前 */
        buf[depth] = (char)('a' + i);
        trie_collect(node->children[i], buf, depth + 1, buf_size, out);
    }
}
```

问题在于**插入路径当时对长度毫无限制**。`is_valid_word` 只检查了"非空"和
"全是小写字母"，没有检查长度。于是两条路径的隐含上限对不上：能插进去的单词，
不一定能被枚举出来。

实测（探针程序，ASan+UBSan 下运行）：

```text
len=254  insert=1 search=1 autocomplete("")=1
len=255  insert=1 search=1 autocomplete("")=1
len=256  insert=1 search=1 autocomplete("")=0   <== 静默丢失
len=300  insert=1 search=1 autocomplete("")=0   <== 静默丢失
```

边界恰好在 255/256：`depth + 1 < 256` 这个条件让递归最深只能走到 `depth == 255`，
所以长度 255 的单词是最后一个能被拼出来的。256 字符的单词**确实在树里**
（`trie_search` 返回 1），但 `trie_autocomplete` 永远看不到它。

更糟的是 caller 完全无法察觉：

```text
插入 3 个词, trie_search 全部命中: cat=1 car=1 long=1
autocomplete("") 只返回 2 个: car cat
```

返回的 `WordList` 里没有任何字段能区分"确实只有 2 个匹配"和
"有 3 个但丢了 1 个"。这是最坏的一类缺陷：**错误的结果长得和正确的结果一模一样**。
ASan 抓不到它（没有越界，`depth + 1 < buf_size` 这个条件本身是安全的），
测试也抓不到（原来 13 组测试里最长的单词是 `"application"`，11 个字符，
离 255 这个边界差了 20 多倍）。

**修复方式**不是把缓冲区改大——那只是把边界挪到另一个位置，缺陷依旧存在。
真正的修法是让两条路径共用同一个常量，把"可插入"和"可枚举"绑成同一个条件：

```c
#define TRIE_MAX_WORD_LEN 255

/* 入口处拦住：is_valid_word 里加长度检查 */
size_t len = 0;
for (const char *p = word; *p != '\0'; p++) {
    if (*p < 'a' || *p > 'z') return false;
    if (++len > TRIE_MAX_WORD_LEN) return false;
}

/* 缓冲区由同一个常量推导，不可能再对不上 */
char buf[TRIE_MAX_WORD_LEN + 1];
```

这样超长单词在 `trie_insert` 就被拒绝（返回 `false`，和"含大写字母"
"空字符串"一样走同一条既有的拒绝路径），树里根本不会出现枚举不出来的单词。
因为 `trie_delete` 也走 `is_valid_word`，删除语义自动保持一致。

另外，`trie_collect` 是公开函数，`buf_size` 由 caller 传，
所以"缓冲区不够"这件事在直接调用时依然可能发生。给 `WordList`
加了一个 `truncated` 标志，让它从静默状态变成可检测状态：

```c
typedef struct WordList {
    char **words;
    size_t count;
    size_t capacity;
    bool truncated;    /* 结果是否不完整 */
} WordList;
```

```c
if (depth + 1 < buf_size) {
    buf[depth] = (char)('a' + i);
    trie_collect(node->children[i], buf, depth + 1, buf_size, out);
} else {
    out->truncated = true;   /* 丢了东西，至少说出来 */
}
```

效果：

```text
词库 {ab, abcdefgh}, buf_size=4 -> count=1, truncated=true
词库 {ab, abcdefgh}, buf_size=9 -> count=2, truncated=false
```

**回归测试**（`test_word_length_boundary` 和 `test_collect_truncation_is_reported`）
验证过是有效的：把长度检查去掉，前者失败；把 `truncated = true` 那行去掉，
后者失败；两者互不重叠，各自守住自己那一半修复。

教训：一个数据结构如果有两条路径（写入和枚举）各自持有一个**隐含的**容量上限，
这两个上限迟早会对不上。把它提成一个具名常量、让两边都从它推导，
是唯一能让"树里的每个单词都能被枚举出来"真正成为不变量的做法。

## 测试

15 组测试用例覆盖：空 Trie、单个单词、重复插入、前缀共享节点计数、
删除不存在的单词（三种不同的"不存在"）、删除会释放节点的独立单词、
删除共享前缀中的一个单词、删除到整棵树清空、`starts_with` 的各种边界、
自动补全结果集合比对（不依赖返回顺序）、非法输入拒绝、
5000 个固定种子伪随机单词（长度 3~8 均匀分布，见"常见错误之二"）的
插入+验证+删除+再验证、`trie_free` 对 `NULL`/空树的安全性、
单词长度边界（恰好 `TRIE_MAX_WORD_LEN` 必须可插入且可枚举，超一个字符必须被拒绝）、
`trie_collect` 的截断必须置位 `truncated`（且不能误报）。

最后两组是为"真实缺陷：能插进去，但枚举不出来"一节补的回归测试。

真实运行输出：

```text
========== Trie 测试套件 ==========
[PASS] test_empty_trie
[PASS] test_single_word
[PASS] test_duplicate_insert
[PASS] test_shared_prefix_nodes
[PASS] test_delete_nonexistent
[PASS] test_delete_leaf_frees_nodes
[PASS] test_delete_shared_prefix_word
[PASS] test_delete_all_words_empties_trie
[PASS] test_starts_with_variants
[PASS] test_autocomplete_matches_expected_set
[PASS] test_invalid_inputs_rejected
[PASS] test_large_random_insert_delete
[PASS] test_free_null_and_empty_safe
[PASS] test_word_length_boundary
[PASS] test_collect_truncation_is_reported

========== 汇总 ==========
通过: 15, 失败: 0, 总计: 15
```

（这是修复了前面两节提到的缺陷之后的最终结果，退出码 `0`。）

## 内存验证：ASan/UBSan 查越界与 UB，`leaks` 查泄漏

先说一句本章最容易搞错的事实：**LeakSanitizer 在 arm64 macOS 上根本跑不起来**。

sanitizer 编译（`-fsanitize=address,undefined`）运行 `tests_san`，
15 项全部通过，输出里没有任何 `ERROR`/`SUMMARY`/`runtime error` 相关的行：

```bash
$ ./tests_san 2>&1 | grep -i "ERROR\|SUMMARY\|leak\|runtime error"
NO_SANITIZER_ISSUES_FOUND
```

很容易据此以为"LeakSanitizer 默认开着并且报告了 0 泄漏"。**这个结论是错的。**
显式要求它开启就能看到真相：

```text
$ ASAN_OPTIONS=detect_leaks=1 ./tests_san
==98578==AddressSanitizer: detect_leaks is not supported on this platform.
$ echo "exit=$?"
exit=134
```

LSan 在 `arm64-apple-darwin` 上**不受支持**，请求开启会直接 abort（退出码 134）。
默认不开启不是"检查过了没发现问题"，而是"这项检查从来没运行过"——
上面那个干净的 grep 结果只能证明 ASan 的越界检查和 UBSan 的未定义行为检查通过了，
它对泄漏**一个字都没说**。

这是一类很典型的误判：**把"工具没报错"当成"工具检查过了"**。
两者的区别只有在你显式要求这项检查、看它是否真的执行时才会暴露出来。

所以泄漏结论必须由 macOS 自带的 `leaks` 提供（`leaks` 和 ASan 不能同时用，
因为 ASan 替换了 `malloc`，所以要用**没有加 sanitizer** 的普通二进制）：

```text
$ MallocStackLogging=1 leaks --atExit -- ./tests
========== 汇总 ==========
通过: 15, 失败: 0, 总计: 15
...
leaks Report Version: 4.0, multi-line stacks
Process 8440: 189 nodes malloced for 31 KB
Process 8440: 0 leaks for 0 total leaked bytes.
```

**0 泄漏**的结论成立，包括那个分配、验证、再释放了几千个动态字符串和
Trie 节点的大规模随机测试——但它来自**一个**工具（`leaks`），不是两个。
在 Linux 上跑同样的代码才能拿到 LSan 的第二份独立确认。

`demo_san`（sanitizer 版本的 `demo`）的运行命令里带了一个关闭 leak 检测的开关：

```bash
ASAN_OPTIONS=detect_leaks=0 ./demo_san
```

需要说清楚的是：在本机（arm64 macOS）上这个开关是**空操作**，因为 LSan 本来就没开。
保留它是为了**可移植性**——第 7 节的错误示例会泄漏一个节点（`"card"` 的 `d` 节点，
被 `trie_delete_BROKEN` 变成了无人指向的孤儿，没有任何办法补救），
在 LSan 可用的平台（比如 x86-64 Linux）上这会被报成泄漏而让 `demo_san` 非零退出，
这个开关才真正起作用。

注意这处泄漏是**错误示例本身的产物，是它要展示的代价之一**，不是"故意不释放"：
`demo.c` 现在会正常调用 `trie_free(root)`。和 `trie.h` 的正式 API 无关——
`tests.c` 才是验证正式 API 是否有泄漏的地方，而它的结果，如上所示（由 `leaks` 给出），
是干净的。`./demo` 的对应数字是 `1 leak for 224 total leaked bytes`，
就是那一个孤儿节点。

## 最佳实践

1. Trie 删除必须用**递归回溯 + 返回值**告诉父节点"我空了，你可以释放我"，
   不能"找到目标就无脑往上删"。
2. 判断一个节点是否可以被释放，检查两件事：`is_word == false` 且**没有任何孩子**——
   两个条件缺一个都不能删，否则会误删仍被使用的前缀节点或误删仍是完整单词的节点。
3. Trie 的存在性是**按字符串内容**的状态（`true`/`false`），不是计数器；
   插入 N 次等于插入 1 次，删除 1 次就彻底清空，这一点在设计测试时尤其要小心。
4. 用 `trie_count_nodes` 这种"结构性质的数字"（而不仅仅是查找结果）来验证删除的副作用范围，
   比只验证 `search` 结果更能发现"多删了"或"少删了"的 bug。
5. 泄漏检测用 `leaks`（普通二进制），内存越界/UB 用 ASan/UBSan（sanitizer 二进制），两者不要混用同一个二进制。
   在 arm64 macOS 上 LeakSanitizer 不受支持，`leaks` 是唯一的泄漏检测手段——
   用 `ASAN_OPTIONS=detect_leaks=1` 显式要求一次，就能知道某项检查到底有没有在运行，
   不要把"输出里没有 leak 字样"当成"泄漏检查通过了"。
6. 一个数据结构如果在两条路径上各自持有一个**隐含的**容量上限（比如写入不限长、
   枚举用固定缓冲区），这两个上限迟早会对不上。提成具名常量、让两边都从它推导。
7. 一个会丢数据的 API，至少要让 caller 有办法知道结果不完整（比如 `truncated` 标志）。
   静默返回一个偏小的结果集，是比报错更难排查的失败方式。
8. 说一个 bug 的危害时，要说**能被工具证实的那一条**，不要顺口升级成更吓人的那一条。
   第 7 节的错误实现，真实危害是"丢数据 + 泄漏一个节点"，不是 "use-after-free"
   也不是"树结构损坏、后续操作是 UB"。前者 `leaks` 一跑就能看到，
   后者用探针 + ASan/UBSan 一验就被推翻。**把危害说重了和说轻了一样有害**：
   它会让人基于错误的模型做决定——比如因为"树已损坏"而不敢 `trie_free(root)`，
   结果凭空多泄漏一个节点。

## 练习

1. 给 `TrieNode` 加一个 `int count`，统计以这个节点结尾的单词被插入过多少次
   （也就是把"存在性状态"换成"计数状态"），重新实现删除：只有 `count` 减到 `0`
   才真正清除 `is_word`。这样"插入两次、删除一次还剩一次"是不是就能实现了？
2. `trie_autocomplete` 现在会返回**所有**匹配前缀的单词。如果词库有几十万单词，
   只想要前 10 个"最可能"的结果（比如按插入频率排序），需要在 `TrieNode` 里加什么字段？
   DFS 收集的顺序要怎么改？
3. 当前实现只支持小写英文字母（`ALPHABET_SIZE = 26`）。如果要支持数字和小写字母
   （36 种字符），`char_to_index` 和 `ALPHABET_SIZE` 该怎么改？如果要支持任意 Unicode
   字符呢？（提示：这时候固定数组就不再合适了，通常换成每个节点内部用一个小的
   有序数组或哈希表存"字符 -> 子节点"的映射，这也是为什么很多生产级 Trie 实现
   管子节点结构叫 "sparse children"。）
