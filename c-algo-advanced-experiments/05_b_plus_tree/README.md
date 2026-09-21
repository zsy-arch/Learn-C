# 05_b_plus_tree —— B+ 树：内部节点只管路由，数据全在叶子

## 本章问题

1. B 树和 B+ 树区别在哪？为什么几乎所有数据库、文件系统的索引选的是 B+ 树而不是上一章的 B 树？
2. B 树的内部节点存着真实数据，删除/合并的时候要考虑"这个 key 到底能不能直接搬走"；B+ 树的内部节点如果只存路由用的 key 副本，删除逻辑会因此变得更简单还是更复杂？
3. "范围查询"这件事，B 树只能靠中序遍历、每次都要在内部节点和叶子之间反复上下；B+ 树凭什么能做到"只扫一条链表"？这条链表要付出什么代价才能维护正确？

## 学习目标

- 理解 B+ 树与 B 树唯一但决定性的结构差异：内部节点**只存路由 key，不存真实数据**；所有真实的 key-value 对**只存在叶子节点**里，且叶子节点之间用 `next` 指针串成一条全局有序的链表。
- 亲手实现并看清楚"插入时提升 key"这件事在 B+ 树里分裂两种情形下的**不对称性**：叶子分裂时，提升上去的 key 是"复制"，它依然作为真实数据留在叶子里；内部节点分裂时，提升上去的 key 是"移动"，两侧都不会再保留副本——这个不对称性是本章唯一的核心教学点。
- 亲手实现删除的全部分支，尤其是叶子合并时必须修复 `next` 指针（否则链表会断），以及内部节点合并时要把分隔 key "拉下来"（跟叶子合并的"直接丢弃"正好相反）。
- 写一个 `bplustree_verify()`，除了 B 树检查的那几条结构不变量之外，还要独立校验**叶子链表的完整性**——这是 B+ 树独有、B 树完全不需要考虑的一条属性。
- 用真实测量的"访问了多少个叶子"数据，验证"范围查询只跟结果集大小相关，跟树的总大小无关"这句话到底有多真。

## 数据结构定义

沿用上一章 B 树的"最小度数 `t`"约定（CLRS 参数化方式），方便直接对照，但节点内部的字段完全不同——内部节点和叶子节点存的东西根本不是一回事：

```c
typedef struct BPlusNode {
    bool is_leaf;
    int n;                 /* 当前 key 数量 */
    int *keys;              /* 容量 2t-1；叶子里是真实 key，内部节点里是路由 key（副本） */
    union {
        struct BPlusNode **children; /* 内部节点用：容量 2t */
        int *values;                  /* 叶子用：容量 2t-1，跟 keys 一一对应 */
    } u;
    struct BPlusNode *next; /* 只有叶子会用：指向链表里的下一个叶子，最右叶子为 NULL */
} BPlusNode;

typedef struct {
    BPlusNode *root; /* 空树时为 NULL */
    int t;            /* 最小度数，t >= 2，与 B 树同一套参数化方式 */
} BPlusTree;
```

对最小度数为 `t` 的 B+ 树（`t >= 2`）：

- 非 root 叶子的 key-value 对数量在 `[t-1, 2t-1]` 之间。
- 非 root 内部节点的路由 key 数量在 `[t-1, 2t-1]` 之间，孩子数量在 `[t, 2t]` 之间。
- root 作为叶子时，key 数可以少到 0（空树）；root 作为内部节点时，路由 key 数至少是 1（不能是 0——0 个路由 key 意味着只有 1 个孩子，这种情况应该已经被"root 收缩"处理成直接用那个孩子取代 root 了）。
- 所有叶子深度相同。
- **内部节点的 key 是"路由用"的副本，不是真实数据**：第 `i` 个孩子子树里，所有 key 都满足 `keys[i-1] <= key < keys[i]`——注意左边界是**闭区间**（`<=`），因为路由 key 本身就等于右边子树的最小值，它在右边子树里也真实存在；右边界是**开区间**（`<`），跟 B 树的"两侧都开区间"不同（B 树的 key 从不在多层重复出现，不需要考虑"这个 key 同时是谁的下界又是谁的上界"这种情况）。
- **叶子链表**：所有叶子按 key 升序、用 `next` 串成一条单向链表，从树的最左叶子开始，到最右叶子的 `next == NULL` 结束。这条链表包含且仅包含树里全部的真实数据，是 B 树完全没有的东西。

**为什么"内部节点只存路由 key"这件事本身值得单独做一章？** B 树的内部节点既要承担"路由"又要承担"存数据"两个职责，这导致：删除内部节点上的 key 时必须先用前驱/后继把它替换掉（不能直接删，否则子树结构就断了）；范围查询必须做中序遍历，因为数据分散在所有层级里。B+ 树把"路由"和"存数据"这两个职责彻底分开之后：内部节点的分裂/合并/借操作变成纯粹的"指针和路由 key 搬运"，不需要考虑"这个 key 是不是数据、能不能被替换掉"；而所有真实数据都在叶子、叶子又串成一条链表，范围查询就退化成"定位起点 + 链表线性扫描"，不用再触碰任何内部节点。代价是：同样的 key 会在树里出现两次（一次作为叶子里的真实数据，一次作为某个内部节点的路由副本），需要多一点存储空间；但换来的是数据库、文件系统索引里最看重的两件事——范围查询效率和实现简单性。

## 文件

| 文件 | 作用 |
|---|---|
| `bplustree.h` | 节点/树结构定义与全部公开函数声明 |
| `bplustree.c` | 插入（含叶子分裂/内部节点分裂）、删除（含借/合并/链表修复/root 收缩）、`bplustree_range_query()`、`bplustree_verify()`、`bplustree_print()` 的实现 |
| `demo.c` | 10 个小节的演示：叶子分裂特写（复制）、内部/root 分裂特写（移动）、叶子借位、叶子合并与链表修复证明、内部节点合并与分隔 key 下移、root 收缩、范围查询与真实叶子访问计数 |
| `tests.c` | 25 个测试用例，覆盖空树/单 key/4 种 `t` 值/叶子与内部节点的分裂借合并/root 收缩/range query 边界情形/1000+ 随机压力测试 |
| `perf.c` | 独立的性能测量：插入耗时（4 种规模 × 4 种 `t`）、range query 叶子访问数随规模的变化、删除耗时 |
| `Makefile` | `make all/run/test/san/bench/clean` |

## 编译与运行

