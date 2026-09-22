# 06_red_black_tree —— 红黑树:自平衡二叉搜索树

## 实验目的

回答四个核心问题:

1. 普通 BST 插入 1,2,3,4,5...会退化成链表,查找变成 O(n)。红黑树靠什么
   规则保证树高恒为 O(log n)?
2. 为什么用一个"哨兵 NIL 节点"而不是 NULL 表示空叶子?
3. 旋转(rotation)到底在动哪些指针?为什么"漏更新一个 parent"是新手
   最容易犯、又最难发现的错误?
4. 删除修复(delete-fixup)的"双黑"(double-black)是什么意思?为什么它
   比插入修复复杂一倍还多?

## 文件

| 文件 | 作用 |
|---|---|
| `rbtree.h` | 五条性质的规范说明 + 公开 API |
| `rbtree.c` | 旋转 / 插入+修复 / 删除+修复 / 性质校验 / 打印,全部实现 |
| `demo.c` | 11 节演示:插入修复两类情况、删除修复四类情况、后继替位、批量测试、错误示例 |
| `tests.c` | 21 个测试:基础情形、7 类 fixup case 定向构造(插入 3 类 + 删除 4 类)、3 种根删除结构变体、大规模顺序插入+逆序删除、`rb_verify` 自身的检测能力、析构完整性、3 组 1000+ 键随机压力测试 |

## 编译与运行

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g rbtree.c demo.c  -o demo
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g rbtree.c tests.c -o tests
./demo
./tests

# sanitizer 版本(检测内存错误 + 未定义行为)
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g \
   -fsanitize=address,undefined -fno-omit-frame-pointer \
   rbtree.c demo.c  -o demo_san
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g \
   -fsanitize=address,undefined -fno-omit-frame-pointer \
   rbtree.c tests.c -o tests_san
./demo_san && ./tests_san
```

也可以直接用本目录的 `Makefile`:`make all`(编译)、`make test`(跑测试)、
`make san`(跑 sanitizer 版本)、`make clean`。

**真实编译结果**(无警告、无错误,四个目标全部成功):

```text
$ make clean && make all
rm -f demo tests demo_san tests_san
rm -rf *.dSYM
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g rbtree.c demo.c -o demo
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g rbtree.c tests.c -o tests
```

**真实测试结果**(21 个测试全部通过,包括三组共 4500 个键的随机压力测试):

```text
$ ./tests
========== 红黑树测试套件 ==========
[PASS] test_empty_tree_search_and_delete
[PASS] test_single_key_insert_delete
[PASS] test_duplicate_insert_rejected
[PASS] test_delete_nonexistent_is_noop
[PASS] test_insert_fixup_case1_red_uncle
[PASS] test_insert_fixup_case3_black_uncle_line
[PASS] test_insert_fixup_case2_black_uncle_zigzag
[PASS] test_delete_fixup_case1_red_sibling
[PASS] test_delete_fixup_case2_black_sibling_black_nephews
[PASS] test_delete_fixup_case4_black_sibling_red_far_nephew
[PASS] test_delete_fixup_case3_then_case4_red_near_nephew
[PASS] test_delete_two_children_successor_replace
[PASS] test_delete_root_as_leaf
[PASS] test_delete_root_with_one_child
[PASS] test_delete_root_with_two_children
[PASS] test_large_sequential_then_reverse_delete
[PASS] test_random_stress_dense_1000
[PASS] test_random_stress_sparse_1500
[PASS] test_random_stress_wide_2000
[PASS] test_verify_detects_corrupt_parent_pointer
[PASS] test_destroy_clears_both_pointers

========== 汇总 ==========
通过: 21, 失败: 0, 总计: 21
```

**sanitizer 版本同样全部通过**(`make san`,ASan + UBSan,零报告):

```text
$ make san
./demo_san
...(11 节演示全部完成,无 sanitizer 报告)...
./tests_san
========== 红黑树测试套件 ==========
...(21/21 全部 [PASS],与普通版本逐行一致)...
通过: 21, 失败: 0, 总计: 21
```

## 1. 五条性质:红黑树"近似平衡"的全部约束

这是本实现 `rbtree.h` 里的规范原文(CLRS 第三版 Chapter 13):

```c
/* 红黑树五条性质(RB-property,出自 CLRS 第三版 Chapter 13):
 * 1. 每个节点是红色或黑色。
 * 2. 根节点是黑色。
 * 3. 每个叶子节点(这里用哨兵 NIL 表示)是黑色。
 * 4. 如果一个节点是红色,则它的两个子节点都是黑色(不存在两个连续的红色节点)。
 * 5. 对每个节点,从该节点到其所有后代叶子的简单路径上,
 *    均包含相同数目的黑色节点(黑高一致,不含该节点自身)。
 */
```

**这五条性质如何推出"树高是 O(log n)"?** 核心是性质 4 + 性质 5 联合限制了
"最长路径最多是最短路径的 2 倍":

- 性质 5 保证每条路径的**黑节点数**相同,记为黑高 `bh`。
- 性质 4 保证红节点不能相邻,所以任何一条路径上,红节点数最多等于黑节点数
  (每个红节点后面必须跟一个黑节点)。
- 所以任意路径长度上界是 `2 * bh`,下界是 `bh`(全黑路径)。
- 可以证明 `bh >= log2(n+1) - 1`,所以树高 `<= 2 * log2(n+1)`,恒为 O(log n)。

**实测验证**(demo.c 第 10 节,插入 1..15 共 15 个键):

```text
count=15, black_height=3
(说明:15 个节点的红黑树黑高为 3,与理论上界
黑高 <= log2(n+1) 一致,可见红黑树的"近似平衡"约束。)
```

`log2(16) = 4`,黑高 3 确实小于这个上界,与理论一致。

## 2. 为什么用哨兵 NIL 而不是 NULL

同样引用 `rbtree.h` 的原文:

```c
/* 本实现使用「哨兵 NIL 节点」而不是 NULL 表示空叶子:
 * 整棵树只分配一个 NIL 节点,所有空指针位置都指向它。NIL 的颜色永远是黑色,
 * 且 NIL->parent 在旋转/删除修复过程中会被设置为「刚刚离开的那个位置的父节点」,
 * 这样删除修复函数可以统一从 NIL 出发往上找父节点,不需要额外传参数、
 * 不需要在每个函数里对「孩子是不是 NULL」写一次特判。
 * 代价:每次访问 x->left/x->right 时,即使 x 是叶子,也不用先判空,
 * 但反过来「打印/统计」这类只读函数要小心把 NIL 当成普通节点递归下去
 * (会死循环,因为 NIL 的 left/right 也指向自己)——本实现所有递归函数
 * 都先判 `x == tree->nil` 再展开。
 */
