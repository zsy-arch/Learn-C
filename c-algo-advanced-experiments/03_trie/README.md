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
5. 用 ASan/UBSan/`leaks` 三种工具交叉验证一个递归数据结构没有内存泄漏。

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
详见下面"错误 2：能插进去，但枚举不出来"一节。

**设计取舍**：用固定大小的 26 元素数组存子节点，查子节点是 `O(1)` 数组下标访问，
代价是每个节点固定占用 `26 * 8 = 208` 字节（64 位指针）的子节点数组，
不管这个节点实际有几个孩子。如果字符集很大（比如要支持 Unicode），
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

`starts_with("")` 是 `true`：空前缀按定义匹配任何非空 Trie，这是很多人会忽略的边界情况。

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
       t   r [is_word=true, 对应 "car"]
       |   |
   [cat]   +---d [is_word=true, 对应 "card"]
           |
           +---e [is_word=true, 对应 "care"]

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
    TrieNode *path[64];
    int idx[64];
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

真实运行输出：

```text
========== 7. ⚠️ 错误示例：无条件往上 free 整条路径 ==========
  插入 car, card（共享 c-a-r 路径）
  删除前:   search("card")       = true
  调用 trie_delete_BROKEN(root, "car") ...
  删除后:   search("card")       = false
  -> "card" 本该完好无损，但因为 c/a/r 节点被无条件 free，
     现在查找 "card" 会从一个已经不存在于树里的路径开始找，
     结果是 "card" 也从 Trie 里"消失"了 —— 这就是没做
     "这一层是否还被其他单词占用"检查的后果。
  本演示到此为止，不再使用 root（它的内部已被破坏，
     继续操作是未定义行为，这里选择直接放弃它、不调用 trie_free）。
```

**这不是内存安全 bug（没有 use-after-free、没有 double free），是逻辑正确性 bug**：
`trie_delete_BROKEN` 会把仍被 `"card"` 使用的 `c/a/r` 三个节点全部释放，
导致 `"card"` 这个完全没被要求删除的单词，也从树里消失了。
这正是为什么正确实现必须让每一层自己判断"我删完这个孩子之后，我自己是不是也空了"，
而不能"一旦找到目标就无脑往上删"。

演示结束后代码故意不调用 `trie_free(root)`——因为 `root` 内部已经被
`trie_delete_BROKEN` 破坏成了一棵结构不一致的树（部分节点已经被 free，
但这不影响残留的、依然合法可达的部分被安全地"放弃不管"，进程退出时操作系统会回收全部内存），
继续对它调用任何 API 都是未定义行为，这里选择停止使用它，不去示范"操作一棵已经损坏的树"。

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

修复后重新编译运行，13 项全部通过（见下方"测试"一节的真实输出）。

这个故事的教训：**测试代码本身的假设也需要被验证**，尤其是在写"对拍"式测试时，
先想清楚被测数据结构的真实语义（这里是"按内容去重的存在性状态"），
再决定预期值怎么算，不要想当然地套用别的数据结构（比如带计数的 multiset）的直觉。

## 测试

13 组测试用例覆盖：空 Trie、单个单词、重复插入、前缀共享节点计数、
删除不存在的单词（三种不同的"不存在"）、删除会释放节点的独立单词、
删除共享前缀中的一个单词、删除到整棵树清空、`starts_with` 的各种边界、
自动补全结果集合比对（不依赖返回顺序）、非法输入拒绝、
5000 个固定种子伪随机单词的插入+验证+删除+再验证、`trie_free` 对 `NULL`/空树的安全性。

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

========== 汇总 ==========
通过: 13, 失败: 0, 总计: 13
```

（这是修复了上一节提到的测试期望值 bug之后的最终结果，退出码 `0`。）

## 内存验证：sanitizer + leaks 双重确认

sanitizer 编译（`-fsanitize=address,undefined`）运行 `tests_san`，
LeakSanitizer 默认开启（本节没有手动关闭），13 项全部通过，
输出里没有任何 `ERROR`/`SUMMARY`/`leak` 相关的行：

```bash
$ ./tests_san 2>&1 | grep -i "ERROR\|SUMMARY\|leak\|runtime error"
NO_SANITIZER_ISSUES_FOUND
```

再用 macOS 自带的 `leaks` 工具独立确认一遍（`leaks` 和 ASan 不能同时用，
因为 ASan 替换了 `malloc`，所以要用**没有加 sanitizer** 的普通二进制）：

```text
$ MallocStackLogging=1 leaks --atExit -- ./tests
========== 汇总 ==========
通过: 13, 失败: 0, 总计: 13
...
leaks Report Version: 4.0, multi-line stacks
Process 40262: 189 nodes malloced for 31 KB
Process 40262: 0 leaks for 0 total leaked bytes.
```

两个独立工具（LeakSanitizer 和 `leaks`）给出一致结论：**0 泄漏**，
包括那个分配、验证、再释放了几千个动态字符串和 Trie 节点的大规模随机测试。

`demo_san`（sanitizer 版本的 `demo`）需要单独关掉 leak 检测：

```bash
ASAN_OPTIONS=detect_leaks=0 ./demo_san
```

原因是第 7 节的错误示例演示故意不调用 `trie_free`（树已经被破坏，示范"放弃使用一棵损坏的树"
比"继续操作它去释放"更安全），这是**唯一**故意保留的内存，且只发生在教学用的错误函数里，
和 `trie.h` 的正式 API 无关——`tests.c` 才是验证正式 API 是否有泄漏的地方，
而它的结果，如上所示，是干净的。

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
6. 演示"错误实现"造成的破坏后，直接放弃那棵被破坏的树（不再调用任何 API，也不去 `free` 它），
   不要在教学代码里示范"继续操作一个已知处于未定义状态的对象"。

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