```bash
$ cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g bplustree.c demo.c -o demo
$ echo $?
0
$ cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g bplustree.c tests.c -o tests
$ echo $?
0
```

两次编译都是零警告、退出码 0。也可以用 `make all` 一次性构建两个二进制，`make run`/`make test` 分别运行，`make san` 用 AddressSanitizer + UndefinedBehaviorSanitizer 构建并运行两者，`make bench` 编译并运行 `perf.c`（`-O2`，不含 sanitizer）。

## B 树 vs B+ 树：到底差在哪

| | B 树（04_b_tree） | B+ 树（本章） |
|---|---|---|
| 内部节点存什么 | 真实 key（可能还带 value） | 只存路由 key 的**副本**，不存真实 value |
| 真实数据在哪 | 分散在所有层级 | 只在叶子层 |
| 同一个 key 会不会出现两次 | 不会，每个 key 全树唯一 | 会：一次在叶子（真实），一次在祖先的路由位置（副本） |
| 叶子之间有没有链接 | 没有 | 有，`next` 串成全局有序单链表 |
| 删除内部节点上的 key | 必须找前驱/后继替换（不能直接删） | 不存在这个问题——路由 key 只是副本，合并/借的时候可以直接丢弃或搬运，不用替换 |
| 范围查询 `[low, high]` | 中序遍历，遍历路径在内部节点和叶子之间反复切换 | 一次下降定位到 `low` 所在叶子，之后**只**沿 `next` 线性扫描，不再触碰任何内部节点 |
| 路由 key 的边界语义 | 两侧都是开区间（`keys[i-1] < key < keys[i]`） | 左闭右开（`keys[i-1] <= key < keys[i]`），因为路由 key 本身在右子树里真实存在 |
| 叶子合并时分隔 key 怎么处理 | 不适用（B 树没有"叶子"这个特殊层级） | **直接丢弃**——它只是副本，反正原来的真实 key 还留在合并后的叶子里 |
| 内部节点合并时分隔 key 怎么处理 | 拉下来跟两个孩子的 key 拼在一起 | 同样要**拉下来**——这一步跟 B 树是一样的，不一样的只有"叶子合并" |
| 空间代价 | 无冗余 | 每个非叶子路由 key 都多占一份存储 |
| 适合谁 | 需要频繁随机点查、不太需要范围扫描的场景 | 数据库索引、文件系统——范围查询和顺序扫描是常态 |

## 1. Leaf 分裂特写：提升的 key 是"复制"

`demo.c` 的第 1 节。root 一开始就是一个满叶子（`t=2`，容量 `2t-1=3`）：

```
[10:1000 20:2000 30:3000] (leaf)
```

插入 40 之后：

```
[30]
  [10:1000 20:2000] (leaf)
  [30:3000 40:4000] (leaf)
  leaf chain: [10,20] -> [30,40] -> NULL
```

root 的路由 key 是 30——它是从原来叶子 `[10 20 30]` 分裂时**复制**上去的：key 30 依然作为真实数据留在右边叶子 `[30:3000 40:4000]` 里，root 里的 30 只是一份指路用的副本，并没有把 30 从叶子里搬走。这就是叶子分裂的核心行为：`leaf_split()` 把原叶子的后半部分（含中间 key）搬到新叶子，然后把"新叶子的最小 key"复制一份塞进父节点作为路由 key。

## 2~3. Internal 分裂特写（root 分裂）：提升的 key 是"移动"，两边都不留副本

继续插入到 90（`demo.c` 第 2 节），root 内部节点会先长到 `[30 50 70]`、4 个叶子孩子、已经满员（`n=3=2t-1`）。再插入 100（第 3 节）触发 root 分裂：

```
插入 100 之前：
[30 50 70]
  [10:1000 20:2000] (leaf)
  [30:3000 40:4000] (leaf)
  [50:5000 60:6000] (leaf)
  [70:7000 80:8000 90:9000] (leaf)

插入 100 之后：
[50]
  [30]
    [10:1000 20:2000] (leaf)
    [30:3000 40:4000] (leaf)
  [70 90]
    [50:5000 60:6000] (leaf)
    [70:7000 80:8000] (leaf)
    [90:9000 100:10000] (leaf)
  leaf chain: [10,20] -> [30,40] -> [50,60] -> [70,80] -> [90,100] -> NULL
```

新 root 只有 1 个 key：50。这个 50 原本是旧 root `[30 50 70]` 的中间路由 key，分裂时被**移动**到新 root：左孩子变成 `[30]`，右孩子变成 `[70 90]`，两边都不再含有 50。这跟第 1 节的叶子分裂正好相反——internal 节点从来不存真实数据，它的每一个 key 都只是路由用的副本，分裂时没有理由在原地留一份，直接搬走即可，也让 `internal_split()` 比 `leaf_split()` 简单一步（不需要"复制后中间 key 依然留在某一侧"这种特殊处理）。

**这正是本章的核心教学点**：同一件事"分裂时提升中间 key"，在叶子层是复制（因为叶子存的是真实数据，提升上去的只是路由用的影子），在内部节点层是移动（因为内部节点存的本来就是影子，分裂只是把影子重新分配）。

## 4~7. 删除：不下溢 → 借位 → 合并（并证明链表被修好了）

继续插入到 130，得到一棵两层内部节点、6 个叶子、共 13 个 key 的树（`demo.c` 第 4 节），后续删除演示都基于这棵树。

**不触发调整**：删除 130 后，叶子 `[110 120]` 还有 2 个 key，超过下限 `t-1=1`，什么都不用动（第 5 节）。

**借位（borrow）**：继续删除 120、再删除 110（第 6 节）。删 120 后叶子变成 `[110]`，正好落在下限，还不算下溢；再删 110 时该叶子变空，真正下溢，于是向左邻居 `[90 100]` 借一个 key（`[90 100]` 有 2 个 key，超过下限 1，是合法的出借方）：

```
[70 90 100]
  [50:5000 60:6000] (leaf)
  [70:7000 80:8000] (leaf)
  [90:9000] (leaf)
  [100:10000] (leaf)
  leaf chain: [10,20] -> [30,40] -> [50,60] -> [70,80] -> [90] -> [100] -> NULL
```

parent 的路由 key 从 110 变成了 100（新右子树最小值 100 的复制），跟借位前 `[90 100]` 变成 `[90]` / 新叶子多了 100 是一致的。