```

用一句话概括这笔"设计交易":**换掉了插入/删除里散落的空指针特判**
(`if (x->left != NULL) ...`),**换成了递归/遍历函数里统一的一次哨兵判断**
(`if (x == tree->nil) return;`)。对红黑树来说这笔交易极其划算,因为
insert_fixup、delete_fixup 里访问 `x->parent->parent`、`sibling->left->color`
这类多级指针链的地方非常密集,少一次判空能省掉大量分支。

`rb_create` 里 NIL 的初始化:

```c
tree.nil->color = RB_BLACK;
tree.nil->left = tree.nil->right = tree.nil->parent = tree.nil;
```

`nil->left/right/parent` 全部指向自己——这不是笔误,是故意的:它保证"从
任何方向访问 NIL 的邻居,得到的还是 NIL",不会因为访问到某个未初始化的
垂悬指针而崩溃。但正因如此,`rb_print`、`rb_count` 这类遍历函数必须
**先判断 `x == tree->nil` 再递归**,否则会在 NIL 上死循环。

## 3. 旋转:三个节点换 parent,一个都不能漏

旋转是插入修复、删除修复共用的底层原语。`rbtree.c` 里 `left_rotate`
自带的图解和注释:

```c
/* left_rotate(x):x 的右孩子 y 顶替 x 的位置,x 变成 y 的左孩子。
 *
 *      x                       y
 *     / \                     / \
 *    a   y      ==>          x   c
 *       / \                 / \
 *      b   c               a   b
 *
 * 一共有三个节点的 parent 会变化:y(接管 x 原来的位置)、
 * x(变成 y 的左孩子)、b(从 y 的左孩子变成 x 的右孩子)。
 * 新手最容易漏掉的就是 b 的 parent——b 换了「继父」,必须显式更新。
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
```

**为什么"漏掉 b 的 parent"是最容易犯、又最难发现的错误?**
因为 `x->right = y->left`(把 b 挂到 x 下面)这一步,单看 `x` 这一侧的
`left`/`right` 指针,结构**看起来完全正确**——从 x 往下走能走到 b,
从 y 往下走能走到 x、c。如果你写的遍历/打印/查找代码只沿着
`left`/`right` 往下走,不会发现任何异常。但 b 的 `parent` 依然指向旧的
`y`——一旦后续代码从 b 往上走(`delete_fixup`、`insert_fixup` 几乎全部
逆着 parent 往上找兄弟/叔叔/祖父),就会用错误的路径计算黑高、找错兄弟,
产生的错误可能在很多次操作之后才以看似无关的方式暴露,非常难定位。

### 3.1 真实复现:漏更新 parent 会发生什么

用一个隔离的最小实现复现这个错误(`x=10` 是根,`y=20` 是它的右孩子,
`b=15` 是 y 的左孩子——对应上面图解里旋转前的状态):

```c
static void broken_left_rotate(BNode *x) {
    BNode *y = x->right;
    x->right = y->left;
    /* 漏写: if (y->left) y->left->parent = x; */
    y->left = x;
    /* 漏写: y->parent = x->parent; 以及父节点侧的重新挂接、x->parent = y; */
}
```

**真实运行结果**:

```text
=== 错误示例:left_rotate 忘记更新 parent 指针 ===
旋转前:x=10(parent=NULL,是根), y=20(parent=10), b=15(parent=20)
        x.right -> y, y.left -> b
broken_left_rotate 之后:
  left/right 结构指针看起来「对」:x.right -> 15, y.left -> 10
  但没人更新 parent,三个指针全部过期:
    b.parent = 20   (应为 10,b 现在其实挂在 x 下面)
    x.parent = NULL (应为 20,x 现在是 y 的孩子)
    y.parent = 10   (应为 NULL,y 现在是新的根)

=== 对照组:correct_left_rotate(真实 rbtree.c 的写法)===
correct_left_rotate 之后:
  root = 20, b.parent = 10, x.parent = 20, y.parent = NULL
```

`x.right -> 15, y.left -> 10` 这两行确认了"从上往下看结构是对的"这个
说法——如果你只用 `rb_print` 之类从根往下遍历的方式检查,这棵树看起来
完全正常。但三个 `parent` 全部是错的。

**为什么 AddressSanitizer / UndefinedBehaviorSanitizer 抓不到这个错误?**
把上面的复现代码分别编译成普通版本和 sanitizer 版本运行,两者输出
**逐字节一致**,sanitizer 版本退出码是 0,没有任何报告:

```text
$ cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g \
     -fsanitize=address,undefined -fno-omit-frame-pointer \
     broken_rotation3.c -o broken_rotation3_san