**合并（merge）+ 链表修复证明**：删除 100（第 7 节）。此时叶子 `[100]` 只有 1 个 key，左邻居 `[90]` 也只有 1 个（没有多余的可借），两边都在下限，只能合并。合并时路由 key 100 直接被**丢弃**——它本来就只是一份复制品，不像 B 树合并那样需要把 key 拉下来：

```
合并之前的链表：
  leaf chain: [10,20] -> [30,40] -> [50,60] -> [70,80] -> [90] -> [100] -> NULL

合并之后的链表（[100] 从链上消失，[90] 的 next 直接指向下一个存活的叶子）：
  leaf chain: [10,20] -> [30,40] -> [50,60] -> [70,80] -> [90] -> NULL
```

这段真实打印出的完整链表就是"合并没有弄断链表"最直接的证据：`leaf_merge()` 在把右叶子的 key/value 搬进左叶子、释放右叶子之前，必须先执行 `left->next = right->next`，否则链表会在这个节点处断掉，后续所有 range query 只能扫到断点为止就丢数据——这是 B+ 树实现里最容易漏掉、也最难在小规模测试里被发现的一类 bug（因为断链之后，单纯的"key 是否存在"检查完全测不出来，只有专门检查链表完整性的测试才能抓到）。

## 8. Internal 节点合并：分隔 key 被"拉下来"

叶子层的合并是"丢弃"分隔 key，但**内部节点**层的合并要"拉下来"，这跟叶子层正好相反。`demo.c` 第 8 节用一棵新建的 `t=2`、key 为 0..60（step 2）的树持续删到只剩 3 层结构时触发：

```
删除 42：叶子 [40] 变空并与邻居合并，导致它的父节点 [42]（一个
internal 节点）只剩 1 个孩子、0 个 routing key，下溢。它跟兄弟 [24]
合并：root 的 routing key 32（分隔 [24] 子树和 [40] 子树的那个 key）
被“拉下来”塞进合并后的节点，因为它是唯一还在区分两边孙子层的东西。
```

合并前 root 是 `[16 32]`，合并后变成 `[16]`（root 从 2 个 routing key 减到 1 个，但还没到"只剩 1 个孩子"的收缩条件）。为什么内部节点合并必须拉下分隔 key，而叶子合并可以直接丢弃？因为叶子的路由 key 在祖先节点里永远只是一份**多余的**副本（真实数据仍在叶子里，丢了副本不影响任何数据完整性），但两个内部节点之间的分隔 key 是**唯一**用来区分"哪些孙子属于左边、哪些属于右边"的信息——合并之后如果丢掉它，两侧孙子的路由信息就会丢失一层，树就不再满足"routing key 是子树下界"这条不变量了。这一步的合并逻辑跟 B 树的内部节点合并完全一致，B+ 树和 B 树在这里没有差异，差异只发生在叶子层。

## 9. Root 收缩：合并到只剩 1 个孩子时，直接被那个孩子取代

持续删除到整棵树只剩很少 key 时（第 9 节），最终会删到空树：

```
height=-1（期望 -1），root==NULL: 1
```

跟 B 树完全一样：当 root 是内部节点、合并之后只剩 1 个孩子（0 个 routing key）时，直接释放 root，让那个唯一的孩子取代它成为新 root；如果一路收缩到 root 变成一个空叶子（`n=0`），就释放它，把 `tree->root` 置为 `NULL`，回到空树的规范状态。

## 10. Range Query：只扫叶子链表，实测访问了多少个叶子

`demo.c` 第 10 节用一棵 `t=2`、6 个叶子的树做 `range_query(35, 95)`：

```
range_query(35, 95)：先向下走到 key=35 应该在的叶子（不是最左叶子），
然后沿 next 链表一路向右扫描，直到超过 95 为止：
找到 6 对 key/value，一共访问了 4 个叶子（总叶子数 6）：
  40:4000
  50:5000
  60:6000
  70:7000
  80:8000
  90:9000
```

`bplustree_range_query()` 的实现分两步：先从 root 往下走一次（跟 `search` 的下降逻辑一样，只是终止条件换成"第一个 `key >= low` 的叶子"），定位到 `low` 应该在的那个叶子；然后完全不再触碰任何内部节点，只沿着 `next` 指针线性扫描，把每个 `>= low && <= high` 的 key-value 对收进结果，一旦遇到 `> high` 或链表走到 `NULL` 就停。整个过程访问的叶子数只跟"结果集横跨了多少个叶子"有关，跟树的总大小、树高完全无关——这跟 B 树的中序遍历（每个经过的内部节点都要重新决策走哪个孩子，访问节点数跟树的形状强相关）是本质不同的复杂度特征。性能测试小节会用更大规模的真实数据验证这句话。

## 属性校验：`bplustree_verify()` 具体检查什么

`verify_rec()` 从 root 开始递归，对每个节点做同样几件事：

1. **key 数量在合法区间内**：非 root 节点是 `[t-1, 2t-1]`；root 作为叶子可以少到 0（空树），root 作为内部节点至少要有 1 个 routing key。
2. **同一节点内的 key 严格递增**：`keys[i] < keys[i+1]`，不允许相等或逆序。
3. **routing key 的边界语义**：递归时给每个孩子传一对 `(min_key, max_key)`，孩子的所有 key 必须满足 `min_key <= key < max_key`——注意左边是**闭区间**（因为 `min_key` 本身就是这个孩子对应的父节点 routing key，它在这个孩子子树里真实存在，等于是允许的），右边是**开区间**（下一个 routing key 属于右边兄弟子树，不能出现在这个孩子里）。这跟 B 树"两侧都严格开区间"的做法不一样，是 B+ 树"内部节点 key 是右子树最小值的复制品"这一特性直接导致的。
4. **叶子深度一致**：用一个共享的 `leaf_depth_out` 记录第一次遇到叶子时的深度，之后每个叶子都必须跟这个深度相同，否则说明树不平衡。
5. **叶子链表完整性**（B+ 树独有，B 树完全不需要这一条）：`bplustree_verify()` 在 `verify_rec()` 递归通过之后，单独做第二次遍历——从 root 一路往左走到最左叶子，然后沿 `next` 走一遍整条链表，边走边检查：链上的 key 是否严格递增（跨越叶子边界也要递增，不能只在单个叶子内部递增）；链表访问到的叶子总数是否等于递归时数到的叶子总数（`chain_leaf_count == leaf_count_via_tree`）；链表上收集到的 key 总数是否等于对整棵树递归数出来的 key 总数（`chain_key_count == total_keys_via_tree`）。这三条交叉校验能同时抓住两类问题：链表断裂（某个叶子被合并释放后忘了修 `next`，会导致链表访问到的叶子数比树recursion看到的少）和链表窜错（某个叶子的 `next` 被错误地指向了不该指向的节点，会导致链上出现逆序或重复）。

## 调试笔记

这一节记录四处真实问题：两处是 `bplustree.c` 本身的缺陷（插入时向上传播分裂的下标计算；`range_query` 写越界），另外两处是 `tests.c` 里定向构造测试用例时犯的构造错误。

**错误 1（算法层面）：分裂父节点时，判断新孩子该落在哪一半用错了时机的计数**。`bplustree.c` 源码里 `bplustree_insert` 向上传播分裂的循环中（父节点也满、需要一起分裂那一段）留着一条 `BUG` 注释，写着"found during random-stress verification"。但我无法在事后重建当时发现它的确切时间点——这一章没有中间提交记录，也没有留下当时的失败日志。诚实的说法是：我**不能**断言这是靠随机压力测试真实抓到的，也不能断言这是写代码时靠人工审查/心算发现、从未在跑起来的程序里真实失败过；两种可能我都没有留下能证明的证据，这里如果编一个具体的"当时的报错输出"会是编造，所以不编。

能做、也做了的是：把这个 bug 重新构造出来，验证它真实存在、真实严重、也真实会被现有测试体系抓住。方法是把修复后的判断（`idx < left_children_before_split`，`left_children_before_split` 在调用 `split_internal` 之前就固定为 `t`）还原成修复前那种直接写法（`idx < parent->n`，在调用 `split_internal` 之后才比较），单独编译成一个临时版本重新跑一遍完整测试套件：

**根本原因**：`split_internal(y, t, sep_key_out)` 会把 `y->n` 直接改成 `t - 1` 作为副作用（分裂后左半部分只剩 `t-1` 个 routing key），然后才返回。调用方本来想用"分裂前左半部分有多少个孩子存活"（正确答案是 `t` 个，下标 `0..t-1`）来判断 `idx` 应该落在分裂后哪一半；如果在调用 `split_internal` **之后**才去读 `parent->n` 来做这个判断，读到的就是已经被改写成 `t-1` 的值，而不是分裂前的孩子数 `t`。读错这个数字会以**两种不同的方式**出错，源码里那条 `BUG` 注释把两种都列了出来，受影响的 `idx` 范围是 `[t-1, 2t-1]`（父节点分裂前是满的，有 `2t-1` 个 routing key、`2t` 个孩子，所以 `idx` 的上界是 `2t-1`）：

1. **`idx == t-1`：走错了一半。** `idx < parent->n` 即 `t-1 < t-1` 为假，新孩子被送进了**右半部分**，而它本该留在左半。这一种确实只在这个单点上发生。
2. **`idx >= t`：这一半走对了，但落点偏了一格。** 判断结果（去右半）是对的，可插入位置算的是 `idx - parent->n`，也就是 `idx - (t-1)`，比正确的 `idx - t` **大 1**。这一种覆盖 `[t, 2t-1]` 整个上半区间。

上一版这里只描述了第一种，并据此写了"错误概率不高"——这个结论是错的：把第二种算进来之后，`[t-1, 2t-1]` 这整段 `idx` 全部会出错，而 `idx` 正是"刚才是从父节点的第几个孩子下来的"，在一棵持续插入的树里落进这段区间是常态而非巧合。这也解释了为什么下面的还原实验里连 `t=2` 的基础顺序插入（`test_degree_t2_sequential`）都会立刻失败，根本不需要跑到 1000+ 规模的随机压力测试。

**还原后的真实运行结果**（`cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer`）。注意"还原"必须把 `left_children_before_split` 的**两处**用法一起换回 `parent->n`——判断那一处（`idx < parent->n`）和减法那一处（`idx - parent->n`），也就是上面两种失效模式各对应一处：

```c
/* 还原后（buggy）：注意 split_internal 已经把 parent->n 改成 t-1 了 */
BPlusNode *new_parent = split_internal(parent, t, &parent_sep);
if (idx < parent->n) {
    internal_insert_at(parent, idx, sep_key, right_child);
} else {
    internal_insert_at(new_parent, idx - parent->n, sep_key, right_child);
}
```

只还原判断那一处、保留 `idx - left_children_before_split` 的减法，得到的是另一种表现：程序**不崩溃，而是挂住不返回**（需要手工 kill）。同一个下标错误既可能表现为 segfault，也可能表现为死循环——取决于坏下标具体把哪个指针写成了什么，这本身就是"下标越界破坏树结构"这类 bug 不可预测性的一个例证。下面这组输出对应的是两处一起还原的版本：

```
$ ./tests_buggy
bplustree_buggy.c:87:16: runtime error: member access within misaligned address 0xbebebebebebebebe for type 'BPlusNode' (aka 'struct BPlusNode'), which requires 8 byte alignment
...
AddressSanitizer:DEADLYSIGNAL
==31027==ERROR: AddressSanitizer: SEGV on unknown address 0x5857d7d9d7d7
    #0 find_leaf_path bplustree_buggy.c:87
    #1 bplustree_insert bplustree_buggy.c:160
    #2 test_leaf_borrow_from_left_sibling tests.c:167
    #3 main tests.c:509
SUMMARY: AddressSanitizer: SEGV bplustree_buggy.c:87 in find_leaf_path
==31027==ABORTING
========== B+ 树测试套件 ==========
[PASS] test_empty_tree_search_and_delete
[PASS] test_single_key_insert_delete
[PASS] test_duplicate_insert_updates_value
[PASS] test_delete_nonexistent_is_noop
  bplustree_verify failed after sequential insert: keys not strictly increasing at index 0: 7 >= -1094795586
  FAILED CHECK: verify_ok(&t, "sequential insert") (tests.c:101)
[FAIL] test_degree_t2_sequential
  bplustree_verify failed after sequential insert: keys not strictly increasing at index 1: 16 >= -1094795586
  FAILED CHECK: verify_ok(&t, "sequential insert") (tests.c:101)
[FAIL] test_degree_t3_sequential
  bplustree_verify failed after sequential insert: keys not strictly increasing at index 2: 29 >= -1094795586
  FAILED CHECK: verify_ok(&t, "sequential insert") (tests.c:101)
[FAIL] test_degree_t4_sequential
[PASS] test_degree_t10_sequential
[PASS] test_leaf_split_copies_key_upward
  bplustree_verify failed after root/internal split: keys not strictly increasing at index 0: 70 >= -1094795586
  FAILED CHECK: verify_ok(&t, "root/internal split") (tests.c:160)
[FAIL] test_root_split_moves_key_no_duplicate
(进程被 AddressSanitizer 中止，之后的用例没有机会跑)
```