$ ./broken_rotation3_san; echo "exit=$?"
...(输出与普通版本完全相同)...
exit=0
```

原因很简单:这不是内存安全问题——`b.parent` 指向的 `y` 是一个完全合法、
仍然存活的对象,读写它不会越界、不会 use-after-free、不会有未定义行为。
它是一个**纯逻辑错误**:指针指向了一个语义上错误但内存上有效的对象。
Sanitizer 只能检测"内存访问是否合法",不能检测"这个合法的指针是否
指向了语义上正确的东西"。这类 bug 只能靠 `rb_verify` 这种**语义校验**
(检查性质是否成立)或者足够多的定向单测抓出来——这正是本实验坚持
"每次插入/删除之后都跑一次 `rb_verify`"的原因。

## 4. 插入修复:三种情况,把"双红"问题化解掉

新节点永远先染红插入(这样不会改变任何黑高,只可能违反性质 4)。
`insert_fixup` 的循环不变式(CLRS 13.3):**z 是红色,如果 z->parent 也是
红色,则 z->parent 是唯一违反性质 4 的节点。** 每一轮要么把问题原样
上推两层,要么旋转+变色彻底解决。以 z 的父亲是"左孩子"为例(右孩子是
镜像,把所有 left/right 对调):

```text
case 1:叔叔是红色 —— 父、叔变黑,祖父变红,问题原样上推两层
        gp(黑)                      gp(红)
       /      \                    /      \
     p(红)   uncle(红)   ==>     p(黑)   uncle(黑)
     /                            /
   z(红,新插入)                 z(红)
        问题从 p 转移到 gp,继续在 gp 处检查(gp 可能又是别人的孩子)

case 2:叔叔是黑色,z 是"之字形"(左-右)—— 先左旋父节点拉直成一条线
     gp                         gp
       \                          \
      p(红)          ==>        z(红)
         \                      /
        z(红)                p(红)
   (z 之字形)              (转成 case 3 的直线形态,z 变成新的 z->parent)

case 3:叔叔是黑色,z 和父亲一条线(左-左)—— 父变黑、祖父变红,右旋祖父
        gp(黑)                     p(黑)
       /                          /    \
     p(红)           ==>       z(红)  gp(红)
     /
   z(红)
        一次旋转 + 变色,彻底修复,循环结束
```

### 4.1 真实运行:case 1(红色叔叔)——依次插入 10,20,30,15,5,1

```text
===== 第 1 节:插入修复情况 1:红色叔叔 —— 依次插入 10,20,30,15,5,1 =====
  --- 插入 30 之后 ---
    root: key=20  color=BLACK
    root.L: key=10  color=RED
    root.R: key=30  color=RED
  --- 插入 15 之后 ---
    root: key=20  color=BLACK
    root.L: key=10  color=BLACK
    root.L.R: key=15  color=RED
    root.R: key=30  color=BLACK
  --- 插入 1 之后 ---
    root: key=20  color=BLACK
    root.L: key=10  color=RED
    root.L.L: key=5   color=BLACK
    root.L.L.L: key=1   color=RED
    root.L.R: key=15  color=BLACK
    root.R: key=30  color=BLACK
  [verify OK] 六次插入之后的结构
```

插入 15 时:新节点 15 的父亲是 10(红),叔叔是 30(红)——命中 case 1:
10 和 30 变黑,20(祖父)变红,但 20 是根,`insert_fixup` 循环结束后
"强制根变黑"这一步把它压回黑色。插入 1 时同理:父亲 5(红)、叔叔 15
(红)再次命中 case 1,变色后把"双红"推给 10,10 变红——但 10 此时是
根的孩子,循环因新的 `z=10` 不满足"父亲是红"而正常停止,10 最终保持
红色,与实测输出 `root.L: key=10  color=RED` 一致。

### 4.2 真实运行:case 2→3(黑色叔叔,先之字形再拉直)——插入 10,5,7

```text
===== 第 2 节:插入修复情况 2→3:黑色叔叔,先之字形(zigzag)再直线(line) =====
  --- 插入 10,5 之后 ---
    root: key=10  color=BLACK
    root.L: key=5   color=RED
  --- 插入 7 之后(之字形修复完成) ---
    root: key=7   color=BLACK
    root.L: key=5   color=RED
    root.R: key=10  color=RED
  [verify OK] 之字形修复之后
```

插入 7 后:7 是 5 的右孩子、5 是 10 的左孩子,形成"左-右"之字形,叔叔
(10 的另一侧,这里是 NIL)是黑色——先左旋 5 把之字形拉直成"左-左"直线
(case 2→3),再对 10 右旋并变色,一步到位:7 变成新的黑色根,5 和 10
都变成它的红色孩子。整棵树从一条左偏路径变成了完全平衡的三节点结构,
这正是红黑树"用旋转把偏斜结构拉平"的最小例子。

## 5. 删除修复:"双黑"是什么,为什么比插入修复复杂一倍

删除一个**黑色**节点会让它所在的路径少一个黑节点,破坏性质 5。
`rb_delete` 用 `x` 顶替被物理移走的节点(`x` 可能是 `tree->nil`),并把
`x` 记作"双黑"(double-black)——**多背了一重黑色**,用来"欠着"刚被
删掉的那个黑节点。`delete_fixup` 的循环不变式:**x 是黑色,但它所在
路径的黑高比其他路径少 1(因为少算了 x 身上多背的那一重黑)。**
每一轮要么把双黑标记往上推一层,要么彻底吸收掉它,循环结束。

这就是删除修复比插入修复复杂的根本原因:插入修复处理的是"一个局部
的红红冲突",双红问题最多传播两层就能用一次旋转解决;删除修复处理的
是"一整条路径欠一个黑节点",必须让兄弟或祖先"借"黑色出来填补,情况
比双红多一倍不止(4 类 + 镜像,而不是 3 类 + 镜像)。以 x 是"左孩子"
为例(x 是右孩子是完全镜像,把所有 left/right 对调):

```text
case 1:兄弟是红色 —— 兄弟不可能直接吸收双黑(红色节点扛不了黑色债务),
        先变色+左旋父节点,把一个黑色的侄子换成新兄弟,转化成 2/3/4 之一
         p(?)                        s(旧p的颜色)
        /    \                      /    \
     x(黑,双黑) s(红)   ==>       p(红)    c
              /   \               /  \
             b      c          x(双黑) b
        (s 变黑,p 变红,左旋 p,新兄弟是原来的 b)

case 2:兄弟黑色,两个侄子都黑 —— 兄弟"借"一重黑给 x,双黑标记上移一层
         p(?)                       p(双黑,?)
        /    \                     /    \
    x(双黑) s(黑)        ==>    x(黑)   s(红)
           /  \                        /  \
        黑    黑                    黑    黑
        (s 变红,x 指向 p,继续在 p 处检查;若 p 是根,循环直接结束)

case 3:兄弟黑,近侄红、远侄黑 —— 先右旋兄弟把红孩子转到远侧,转化成 case4
         p(?)                        p(?)
        /    \                      /    \
    x(双黑) s(黑)         ==>    x(双黑) near(黑,新s)
           /    \                          \
       near(红) far(黑)                    s(红)
                                              \
                                             far(黑)

case 4:兄弟黑,远侧孩子红 —— 终点:左旋父节点把兄弟提上来,远侧红孩子
        变黑吸收掉双黑标记,x 变成 root,循环终止
         p(?)                        s(继承p的颜色)
        /    \                      /      \
    x(双黑) s(黑)         ==>     p(黑)    far(黑)
              \                   /
             far(红)          x(黑,不再双黑)
        (s 继承 p 的颜色,p 变黑,far 变黑,左旋 p,x=root)
```

### 5.1 真实运行:case 1→2(红色兄弟,变色旋转后落入 case 2 收尾)

构造树:依次插入 2,1,4,3,6,5,7,删除黑色叶子 1:

```text
===== 第 4 节:删除修复情况 1→2:红色兄弟,变色旋转后落入 case2 收尾 =====
    root: key=2   color=BLACK
    root.L: key=1   color=BLACK
    root.R: key=4   color=RED
    root.R.L: key=3   color=BLACK
    root.R.R: key=6   color=BLACK
    root.R.R.L: key=5   color=RED
    root.R.R.R: key=7   color=RED
  [verify OK] 构造完成
  --- 删除 1 之后 ---
    root: key=4   color=BLACK
    root.L: key=2   color=BLACK
    root.L.R: key=3   color=RED
    root.R: key=6   color=BLACK
    root.R.L: key=5   color=RED
    root.R.R: key=7   color=RED
  [verify OK] case1→case2 修复之后
```

1 在 `root(2)` 的左孩子位置,兄弟是 `root.R=4`(红)——命中 case 1:兄弟
4 变黑、父亲 2 变红,再对父亲 2 左旋,4 顶替 2 的位置成为新根,2 变成
4 的左孩子,新兄弟换成 2 原来的右孩子 3(黑,两个孩子都是 NIL)。旋转后
继续检查,兄弟 3 的两个孩子都是黑——命中 case 2:兄弟 3 变红,双黑标记
上移到新根 4,因为 x 已经等于根,循环立即结束。

### 5.2 真实运行:case 2(黑色兄弟,两个侄子都黑)

构造树:插入 20,10,30,5,15,25,35,再删除 5,15,35,25(均为红色叶子,
不触发 fixup)把 10 和 30"拍平"成无孩子的黑色叶子,再删除 10:

```text
===== 第 6 节:删除修复情况 2:黑色兄弟,两个侄子都是黑色 =====
  --- 拍平之后(10、30 均为无孩子的黑色叶子) ---
    root: key=20  color=BLACK
    root.L: key=10  color=BLACK
    root.R: key=30  color=BLACK
  [verify OK] 拍平之后
  --- 删除 10 之后 ---
    root: key=20  color=BLACK
    root.R: key=30  color=RED
  [verify OK] case2 修复之后
```

删除 10:兄弟是 `root.R=30`(黑),30 的两个孩子都是 NIL(黑)——直接
命中 case 2:兄弟 30 变红,双黑标记原样上移到 `x=root`,因为 x 已经是
根,循环立即结束——这是最简单的一种收尾方式,全程没有发生任何旋转。

### 5.3 真实运行:case 3→4(近侄红、远侄黑,先转成 case 4 再收尾)

构造树:插入 20,10,35,32,删除黑色叶子 10:

```text
===== 第 7 节:删除修复情况 3→4:黑色兄弟,近侄子红、远侄子黑 =====
  --- 基础结构 ---
    root: key=20  color=BLACK
    root.L: key=10  color=BLACK
    root.R: key=35  color=BLACK
    root.R.L: key=32  color=RED
  [verify OK] 构造完成
  --- 删除 10 之后 ---
    root: key=32  color=BLACK
    root.L: key=20  color=BLACK
    root.R: key=35  color=BLACK
  [verify OK] case3→case4 修复之后
```

兄弟是 `root.R=35`(黑),35 的左孩子(近侄子)32 是红色,右孩子(远侄子)
是 NIL(黑)——命中 case 3:先对兄弟 35 做"变色+右旋",让远侄子变成
红色,转化为 case 4;紧接着 case 4 一步到位:32 变成新根(黑),20 和 35
变成它的黑色孩子,x 被设成 `tree->root`,循环结束。

### 5.4 真实运行:case 4(远侄子直接为红,一次旋转收尾)

构造树:插入 20,10,35,40,删除黑色叶子 10:

```text
===== 第 8 节:删除修复情况 4:黑色兄弟,远侄子直接为红 =====
  --- 基础结构 ---
    root: key=20  color=BLACK
    root.L: key=10  color=BLACK
    root.R: key=35  color=BLACK
    root.R.R: key=40  color=RED
  [verify OK] 构造完成
  --- 删除 10 之后 ---
    root: key=35  color=BLACK
    root.L: key=20  color=BLACK
    root.R: key=40  color=BLACK
  [verify OK] case4 直接命中之后