不加 sanitizer 的普通严格编译（`-std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g`）跑同一个还原版本，结果更直接：还没打印任何一行输出就直接 segfault（退出码 139）——说明这不是一个"结果稍微不对但程序还能跑完"级别的问题，而是会破坏内存布局、多数情况下直接崩溃的严重缺陷。这两组还原实验证明的是：`test_degree_t2/t3/t4_sequential` 这几个最基础的顺序插入测试、以及 `test_root_split_moves_key_no_duplicate` 会在第一时间抓住这个 bug（连 1000+ 规模的随机压力测试都不需要跑到），如果它出现在最终交付的代码里，是不可能蒙混过关的。而当前仓库里的 `bplustree.c`（提前用 `left_children_before_split = t` 在调用 `split_internal` 之前就固定住分裂前的孩子数）重新编译跑同样的 25 个测试，全部 `[PASS]`，AddressSanitizer/UndefinedBehaviorSanitizer 全程干净——这是这份 README 里所有"全部通过"表述真正对应的、当前磁盘上代码的实际状态。

上一版 README 在这一节开头写的是"`bplustree.c` 本身...没有出现算法层面的 bug"，这句话跟源码里明确留着的这条 `BUG` 注释互相矛盾，属于表述错误，这里予以更正：**`bplustree.c` 有两处真实缺陷——上面的错误 1（分裂传播时的下标越界/树结构损坏）和错误 4（`range_query` 不遵守 `cap` 上限的堆越界写），两处都已修复；错误 1 通过上面的还原实验确认修复有效，错误 4 通过新增的 `test_range_query_respects_cap` 覆盖。错误 1 具体是在开发过程的哪个阶段被发现的，现在已经无法准确重建，不做未经证实的断言。**

**错误 4（`bplustree.c`，写越界）：`range_query` 不遵守自己声明的 `cap` 上限**。`bplustree.h` 里 `bplustree_range_query()` 的契约写的是"返回写入的 key-value 对数（不超过 `cap`）"，但实现里的收集循环只在 `keys_out[written]` 赋值那一步之前检查了一次范围条件，没有检查 `written` 是否已经撞到 `cap`：只要结果集比 `cap` 大，它就会一路写到调用方数组的外面去，同时把 `written` 继续往上加，最后返回一个**比 `cap` 大**的数。

这个 bug 有两重危害，第二重比第一重更隐蔽：

1. 直接的堆越界写。ASan 抓到的是 `heap-buffer-overflow`，越界点刚好在一个 20 字节区域的末尾之后 0 字节处（`cap=5` 的 `int[5]`）。
2. 即使调用方的数组恰好后面还有别的东西、越界写没有立刻崩，返回值本身也已经在撒谎。一个按契约写的调用方会做 `for (i = 0; i < ret; i++)`，而 `ret > cap`，于是**调用方自己的循环**又会读到自己数组的外面去——错误从库内部泄漏到了调用方代码里，而调用方那边看起来完全没有毛病。

修复是在写入之前先判 `written >= cap`，撞到上限就立刻带着已写入的数量返回（同时照样把 `nodes_visited_out` 填好，否则调用方拿到的访问节点数是残缺的）：

```c
if (written >= cap) {
    if (nodes_visited_out) *nodes_visited_out = visited;
    return written;
}
keys_out[written] = leaf->keys[j];
values_out[written] = leaf->u.values[j];
written++;
```

这处缺陷是原来的 24 个测试**全部通过**的情况下存在的——所有 range query 测试都给了足够大的缓冲区，没有一个测试故意把 `cap` 设小。补上的 `test_range_query_respects_cap` 就是专门盯这一条契约的：用一个故意偏小的 `cap` 去查一个更大的区间，断言返回值 `<= cap`。这也是这一章最值得记的一条教训：**"测试全绿"只能说明"被测到的路径是对的"，一条从来没有被测到的契约（这里是 `cap` 上限）出错时，测试套件是完全沉默的。**

接下来两处是 `tests.c` 里定向构造测试用例时犯的构造错误，跟上面两处 `bplustree.c` 的缺陷是完全不同性质的问题：

**错误 2：借位检测条件本身逻辑不自洽**。最初给 `test_internal_borrow` 写的判定条件是"`nodes_after == nodes_before && leaves_after < leaves_before`"，想法是"借位不应该改变节点总数，但叶子数应该变少"——但这个条件本身就是错的：**纯粹的叶子借位根本不会改变叶子数量**，借位只是在两个已存在的叶子之间重新分配 key，不会新建或释放任何叶子。真正应该发生、也真正能观察到的信号是"内部节点借位"——底层先发生一次叶子合并（`leaves` 减 1），这个下溢往上传导到父节点层，父节点从兄弟那里借一个 routing key + 一个孩子指针，而不是合并（`internal` 节点数量不变）。而且我最初选的构造方式（对一棵用 `0..120 step 2` 建出来的宽 `t=2` 树做连续升序删除）根本没有在预期的位置触发过内部借位——沿着那个具体的删除序列，每一次下溢都被合并吸收掉了，从没有轮到"兄弟有多余、可以借"的情况。

发现这个问题之后，我写了大约 15 个一次性探测程序（在 `/tmp`，不在仓库里），一步步缩小范围，最终找到一个最小、无歧义的构造：手工搭一个 root 只有 2 个孩子的树——左孩子 `[30]`（刚好在下限，`n=1`，2 个叶子孩子）、右孩子 `[120 140 160]`（有多余，`n=3`）；然后把左孩子底下两个叶子都削到 `n=1`（两边都没有多余、互相借不了），再删掉其中一个 key，逼出一次叶子合并，这次合并会让父节点 `[30]` 的孩子数减到 1、routing key 数减到 0，从而下溢——这时候它只能向那个"有多余"的兄弟 `[120 140 160]` 借，而不是合并。用精确的计数差值确认了这确实是借位而不是合并：`leaves` 从操作前到操作后减少了正好 1（对应底层那一次叶子合并），但 `internal`（`总节点数 - leaves`）完全没变——如果这是一次内部节点合并，`internal` 应该也要减少。

**错误 3：想当然假定了一个错误的合并连锁层数**。`test_internal_merge_pulls_separator_down` 最初的断言是"删除某个 key 应该正好释放 2 个节点（1 个叶子 + 1 个内部节点）"，这个数字是我在纸面上推演"应该只连锁一层"直接写下的，没有实际测量过。用一次性探测程序把真实的节点数差值打出来之后发现，那次特定的删除（在一棵 `t=2`、从 `0..58`（step 2）建起、又连续删掉 `58,56,...,44` 八个 key 之后的树上删除 42）实际上连锁触发了**两次**内部节点合并，一共释放了 3 个节点（1 个叶子 + 2 个内部节点），不是我猜的 2 个。

错误 2 和错误 3 的共同教训是：**光靠打印出来的树形状用眼睛看，很容易把"借位"和"合并"看反，也很容易低估连锁反应会传导几层**——尤其是当合并/借位发生在中间层、周围还有好几层其他节点的时候，人眼扫一遍打印结果很难一次性数清楚到底哪几个节点被释放了。唯一可靠的办法是在操作前后分别记录精确的 `nodes`/`leaves`/`internal = nodes - leaves` 三个计数，用计数的**差值**（而不是操作前或操作后某一次的绝对值）来判断到底发生了什么：叶子数减 1、内部节点数不变 → 借位；叶子数减 1、内部节点数也减少（可能不止 1 层）→ 合并，具体减少几个就是连锁了几层。这跟上一章 `04_b_tree` 总结出的"先用 `btree_print()` 把真实结构打出来看一眼再写断言"是同一个精神的延伸，但这一章的教训更进一步："打印出来"只是第一步，真正下断言之前还必须换成**精确的计数差值**，不能只靠肉眼判断树形状。

错误 2、3 的修复都在改完之后立刻用 `cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer` 重新编译、重新跑过完整的 25 个测试，全部 `[PASS]`，AddressSanitizer/UndefinedBehaviorSanitizer 都没有报告任何问题。错误 1（下标 bug）的修复同样在当前仓库的 `bplustree.c` 里，也已经用上面还原实验中的"修复后重新编译"这一步确认过：全部 25 个测试 `[PASS]`，sanitizer 干净。错误 4（`range_query` 越界写）修复后同样重新编译、重新跑过 25 个测试并通过，原先能稳定复现的 ASan `heap-buffer-overflow` 不再出现。

## 测试运行：真实输出

```
$ ./tests
========== B+ 树测试套件 ==========
[PASS] test_empty_tree_search_and_delete
[PASS] test_single_key_insert_delete
[PASS] test_duplicate_insert_updates_value
[PASS] test_delete_nonexistent_is_noop
[PASS] test_degree_t2_sequential
[PASS] test_degree_t3_sequential
[PASS] test_degree_t4_sequential
[PASS] test_degree_t10_sequential
[PASS] test_leaf_split_copies_key_upward
[PASS] test_root_split_moves_key_no_duplicate
[PASS] test_leaf_borrow_from_left_sibling
[PASS] test_leaf_merge_repairs_chain
[PASS] test_internal_borrow
[PASS] test_internal_merge_pulls_separator_down
[PASS] test_root_collapses_to_child
[PASS] test_delete_root_variants
[PASS] test_leaf_chain_matches_sorted_keys
[PASS] test_range_query_edge_cases
[PASS] test_range_query_low_greater_than_high
[PASS] test_large_sequential_then_reverse_delete
[PASS] test_random_stress_t2_1000
[PASS] test_random_stress_t3_1000
[PASS] test_random_stress_t5_1500
[PASS] test_random_stress_t10_2000

========== 汇总 ==========
通过: 24, 失败: 0, 总计: 24
```

用 `make san`（`-fsanitize=address,undefined -fno-omit-frame-pointer`）重新构建 `demo`/`tests` 并运行，二者的输出跟非 sanitizer 版本逐字节相同（`diff` 无输出），退出码都是 0——说明这些操作在 ASan/UBSan 的视角下没有任何越界访问、use-after-free、未初始化读取或未定义行为，而且执行路径是完全确定的（sanitizer 不会改变任何分支走向）。

## 设计问题：为什么选"最小度数 `t`"而不是单独定义"叶子容量"和"内部节点容量"

真实数据库实现（比如 InnoDB 的 B+ 树页）通常会让叶子节点和内部节点有不同的容量——叶子存的是完整的 key-value（有时候还带变长字段），内部节点只存 key + 子指针，单条记录更小，同样大小的一页能塞更多路由 key。本章为了跟 `04_b_tree` 直接对照、并且保持实现简单，选择让叶子和内部节点共用同一个 `t`：非 root 叶子和非 root 内部节点的 key 数量都是 `[t-1, 2t-1]`。这带来的后果是叶子分裂阈值和内部节点分裂阈值完全一样，测试和演示的复杂度都低了一档；如果要更贴近真实数据库，一个自然的练习方向是把 `t` 拆成 `leaf_order` 和 `internal_order` 两个独立参数（练习部分会给出这个方向）。

另一个设计取舍是**要不要在叶子里存指向 value 的指针，还是直接存 value 本身**。本实现选的是"直接存 `int` value"（跟 `04_b_tree` 一致），这对小顶点数据是合理的；如果 value 是变长的（比如字符串、大对象），真实做法通常是叶子里存一个指向堆上数据或者磁盘页的指针/偏移量，而不是把变长数据直接摊平进节点数组，否则节点大小会失去可预测性，这跟 B+ 树"节点大小要对齐磁盘页大小"的设计初衷（这也是为什么真实数据库偏爱 B+ 树而不是二叉搜索树——每个节点对应一次磁盘 I/O，希望单次 I/O 能带出尽可能多的路由信息）是冲突的。

## 性能测试：真实测量数据

`perf.c` 是独立于 `demo.c`/`tests.c` 的常驻文件（不是临时脚本），用 `-O2` 编译（`demo.c`/`tests.c` 保持 `-O0 -g` 是为了 sanitizer 调试友好，性能数据需要真实的优化后代码耗时），内部用一个不依赖 libc 随机数的 `xorshift32` 生成可复现的伪随机序列，每次批量插入/删除之后都调用一次 `bplustree_verify()` 确认结构没坏掉再计时下一段。