```

兄弟 `root.R=35`(黑),远侄子 40 是红色,直接命中 case 4:兄弟 35 变成
父亲(20)的颜色(黑),父亲变黑,远侄子 40 变黑,对父亲左旋,x 设为根,
循环结束——一次旋转就消掉了双黑标记,不需要经过 case 3 的中转。这是
删除修复里"最快"的收尾路径。

## 6. 删除有两个孩子的节点:后继替位

`rb_delete` 处理"z 有两个孩子"时的策略(源码注释原文,`rbtree.c`):

```c
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
```

**关键细节**:真正被 `free` 掉的是 **z**(要删的那个节点对象本身),
而不是后继 y——`free(z)` 是整个 `rb_delete` 里唯一的一次 `free`。
y 并没有被释放,它只是**换了个位置**。这一步用到两次 `transplant`:

1. 先把 y 从它原来的位置摘下来,让它的右孩子 x 顶上去
   (`transplant(tree, y, y->right)`)。如果 y 本来就是 z 的右孩子,这一步
   不需要做,只要把 `x->parent` 手动指向 y 即可(源码里那个
   `if (y->parent == z)` 分支)。
2. 再把 y 接到 z 原来的位置上(`transplant(tree, z, y)`),让 y 接管 z 的
   两个孩子,并继承 z 的颜色(`y->color = z->color`)。

全程没有任何一句往节点里写 key:整个 `rbtree.c` 里唯一给 `key` 赋值的
地方是 `rb_insert` 里新建节点时的 `z->key = key`。

理解这件事的关键是把两样东西分开看:**颜色属于"位置",key 属于"节点
对象"**。这次删除之后,z 那个位置上的颜色没变(由 y 继承),但占据这个
位置的节点对象换成了 y,所以这个位置上的 key 也跟着换成了 y 的 key。
常见的"复制 key 再删后继"写法正好相反——节点对象留在原地不动,被改写
的是它的 key。两种写法在**树形和颜色上完全等价**,但对外行为有一个实质
差别:

- **指针拼接(本实现)**:调用方手里的 `RBNode*` 永远对应同一个 key。
  一个节点对象从生到死只代表一个 key。
- **复制 key**:z 这个地址会突然变成另一个 key,而原来指向后继 y 的
  外部指针变成悬垂指针——两种失效都是静默的,没有任何迹象。

这个区别是可以被**行为测试**直接观测到的,不只是实现风格问题:只要
在删除前记录下每个 key 对应的节点地址,删除之后再逐个检查"这个地址
上的 key 还是不是原来那个",两种写法就会给出不同的结果。

这也解释了为什么 `y_original_color`(**后继原来的**颜色,不是 z 的
颜色)才是决定"要不要调用 `delete_fixup`"的依据:z 原来那个位置上的
颜色由 y 继承下来了,没有变化;真正在树上"少了一个节点"的地方是 y
**原来**待着的那个位置(它被自己的右孩子 x 顶替),所以黑高是否被破
坏只跟 y 原来的颜色有关,跟 z 的颜色无关。

**真实运行**:构造树插入 50,30,70,20,40,60,80,删除有两个孩子的 30:

```text
===== 第 9 节:删除有两个孩子的节点:后继替位 =====
  --- 基础结构 ---
    root: key=50  color=BLACK
    root.L: key=30  color=BLACK
    root.L.L: key=20  color=RED
    root.L.R: key=40  color=RED
    root.R: key=70  color=BLACK
    root.R.L: key=60  color=RED
    root.R.R: key=80  color=RED
  --- 删除 30 之后 ---
    root: key=50  color=BLACK
    root.L: key=40  color=BLACK
    root.L.L: key=20  color=RED
    root.R: key=70  color=BLACK
    root.R.L: key=60  color=RED
    root.R.R: key=80  color=RED
  [verify OK] 后继替位之后
```

30 的后继(右子树最小值)是 40。删除后:40 原来那个红色叶子位置消失了,
40 出现在原来 30 所在的位置上,并且是**黑色**(30 原来的颜色)。

要注意这段打印本身**区分不出**两种实现:"y 搬到 z 的位置" 和 "把 y 的
key 复制进 z" 打出来的树形和颜色是一模一样的,这也正是上面说的"树形上
等价"。能区分的只有节点地址——`40` 这一行现在对应的是**原来那个装着 40
的节点对象**(它被搬了位置、换了颜色),而不是"原来装着 30 的节点被改
写成了 40"。那个装 30 的对象已经被 `free` 掉了。这一点靠看打印看不出来,
要靠记录地址来验证(见第 13 节练习 4)。

至于颜色:位置上的颜色是 30 原来的黑色,这由 `y->color = z->color` 保证,
跟 40 自己原来是红色没有关系——颜色属于"位置",key 属于"节点对象",这
两者在这次删除里各走各的路。

## 7. 和 2-3-4 树的关系

红黑树本质上是**2-3-4 树的二叉表示**:把一个 2-3-4 树的每个节点"拆开"成
一条由红色节点连接的小链,就得到一棵等价的红黑树。

```text
2-3-4 树的一个 3-节点(装两个键 a < b,三个子指针):
        [ a | b ]
       /    |    \
     T0    T1    T2

拆成红黑树的等价形态(选 a 或 b 作为黑色节点,另一个变成它的红色孩子):
         b(黑)                    a(黑)
        /    \          或        /    \
      a(红)   T2               T0     b(红)
     /   \                            /   \
    T0   T1                         T1    T2