```
$ make bench
=== insert timing across scale and t ===
n=1000    t=2   height=6   keys=999      time=0.0001s (10978.02 ops/ms)
n=1000    t=4   height=3   keys=999      time=0.0001s (18849.06 ops/ms)
n=1000    t=8   height=2   keys=999      time=0.0000s (24975.00 ops/ms)
n=1000    t=16  height=2   keys=999      time=0.0000s (27750.00 ops/ms)
n=10000   t=2   height=8   keys=9974     time=0.0011s (9436.14 ops/ms)
n=10000   t=4   height=5   keys=9974     time=0.0006s (16217.89 ops/ms)
n=10000   t=8   height=3   keys=9974     time=0.0005s (20997.89 ops/ms)
n=10000   t=16  height=2   keys=9974     time=0.0004s (23975.96 ops/ms)
n=100000  t=2   height=10  keys=97559    time=0.0139s (7042.45 ops/ms)
n=100000  t=4   height=6   keys=97559    time=0.0083s (11768.28 ops/ms)
n=100000  t=8   height=4   keys=97559    time=0.0068s (14416.88 ops/ms)
n=100000  t=16  height=3   keys=97559    time=0.0059s (16614.27 ops/ms)
n=500000  t=2   height=11  keys=442156   time=0.0929s (4759.07 ops/ms)
n=500000  t=4   height=7   keys=442156   time=0.0548s (8075.61 ops/ms)
n=500000  t=8   height=5   keys=442156   time=0.0441s (10020.76 ops/ms)
n=500000  t=16  height=4   keys=442156   time=0.0375s (11803.42 ops/ms)

=== range-query leaf-visit scaling (t=4, varying tree size and range width) ===
n=1000     total_leaves=250      range_width=10     found=6      visited_leaves=2      (0.8% of all leaves)
n=1000     total_leaves=250      range_width=100    found=51     visited_leaves=14     (5.6% of all leaves)
n=1000     total_leaves=250      range_width=1000   found=501    visited_leaves=126    (50.4% of all leaves)
n=10000    total_leaves=2500     range_width=10     found=6      visited_leaves=2      (0.1% of all leaves)
n=10000    total_leaves=2500     range_width=100    found=51     visited_leaves=14     (0.6% of all leaves)
n=10000    total_leaves=2500     range_width=1000   found=501    visited_leaves=126    (5.0% of all leaves)
n=100000   total_leaves=25000    range_width=10     found=6      visited_leaves=2      (0.0% of all leaves)
n=100000   total_leaves=25000    range_width=100    found=51     visited_leaves=14     (0.1% of all leaves)
n=100000   total_leaves=25000    range_width=1000   found=501    visited_leaves=126    (0.5% of all leaves)
n=500000   total_leaves=125000   range_width=10     found=5      visited_leaves=3      (0.0% of all leaves)
n=500000   total_leaves=125000   range_width=100    found=50     visited_leaves=14     (0.0% of all leaves)
n=500000   total_leaves=125000   range_width=1000   found=500    visited_leaves=126    (0.1% of all leaves)

=== delete timing (t=4, 100000 random insert then 100000 random delete of same keys) ===
insert 100000 keys: 0.0084s, delete 100000 keys: 0.0095s, final keys=0 (expect 0)
```

三个结论都能从这份真实数据里直接读出来：

先看一眼 `keys=` 这一列：在每个 `n` 上，4 个 `t` 值的 `keys` 完全相同（999 / 9974 / 97559 / 442156）。这不是巧合，而是这张表能不能拿来横向比较的前提——`perf.c` 里的随机种子只跟 `n` 有关（`xs_state = 12345u + (uint32_t)n`），所以同一个 `n` 下的 4 个 cell 插入的是**同一串** key，唯一的变量就是 `t`。早先的版本里种子还带上了 `t_deg`（`12345 + n + t_deg * 7919`），于是 4 个 cell 拿到 4 串不同的随机键，`keys` 那一列也跟着是 4 个不同的数字（比如 `n=100000` 那一行曾经是 97464 / 97564 / 97533 / 97515）——测出来的时间差里混进了"输入根本不是同一批"这个额外变量。这类噪声在 `n` 很大时统计上很小，但它是零成本就能消掉的，没有理由留在一张专门用来做受控比较的表里。

1. **`t` 越大，插入越快、树越矮**：同样 `n=500000`，`t=2` 的树高是 11、插入吞吐约 4759 ops/ms；`t=16` 的树高只有 4、插入吞吐约 11803 ops/ms——`t` 每翻倍，单节点能容纳的 key 更多，树高显著降低（对数底数变大），每次插入需要下降/分裂的层数变少，吞吐随之提升约 2.5 倍。这也解释了为什么真实数据库的 B+ 树节点大小通常按磁盘页（4KB/8KB/16KB）来定，尽量让 `t` 大到能把树高压缩到 3~4 层——树高就是磁盘 I/O 次数的量级。
2. **range query 访问的叶子数只跟结果集宽度相关，跟树的总大小无关**：固定 `range_width=1000` 这一档，`n` 从 1000 一路涨到 500000（相差 500 倍），`visited_leaves` 始终稳定在 126 左右（1000 时 126，10000 时 126，100000 时 126，500000 时 126）——树越大，这 126 个叶子占全部叶子的比例从 50.4% 一路降到 0.1%，但绝对访问数量完全不随树的总规模变化。这正是"数据库用 B+ 树做范围查询"这句话背后真实的复杂度证据：range query 的代价是 `O(定位一次 log 高度 + 结果集大小)`，而不是 `O(树的总大小)`。
3. **删除跟插入同量级快**：`t=4`、10 万个 key 的随机插入再随机删除全部归零，插入耗时 0.0084s、删除耗时 0.0095s，量级相当，说明借位/合并这条路径（虽然比"不需要调整"复杂得多）并没有引入额外的量级开销。

**关于 sanitizer 的诚实说明**：用 `-fsanitize=address,undefined -fno-omit-frame-pointer` 重新编译并跑一遍同样的 `perf.c`（`-O0 -g`），10 万次删除测试的耗时从 0.0095s 变成了 0.0567s，插入从 0.0084s 变成 0.0542s——慢了大约 6 倍。这是 ASan/UBSan 的检测开销（每次内存访问都要多做一层影子内存检查）叠加 `-O0`（不做任何优化）两者共同造成的，不代表算法本身变慢了；leaf-visit 计数、`verify` 结果、最终 key 数量等所有跟"正确性"相关的数字在 sanitizer 版本下逐一核对完全相同，说明这只是纯粹的常数级指令开销，不影响任何结论。

## 常见误区