```

- 2-3-4 树的**4-节点**(三个键、四个子指针)拆成红黑树是:一个黑色节点,
  左右各挂一个红色孩子——正好是插入修复 case 1(红色叔叔)里"父、叔
  都红,处理后变成的局部形态"。
- 2-3-4 树里"节点分裂"(4-节点插入新键后必须分裂成两个 2-节点,并把
  中间键上交给父节点)对应红黑树插入修复里的**变色+上推**(case 1)。
- 2-3-4 树永远是**完美平衡**的(所有叶子深度相同),这也是红黑树"黑高
  在所有路径上都相同"(性质 5)的直接来源——黑色节点在红黑树里的深度
  分布,对应的正是 2-3-4 树里节点的深度分布。

这只是一个帮助建立直觉的类比,本实验的实现完全基于 CLRS 的红黑树原始
定义(节点着色 + 旋转),不依赖 2-3-4 树的显式数据结构。

## 8. 常见错误示例

### 8.1 ⚠️ 旋转忘记更新 parent 指针

已在第 3 节详细展开,这里给出结论:**旋转永远牵动三个节点的 parent**
(旋转轴、顶替上来的节点、"过继"的那棵子树),漏掉任何一个都会产生一个
sanitizer 抓不到的纯逻辑错误——树从根往下看是对的,但从某个节点往上找
父亲/兄弟/祖父会走错路。**唯一可靠的防线是 `rb_verify` 之类的性质校验**,
配合"每次插入/删除后立即校验"的测试习惯。

### 8.2 ⚠️ 插入修复结束后忘记强制根为黑

`insert_fixup` 循环结束后必须补一句"根强制设为黑"(rbtree.c 原文:
`tree->root->color = RB_BLACK;`),因为 case 1 可能把根从黑变红
(根的两个孩子变黑、根自己变红)。demo.c 第 11 节用一个隔离的最小实现
演示了漏掉这一步的后果:

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

这是另一个"结构合法、性质不合法"的典型:变色后的树仍然是一棵合法的
二叉搜索树,红色节点也没有相邻——**唯一被破坏的是性质 2**。`rb_verify`
在这种情况下会明确报告 `性质 2 被破坏:根节点 X 是红色`,而单纯检查
"是不是 BST"或者"有没有红红相邻"是抓不到这个错误的,这也是为什么
`rb_verify` 要把五条性质**逐一独立**检查,而不是合并成一个笼统的
"是否合法"判断。

### 8.3 ⚠️ 只检查结构、不检查性质,会让 bug 隐藏很久

以上两个例子的共同点:**破坏之后的树在很长一段时间内还能正常工作**——
`rb_search` 不受影响(BST 有序性没被破坏),插入还能继续插(只是有可能
基于错误的颜色前提做出错误的 fixup 决策)。如果测试只验证"查找结果对
不对",这类 bug 可能要经过几十次操作、构造出很特殊的形状之后才会让
查找结果出错,调试成本极高。这正是本实验坚持"每次插入/删除之后立即
调用 `rb_verify`,而不是只在最后验证一次"的原因——把破坏性质的那一刻
和报错的那一刻**尽量拉到同一次操作里**,而不是隔着几十步之后才发现。

## 9. 测试覆盖说明

`tests.c` 的 21 个测试分成六组(下面每组的数字加起来正好是 21):

- **基础情形**(4 个):空树查找/删除、单键插入删除、重复键插入被拒绝
  (`rb_insert` 返回 `false`、树不变)、删除不存在的键是空操作。
- **fixup case 定向构造**(7 个):插入 case 1/2/3、删除 case 1/2/3→4/4,
  每个测试都手工构造出触发该分支所必需的颜色/结构前提,不依赖随机数
  "碰巧"命中某个分支。
- **规模化顺序操作**(1 个):`test_large_sequential_then_reverse_delete`
  ——顺序插入一大批递增键(普通 BST 在这种输入下会退化成链表)、每步
  `rb_verify`,然后按**逆序**全部删掉。它同时压两件事:有序输入下的
  平衡保证,以及"一直从同一侧删"这种最偏斜的删除模式。
- **`rb_verify` 自身的检测能力**(1 个):
  `test_verify_detects_corrupt_parent_pointer` ——故意把 `parent` 指针改
  坏,断言 `rb_verify` **必须**返回失败,然后改回来再确认恢复正常。它构造
  两种破坏:①把某个节点的 `parent` 指向一个不是自己父亲的真实节点;
  ②把 `root->parent` 从哨兵改成一个真实红节点——第 ② 种尤其值得单独测,
  因为 `insert_fixup` 的 while 循环正是靠"哨兵是黑色"在根处终止的,这个
  指针一坏,循环会越过根继续往上走。
  这是一个"测试测试工具"的测试:第 8.1 节说"漏更新 parent 指针只能靠
  `rb_verify` 这条防线抓",那这条防线本身就必须被证明是有牙齿的——否则
  一旦 `rb_verify` 漏掉这项检查,所有依赖它的测试会一起静默变成空转。
- **析构的完整性**(1 个):`test_destroy_clears_both_pointers` ——断言
  `rb_destroy` 之后 `root` 和 `nil` **两个**指针都被置空、`rb_count` 和
  `rb_black_height` 不再去读已释放的哨兵,并且重复调用 `rb_destroy` 是
  幂等的(不会二次释放)。这是一个回归测试:原来的写法先 `root = nil`
  再 `free(nil)`,结果销毁后 `nil == NULL` 而 `root` 指向刚被释放的内存
  ——**"清理了一半"比完全不清理更危险**,因为 `nil == NULL` 看起来像个
  可用的"已销毁"标志,会误导调用方以为这个 `tree` 处于某种可检测的状态
  (源码 `rb_destroy` 里保留了这段注释)。
- **删除结构变体**(4 个):两个孩子的节点后继替位,以及**删除根节点**
  的三种结构子情况——根是叶子、根只有一个孩子、根有两个孩子:

  ```text
  [PASS] test_delete_root_as_leaf
  [PASS] test_delete_root_with_one_child
  [PASS] test_delete_root_with_two_children
  ```

  这三个测试专门覆盖"被删除节点恰好是根"时 `transplant` 对
  `tree->root` 的更新路径(`u->parent == tree->nil` 分支),这条路径
  在"删除任意非根节点"的测试里不会被触发。
- **随机压力测试**(3 组,共 4500 个键,`random_stress` 辅助函数):

  | 测试 | 种子 | 键数 n | 值域 | 覆盖场景 |
  |---|---|---|---|---|
  | `test_random_stress_dense_1000` | 111 | 1000 | 500 | 值域远小于键数,大量重复键被拒绝插入,逼着树处理"键空间稠密"的反复插入/删除 |
  | `test_random_stress_sparse_1500` | 2026 | 1500 | 1,000,000 | 值域远大于键数,几乎不重复,逼着树处理大规模稀疏键分布 |
  | `test_random_stress_wide_2000` | 999999 | 2000 | 100,000 | 中等值域,插入删除模式介于前两者之间 |

  每组测试用固定种子的 `xorshift32` 生成键序列,**插入 n 个键之后再
  用 Fisher-Yates 洗牌得到随机删除顺序**,并且——这是最关键的一点——
  **每插入一个键之后立即调用一次 `rb_verify`,每删除一个键之后也立即
  调用一次 `rb_verify`**,而不是等全部操作完成后只验证一次。任何一次
  验证失败都会立刻打印是在第几次操作、对哪个键上失败,并中止测试。
  固定种子保证了这些测试是**可重现**的:同样的种子每次运行都会产生
  同样的键序列和同样的删除顺序,方便复现和调试。

## 10. 性能:红黑树 vs 什么都不做的 BST

红黑树用"每次插入/删除后可能触发的旋转+变色"这笔**额外开销**,换来
"任何情况下树高都是 O(log n)"这个**保证**。普通 BST 插入有序数据
(比如 1,2,3,...,n)会退化成一条链,查找变成 O(n);红黑树在同样的输入
下,`rb_black_height` 恒定不超过 `log2(n+1)`,查找保持 O(log n)。

实测(demo.c 第 10 节,插入 1..15):

```text
count=15, black_height=3
```

`log2(16)=4`,黑高 3 严格小于这个上界。这笔"用旋转换平衡"的交易,
对**输入本身就接近随机**的场景收益不大(随机插入的 BST 本身期望树高
就是 O(log n)),但对**有序或近似有序的输入**(日志时间戳、自增 ID、
排序后批量导入等)收益是决定性的——没有平衡保证的 BST 在这类输入下
会直接退化成链表,而红黑树不会。

## 11. 最佳实践

1. **用哨兵 NIL 而不是 NULL**:换掉插入/删除里散落的空指针特判,换成
   递归函数里统一的一次 `x == tree->nil` 判断,但要小心遍历/打印函数
   别把 NIL 当成普通节点递归下去。
2. **旋转永远要检查三个 parent**:旋转轴、顶替上来的节点、被过继的
   子树,漏掉任何一个都是 sanitizer 抓不到的纯逻辑错误。
3. **`insert_fixup`/`delete_fixup` 结束后别忘记边界收尾**:插入结束后
   强制根变黑(性质 2);删除的 while 循环退出后 `x->color = RB_BLACK`
   对"因为 x 变成根退出"是幂等的,对"因为 x 是红黑双节点吸收了双黑
   标记退出"是必须的一步,两种退出路径共用同一行代码收尾。
4. **不要凭直觉推导 fixup 的 case 顺序,写代码验证**:双黑修复的 4 类
   +镜像分支互相转化关系复杂(1→2/3/4,3→4),手工在纸上推演容易算错
   一步,应该写一个隔离的最小复现程序实际跑一遍再下结论。
5. **每次插入/删除之后立即 `rb_verify`,不要攒到最后**:结构错误(BST
   有序性被破坏)和性质错误(红红相邻、黑高不一致、根不是黑色)在很
   长时间内都不会影响查找结果,攒到最后才验证会让 bug 隐藏很多步之后
   才暴露,大幅增加调试成本。
6. **随机压力测试要固定种子**:固定种子的伪随机序列(比如 xorshift32)
   保证测试可重现,失败时能够复现同样的操作序列来调试,而不是"偶尔
   失败一次却无法重现"。
7. **删除有两个孩子的节点时,分清"颜色属于位置、key 属于节点对象"**:
   本实现用 CLRS 的指针拼接——被 `free` 的是 z(要删的那个节点对象),
   后继 y 被搬到 z 的位置并继承 z 的颜色。所以"后继替位后颜色不变"
   是因为 `y->color = z->color` 显式赋值,而不是因为节点对象被留下。
   相比常见的"复制 key 再删后继"写法,拼接多花几行指针操作,换来一条
   对外可见的契约:**一个节点对象从生到死只代表一个 key,调用方持有的
   `RBNode*` 不会静默变成别的 key**。要改成复制 key 的写法之前,先想清
   楚有没有外部代码依赖这条契约。

## 12. 开发过程实录:测试到底测出了什么

如实记录:`rbtree.c` 的核心算法(旋转、插入+修复、删除+修复、性质校验)
在本次全部测试中——18 个定向构造的 case/结构测试、3 组共 4500 个键的随机
压力测试(每次插入/删除后立即 `rb_verify`)、以及全部构建目标在
AddressSanitizer + UndefinedBehaviorSanitizer 下的重跑——**没有发现任何
一次 `rb_verify` 失败,没有任何一次 sanitizer 报告**。四个构建目标
(`demo`、`tests`、`demo_san`、`tests_san`)在 `-std=c17 -Wall -Wextra
-Wpedantic -Werror` 下全部编译零警告、零错误。

这不代表"随便写写就能一次写对红黑树"——CLRS 第 13 章的删除修复本身就
以"最容易在实现里出隐蔽 bug"著称,4 类 case+镜像互相转化、每个 case
需要精确控制变色顺序和旋转参数,任何一步写反(比如 case 3 的变色顺序、
case 4 该给 `far` 侄子还是 `near` 侄子染黑)都会产生只有 `rb_verify`
才能抓到的性质违反。核心防线是**先写测试再确认行为,而不是凭直觉相信
实现是对的**:本实验里每一个"定向构造的 case 测试"都是先在草稿程序里
真实构造出触发该分支所需的颜色/结构前提、真实跑一遍确认走到了预期的
分支,再把验证过的构造和期望结果写进 `tests.c`——包括本 README 里
"删除有两个孩子的节点保留的是谁的颜色"这类容易凭直觉猜错的细节,也是
先写一个隔离的探测程序实际打印出 `y_original_color` 和最终颜色的关系,
确认与源码注释一致后才写进文档。

在准备本 README 的示例代码时,确实发现并修复了一个真实的 bug——**不在
`rbtree.c` 里,而在为"错误示例"临时编写的辅助脚本里**:一个用共享
`static` 缓冲区把整数格式化成字符串的辅助函数,当它在**同一个 `printf`
调用的两个不同实参位置**被调用两次时,由于 C 标准没有规定函数实参的
求值顺序,第二次调用会在第一次调用的结果被 `printf` 读取之前就覆盖掉
共享缓冲区,导致打印出的第一个值是错的。这个 bug 是通过"打印出来的值
和直接访问字段得到的值不一致"发现的——一旦怀疑输出有问题,第一反应
就是绕开中间的格式化逻辑直接读原始字段做交叉验证,而不是相信打印结果。
修复方式是给每个待格式化的值分配独立的栈缓冲区,不再共享。这个插曲
本身也印证了本 README 反复强调的原则:**任何输出,无论是程序的还是
文档草稿里的,在写进最终结论之前都需要独立验证**——本 README 里贴出
的每一段"真实运行结果",都是修复问题之后重新真实运行、重新截取的
输出,不存在手写或推测的终端输出。

## 13. 练习

1. **为什么 `rb_insert` 的新节点必须先染红,不能先染黑?** 提示:从
   "黑高是否被改变"这个角度思考——如果新节点直接染黑会发生什么。
2. **手动推演**:依次插入 1,2,3,4,5,6,7 到一棵空红黑树,每插入一个
   数就画出树的形状和颜色。哪几步触发了旋转?哪几步只是变色没有旋转?
   写一个小程序验证你的手动推演是否正确(别只靠直觉,本 README 第 12
   节就是一个"直觉容易出错"的真实例子)。
3. **删除修复的镜像对称性**:`delete_fixup` 里 x 是左孩子和 x 是右孩子
   的两个分支,是把所有 `left`/`right` 互换得到的镜像。找一组能触发
   "x 是右孩子"分支下 case 3→4 的插入/删除序列,验证行为和本 README
   5.3 节(左孩子版本)是完全镜像的。
4. **用行为测试区分"指针拼接"和"复制 key"**(对应第 6 节):写一个探测
   程序,插入一批键之后,先遍历整棵树把每个 `(节点地址, key)` 记下来;
   然后删掉若干个**有两个孩子**的节点,再遍历一遍,检查所有仍然存活的
   地址上的 key 是否**还是原来那个**。在本实现(拼接)下这个断言恒成立;
   如果把 `rb_delete` 改成"把后继的 key 复制进 z、再删后继"的写法,同一
   个断言会立刻失败。想清楚:为什么这两种写法打印出来的树形和颜色完全
   一样,却能被这个测试区分开?这个差别在什么场景下会变成真实的 bug?
5. **扩展 `rb_verify`**:目前它只校验 5 条性质。给它加上"顺便统计一下
   树里一共有多少个红色节点、多少个黑色节点",验证你的统计结果和
   `rb_black_height` 返回的黑高之间满足什么不等式关系。
6. **对照实现一个不用哨兵 NIL、直接用 NULL 的版本**(至少实现
   `rb_insert` + `insert_fixup`),数一数比起本实现,`insert_fixup` 里
   多出了几处"判断孩子是否为 NULL"的特判代码,体会第 2 节讨论的
   这笔设计权衡的真实代价。

## 结语

红黑树是"用有限的几条局部规则(五条性质),换取全局的平衡保证(树高
O(log n))"的一个典范:每一次插入/删除只需要局部地检查父亲、兄弟、
叔叔/侄子的颜色,通过旋转和变色,就能把破坏性质的"问题"逐步转化、
上推或就地消灭,而不需要重新审视整棵树。这种"局部修复保证全局性质"
的设计思路,在 B 树、跳表、并发数据结构里都能看到类似的影子。

本实验完整实现并验证了 CLRS 标准的红黑树:旋转、插入+6 分支修复、
删除+8 分支修复(4 类+镜像)、以及独立于结构遍历之外的性质校验
`rb_verify`。18 个定向测试 + 3 组共 4500 键的随机压力测试(合计 21 个
测试用例)+ 全套 sanitizer 重跑,均未发现核心算法的任何缺陷——但这个"零 bug"结论的
可信度,建立在"每一步都先写探测程序验证行为、再写文档和测试"这个
过程之上,而不是建立在"代码看起来是对的"这种直觉之上。删除修复的
"双黑"概念是全篇最难的部分,建议结合第 5 节的四类 case 图解和
`demo.c` 里对应的真实运行输出反复对照,直到能不看代码、只看树的形状
就说出下一步该走哪个 case 为止。