- **以为 B+ 树内部节点的 key 和 B 树一样，删除时要找前驱/后继替换**：不是。B+ 树内部节点的 key 只是路由用的副本，删除/合并/借位时可以直接丢弃或搬运，完全不需要"替换"这一步——这一步的消失恰恰是 B+ 树删除逻辑比 B 树简单的地方。
- **以为叶子合并和内部节点合并对分隔 key 的处理方式一样**：不一样。叶子合并直接丢弃分隔 key（它只是副本，真实数据还在合并后的叶子里）；内部节点合并必须把分隔 key 拉下来（它是唯一还在区分两侧孙子层级的信息，丢了就破坏了树的不变量）。这一处不对称正是本章需要反复强调的第二个核心点（第一个是分裂时的复制/移动不对称）。
- **合并叶子时忘记修复 `next` 指针**：`leaf_merge()` 必须在释放右叶子之前执行 `left->next = right->next`，顺序写反或者忘记这一步都会让链表在这个节点处断掉——最危险的地方在于，单纯检查"每个 key 是否还能被 `search` 找到"完全测不出这个问题（因为 `search` 走的是树的下降路径，不依赖 `next`），只有专门遍历链表、和树递归结果交叉校验的检查（本章 `bplustree_verify()` 的第 5 条）才能抓住。
- **以为路由 key 的边界跟 B 树一样两侧都严格开区间**：B+ 树的路由 key 左边界是闭区间（`>=`），因为它本身等于右子树的最小值、在右子树里真实存在；如果照抄 B 树"两侧都开区间"的校验逻辑，会在完全正确的树上产生假的校验失败。
- **只测"key 存在性"，不测"叶子链表完整性"**：一棵树即使所有 key 都能被 `search` 正确找到、`verify_rec()` 的结构校验也全部通过，链表仍然可能因为某次合并漏了修 `next` 而断裂或窜错——这是 B+ 树独有的风险面，B 树完全不存在，测试计划里必须单独覆盖。
- **定向构造"内部节点借位/合并"用例时，只凭打印出来的树形状判断发生了什么**：本章调试笔记里记录的两次真实错误都是这个原因——眼睛看树形状很容易把借位和合并看反，也容易低估连锁反应传导了几层。可靠的做法是在操作前后分别记录精确的 `leaves`/`internal` 计数，用差值判断。
- **在一个会产生副作用的函数调用之后，才去读取"调用前的状态"**：`split_internal()` 会把传入节点的 `n` 直接改写成分裂后的新值，如果调用方在调用之后才去比较 `idx` 和 `parent->n`，读到的就已经是分裂后的值，不是原本想比较的"分裂前左半部分的孩子数"。这类 bug 的教训具有一般性：任何"先记录一个阈值/边界，再据此做分支判断"的逻辑，只要中间插入了一次有副作用的函数调用，就必须显式 snapshot 需要的值，不能依赖"反正调用后应该还是原来的意思"这种假设去读被调用者可能已经改写过的共享状态。

## 小结

B+ 树和 B 树的全部区别，最终可以归结成一句话：**把"路由"和"存数据"这两个职责，从"混在同一层节点里"改成"路由留在内部节点、数据全部下沉到叶子层，叶子之间再用链表串起来"**。这个改动看起来只是"多了一层间接"，但连锁带来了三处质变：内部节点的 key 变成了纯粹的、可以随意丢弃/搬运的路由副本，删除逻辑不再需要"用前驱/后继替换"这一步；叶子合并和内部节点合并对分隔 key 的处理方式因此产生了不对称（丢弃 vs 拉下来），跟分裂时的不对称（复制 vs 移动）正好对称呼应；range query 从"必须做中序遍历"退化成"一次下降定位 + 链表线性扫描"，代价从跟树总大小相关变成只跟结果集大小相关。这三处质变共同解释了为什么几乎所有工业级数据库索引、文件系统都选 B+ 树而不是 B 树——它们都需要频繁的范围扫描，而 B+ 树用"多存一份路由副本 + 一条链表"的空间代价，换来了range query 复杂度的本质改善。

实现和测试过程中也验证了一件更朴素的事：结构性数据结构的正确性不能只靠"key 存在性"这一种检查去覆盖，必须针对每种数据结构独有的不变量单独设计校验（本章是叶子链表完整性），而调试这类删除/合并/借位交织的逻辑时，"打印树形状用眼睛看"只能帮你建立直觉，真正下断言必须换成前后精确计数差值——这条经验从 `04_b_tree` 延续到本章，且在本章因为多了一层"叶子 vs 内部节点"的区分而变得更加重要。

## 练习

1. 把共用的 `t` 拆成两个独立参数 `leaf_order` 和 `internal_order`，让叶子容量和内部节点容量可以分别调节，重新跑一遍 `tests.c` 里所有针对特定 `t` 值构造的定向测试（借位/合并/分裂），确认它们在拆分之后依然能被正确触发。
2. 给 `bplustree_range_query()` 增加一个"反向range query"（从 `high` 开始向左扫描到 `low`），这需要给叶子节点额外维护一个 `prev` 指针，形成双向链表——实现之后想一想：`leaf_merge()`/`leaf_borrow()` 除了原来修复 `next` 之外还需要多修复几处指针？
3. 实现一个 `bplustree_bulk_load()`，接收一个已经排好序的 key-value 数组，直接从下往上一次性建出一棵满足所有不变量的树（不通过反复调用 `bplustree_insert()`），并且让每个非最右的叶子/内部节点都尽量装满（`2t-1` 个 key），只有最右侧路径上的节点允许不满——这是真实数据库"建索引"场景常用的优化，相比逐条插入能省下大量分裂开销。
4. 给 `BPlusNode` 加一个 `prev` 指针（叶子专用，指向链表里的前一个叶子），并在 `bplustree_verify()` 里加一条新的校验：从任意一个叶子出发，沿 `next` 走到底再沿 `prev` 走回来，应该回到出发点——想一想这条校验能抓住哪些"`next` 修对了但 `prev` 没同步修"的 bug，以及这类 bug 在只有单向链表时是不是完全测不出来。
5. 本章的删除实现里，叶子借位/合并的判定条件都是"当前节点 key 数是否低于 `t-1`"。尝试实现一种"提前合并"策略（不等到真正下溢才处理，而是在key 数刚好等于 `t` 时就主动检查是否可以合并，参考 2-3-4 树的做法），比较这种策略和"惰性调整"策略在随机插入/删除混合负载下的实际性能差异。
