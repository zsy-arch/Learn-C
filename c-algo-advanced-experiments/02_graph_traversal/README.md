# 02_graph_traversal —— 图遍历算法

## 本章问题

1. 无向图判断有没有环，为什么不能直接照搬有向图的"遇到访问过的节点就是环"？
2. BFS 求最短路径时，为什么先出队的节点一定保证是最短的？
3. 拓扑排序的结果可能不止一种，那"对不对"到底怎么验证？

## 文件

| 文件 | 作用 |
|---|---|
| `graph.h` / `graph.c` | 邻接表 + 邻接矩阵 + 数组模拟队列/栈 |
| `algos.h` / `algos.c` | BFS、DFS（递归+迭代）、连通分量、拓扑排序（Kahn+DFS）、环检测（有向+无向）、二分图判定 |
| `demo.c` | 7 个分节演示，构造有代表性的图并打印执行过程 |
| `tests.c` | 21 组测试用例 |
| `buggy_cycle_demo.c` | ⚠️ **错误示例**：无向图环检测忘记排除父节点，会产生假阳性 |
| `Makefile` | `make all` / `make test` / `make san` |

## 编译与运行

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g graph.c algos.c demo.c -o demo
./demo

cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g graph.c algos.c tests.c -o tests
./tests

# sanitizer 版本
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer graph.c algos.c demo.c -o demo_san
./demo_san
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer graph.c algos.c tests.c -o tests_san
./tests_san

# 错误示例
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g graph.c buggy_cycle_demo.c -o buggy_cycle_demo
./buggy_cycle_demo
```

或者直接：

```bash
make all    # 编译 demo 和 tests
make run    # 编译并运行 demo
make test   # 编译并运行 tests
make san    # 编译并运行 sanitizer 版本
```

所有命令均在本机（Darwin 27.0.0 arm64，Apple clang 21.0.0）实际执行，退出码全部为 `0`。

## 1. 图的表示：邻接矩阵 vs 邻接表

```text
========== 1. 图的表示：邻接矩阵 vs 邻接表 ==========
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

邻接矩阵：`n*n` 的二维数组（这里用一维数组模拟，`m[u*n+v]`），查询"u 和 v 之间有没有边"是 O(1)，但空间是 O(V²)，稀疏图下大量浪费。

邻接表：每个节点挂一条链表，只存真实存在的边，空间 O(V+E)。**代价**是查询"u 和 v 之间有没有边"要遍历链表，最坏 O(度数)。本章后面所有算法都用邻接表，因为图遍历天然是"给我这个节点的所有邻居"，链表正好是这个操作的最优结构。

注意打印顺序：`graph_add_edge` 用头插法往邻接表插入新节点（`node->next = g->adj[u]; g->adj[u] = node`），所以同一个节点后加的边会排在前面——这是**实现细节**，不影响任何算法的正确性（BFS/DFS 只关心"访问了哪些邻居"，不关心访问顺序里谁先谁后由邻接表顺序决定，只要每个邻居都被访问到）。

## 2. BFS 最短路径

```text
========== 2. BFS 最短路径（无权图）==========
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

演示图是：

```text
    0 - 1 - 3
    |       |
    2 ------4
```

**为什么先出队的节点保证是最短的？** BFS 的核心不变量是：队列里的节点，距离永远是"非递减"的——先进队的节点距离一定 ≤ 后进队的节点距离。这是因为 BFS 是"一层一层"扩散的：处理完当前层所有节点后，才会把下一层的节点全部塞进队列。所以当你从队列里弹出一个节点时，所有距离更小的节点已经处理完了，不可能再有更短的路径能到达它。

这也是为什么 `dist[v] == -1`（还没访问过）才更新距离和前驱——**第一次到达就是最短**，后面再有边指向 v 也不需要更新。

## 3. DFS：递归 vs 迭代，发现/完成时间

```text
========== 3. DFS：递归 vs 迭代，发现/完成时间 ==========
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

### 递归调用栈是怎么变化的

```text
dfs(0) disc=0
  dfs(2) disc=1        <- 0 的邻接表第一个是 2（头插法，后加的边先访问）
    dfs(4) disc=2
      dfs(5) disc=3
        (5 无出边) fin=4
      fin=5
    fin=6
  dfs(1) disc=7         <- 回到 0，继续处理 0 的下一个邻居 1
    dfs(3) disc=8
      (3 无出边) fin=9
    fin=10
  fin=11
```

`disc`/`fin` 有一个重要性质：**对任意两个节点 u、v，它们的 `[disc,fin]` 区间要么完全嵌套，要么完全不相交，绝不会部分重叠**。这是因为 DFS 的递归本质就是括号匹配——进入一个节点相当于开括号，离开相当于闭括号。

### 迭代版为什么不能"访问节点时把所有邻居一次性入栈"

一个常见的错误迭代实现是：

```c
/* ❌ 简化但不等价的写法 */
stack_push(s, start);
while (!stack_empty(s)) {
    int u = stack_pop(s);
    if (visited[u]) continue;
    visited[u] = true;
    for (邻居 v of u) stack_push(s, v);
}
```

这样写**能保证访问到所有节点**，但访问**顺序**和递归版不一样，而且没法算出正确的 `disc`/`fin`（一个节点可能被多次压栈）。本实验的 `dfs_iterative` 用"帧"（`Frame { node, next_edge }`）模拟调用栈的每一层，`next_edge` 记录"这一层还没处理到哪个邻居"，这才是递归调用栈的忠实模拟——测试里 `test_dfs_recursive_matches_iterative` 专门验证了两者访问顺序逐项相同。

**关于递归深度**：如果图退化成一条长链（比如 10 万个节点首尾相连），递归 DFS 会有 10 万层调用栈，可能栈溢出；迭代版用堆上分配的数组模拟栈，不受调用栈大小限制，这也是为什么生产代码里处理不可控规模的图时更推荐迭代 DFS。

## 4. 连通分量

```text
========== 4. 连通分量（无向图）==========
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

做法很直接：对每个还没被标记分量的节点做一次 BFS/DFS，能到达的所有节点都归入同一个分量，分量号自增。孤立节点（没有任何边）自己单独成一个分量——这是新手容易漏掉的边界情况，`tests.c` 里的 `test_multiple_components` 专门测了两个孤立节点 5、6。

## 5. 拓扑排序：Kahn 算法 vs DFS

```text
========== 5. 拓扑排序：Kahn vs DFS ==========
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

演示图对应经典的"穿衣服"依赖关系：内衣(0)→裤子(1)、内衣(0)→鞋(2)、裤子(1)→鞋(2)、裤子(1)→皮带(3)、衬衫(4)→外套(5)、外套(5)→皮带(3)、袜子(6)→鞋(2)。

### Kahn 算法：入度表的变化

Kahn 算法的思路是"先穿不依赖任何东西的衣服"：

```text
初始入度: [0]=0 [1]=1 [2]=3 [3]=2 [4]=0 [5]=1 [6]=0
入度为 0 的进队列: 0, 4, 6

取出 0 -> 输出 0，把 0 的邻居(2,1)入度减一: [1]=0 [2]=2
    1 的入度变成 0，入队
取出 4 -> 输出 4，把 4 的邻居(5)入度减一: [5]=0
    5 的入度变成 0，入队
取出 6 -> 输出 6，把 6 的邻居(2)入度减一: [2]=1
取出 1 -> 输出 1，把 1 的邻居(3,2)入度减一: [3]=1 [2]=0
    2 的入度变成 0，入队
取出 5 -> 输出 5，把 5 的邻居(3)入度减一: [3]=0
    3 的入度变成 0，入队
取出 2 -> 输出 2（无出边）
取出 3 -> 输出 3（无出边）

最终输出: [0,4,6,1,5,2,3]
```

如果某个节点的入度**永远降不到 0**（比如环里的节点），队列会提前空掉，处理的节点数少于 n，Kahn 算法据此判断"有环，无法拓扑排序"（对应代码里 `return cnt == n`）。

### DFS 版：完成时间的逆序就是拓扑序

直觉：如果 u 依赖 v（u→v 有边），那么 DFS 必须先把 v 这整棵子树处理完才能"完成" u（因为 u 的完成时间在它所有能到达的节点之后）。所以**按完成时间从晚到早排列**，就得到一个合法拓扑序。实现里用三色标记（0=白/1=灰/2=黑），灰色代表"在当前 DFS 路径上，还没完成"，如果访问到一个灰色节点，说明兜了一圈回到自己路径上的祖先，也就是找到了环。

**为什么两次结果不一样，但都算对？** 因为很多节点之间没有依赖关系（比如"内衣"和"衬衫"互不依赖），谁先谁后都行。拓扑序合法性的验证方法必须是"对每条边 u→v，检查 u 的位置在 v 前面"（`topo_order_is_valid`），**不能**直接比较两个序列是否相同——这是 `test_topo_sort_nonunique_but_both_valid` 这个测试要强调的点。

## 6. 环检测：有向图 vs 无向图（本章重点）

```text
========== 6. 环检测：有向图 vs 无向图 ==========
  [有向图] 0->1->2->0 (有环):
    has_cycle_directed = true
  [有向图] 0->1->2 (无环):
    has_cycle_directed = false
  [无向图] 简单路径 A-B-C (0-1-2)，只有 2 条边:
    has_cycle_undirected = false
    -> 如果照抄有向图的写法（忘记排除父节点），
       DFS 从 1 走到 0 时会看到 0 的邻接表里有「回到 1」的边，
       那其实就是刚刚走过来的边，误判成环就是本章重点错误
  [无向图] 三角形 0-1-2-0 (真的有环):
    has_cycle_undirected = true
```

### 有向图：三色标记

有向图判环用和拓扑排序一样的三色标记：DFS 过程中，如果碰到一个**灰色**（还在当前递归路径上）节点，说明存在一条"回边"，指回了自己的祖先，那就是环。碰到黑色（已经完全处理完的）节点是正常的，不算环（那只是一条指向"别的分支"的普通边）。

### 无向图：为什么不能直接照搬

这是新手最容易犯的错误。原因在于**无向边在邻接表里存了两次**：`graph_add_edge(g, u, v)` 对无向图会同时往 u 的邻接表加 `u→v`，往 v 的邻接表加 `v→u`。DFS 从 u 走到 v 之后，检查 v 的邻接表，第一件事就会看到"回到 u"这一项——但那不是新发现的环，只是刚刚走过来的那条边的镜像。

正确做法：记录"当前节点是从哪个父节点走过来的"，遇到已访问节点时，**只有它不是父节点**，才是真正的环。

### 亲手验证这个 bug：`buggy_cycle_demo.c`

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g graph.c buggy_cycle_demo.c -o buggy_cycle_demo
./buggy_cycle_demo
```

真实输出：

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
加边 graph_add_edge(g, u, v) 对无向图会同时生成两条邻接表项：
  u 的邻接表里加一条 u->v
  v 的邻接表里加一条 v->u
DFS 从节点 1 走到节点 0（沿着 1->0 这条边）之后，
检查 0 的邻接表时，会看到「0->1」这一项——
这其实就是刚才走过来的那条边的另一半，不是新发现的环。
错误实现只要看到「已访问」就报环，等价于把每一条边都当成了环。

正确写法：记录 DFS 是从哪个父节点走过来的，
遇到已访问节点时，只有当它不是父节点，才是真正的环。
对比 algos.c 里的 has_cycle_undirected(const Graph *g)。
```

**连一条简单的三节点路径都会被误判成有环**——这个错误实现会把**任意一条边**都当成环，因为任何一条边在无向图里都会形成"走过去再看回来"的假象。

用 sanitizer 编译这个错误示例（`cc ... -fsanitize=address,undefined ... buggy_cycle_demo.c -o buggy_cycle_demo_san && ./buggy_cycle_demo_san`），程序**依然正常退出（exit=0），sanitizer 不会报任何错**——因为这不是内存安全问题，是纯粹的逻辑错误，指针访问、内存分配全都合法，只是"判断逻辑"错了。这提醒我们：**sanitizer 能抓内存 bug，抓不到算法逻辑 bug**，逻辑正确性必须靠性质验证（比如本实验对照"手算的正确答案"）和多组测试用例来保证。

## 7. 二分图判定

```text
========== 7. 二分图判定 ==========
  [二分图] 4 节点环 0-1-2-3-0 (偶数长度):
    is_bipartite = true, 染色: [0,1,0,1]
  [非二分图] 三角形 0-1-2-0 (奇数长度环):
    is_bipartite = false
    -> 奇数长度的环一定不是二分图：染色到最后一条边必然冲突
```

二分图判定用 BFS 染色法：从任意节点开始染色 0，它的所有邻居必须染成 1（因为二分图要求同色节点之间没有边），邻居的邻居再染回 0，以此类推。如果某条边连接了两个同色节点，说明染色冲突，不是二分图。

**为什么奇数长度的环一定不是二分图？** 沿着环走一圈，颜色是 0,1,0,1,...交替。如果环长度是偶数，走一圈正好回到起点时颜色对得上；如果是奇数，走一圈回到起点时颜色会跟起点自己冲突（比如三角形：0 号染 0，1 号染 1，2 号染 0，但 2 号和 0 号之间还有一条边，两边都是 0，冲突）。

## 测试用例

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g graph.c algos.c tests.c -o tests
./tests
```

真实输出：

```text
[PASS] test_empty_graph
[PASS] test_single_node
[PASS] test_multiple_components
[PASS] test_directed_cycle_present
[PASS] test_directed_no_cycle
[PASS] test_undirected_no_cycle_simple_path
[PASS] test_undirected_no_cycle_star
[PASS] test_undirected_cycle_triangle
[PASS] test_undirected_cycle_with_extra_branch
[PASS] test_topo_sort_valid_dag
[PASS] test_topo_sort_detects_cycle
[PASS] test_topo_sort_nonunique_but_both_valid
[PASS] test_bipartite_even_cycle
[PASS] test_bipartite_odd_cycle_fails
[PASS] test_bipartite_disconnected
[PASS] test_bfs_shortest_path_known_answer
[PASS] test_bfs_unreachable
[PASS] test_dfs_recursive_matches_iterative
[PASS] test_large_random_graph_bfs_dfs_consistency
[PASS] test_large_random_dag_topo_sort
[PASS] test_adjacency_matrix_basic

========== 测试汇总 ==========
PASS: 21, FAIL: 0, TOTAL: 21
```

| 测试 | 覆盖点 |
|---|---|
| `test_empty_graph` | 0 个节点 |
| `test_single_node` | 单节点（含有向自环判环） |
| `test_multiple_components` | 多个连通分量 + 孤立点 |
| `test_directed_cycle_present` / `test_directed_no_cycle` | 有向图环检测：有环/无环 |
| `test_undirected_no_cycle_simple_path` | **重点**：A-B-C 简单路径不能误判为环 |
| `test_undirected_no_cycle_star` | 星形图（多分支但无环） |
| `test_undirected_cycle_triangle` / `test_undirected_cycle_with_extra_branch` | 无向图真实环，含带分支的情况 |
| `test_topo_sort_valid_dag` / `test_topo_sort_detects_cycle` | Kahn + DFS 拓扑排序，合法 DAG 与有环图 |
| `test_topo_sort_nonunique_but_both_valid` | 拓扑序不唯一时的正确验证方式 |
| `test_bipartite_even_cycle` / `test_bipartite_odd_cycle_fails` / `test_bipartite_disconnected` | 二分图正反例 + 多分量图 |
| `test_bfs_shortest_path_known_answer` / `test_bfs_unreachable` | BFS 最短路径与手算答案对比，含不可达 |
| `test_dfs_recursive_matches_iterative` | 递归/迭代 DFS 访问顺序逐项一致 |
| `test_large_random_graph_bfs_dfs_consistency` | 200 节点随机无向图（固定种子），BFS 可达数 = 分量大小 |
| `test_large_random_dag_topo_sort` | 150 节点随机 DAG（固定种子），两种拓扑排序都合法 |
| `test_adjacency_matrix_basic` | 邻接矩阵有向/无向的对称性 |

## Sanitizer 与内存检查

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer graph.c algos.c demo.c -o demo_san
./demo_san    # exit=0，无任何 sanitizer 报告
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer graph.c algos.c tests.c -o tests_san
./tests_san   # exit=0，21/21 PASS，无任何 sanitizer 报告
```

用 macOS `leaks` 工具做泄漏检测（ASan 会替换 malloc，所以泄漏检测要用普通编译的二进制）：

```text
$ MallocStackLogging=1 leaks --atExit -- ./demo
Process 36872: 189 nodes malloced for 16 KB
Process 36872: 0 leaks for 0 total leaked bytes.

$ MallocStackLogging=1 leaks --atExit -- ./tests
Process 37159: 189 nodes malloced for 16 KB
Process 37159: 0 leaks for 0 total leaked bytes.
```

`graph_destroy` 会遍历每个节点的邻接链表逐一 `free`，`bfs_free`/`dfs_free` 释放各自的结果数组——这两处是本实验里最容易漏 `free` 的地方（图有 n 个邻接链表，销毁时必须每条链表都走一遍，不能只 `free(g->adj)` 了事，那样只释放了指针数组本身，链表节点全部泄漏）。

## 机制小结：BFS 队列 vs DFS 栈

```text
BFS（队列，先进先出）：一层一层扩散
  访问 0 -> 队列: [1,2]（0 的所有邻居）
  访问 1 -> 队列: [2,3]（1 的邻居里没访问过的追加到队尾）
  访问 2 -> 队列: [3,4]
  ...
  -> 同一层的节点大致按发现顺序连续出队，天然按"距离"分层

DFS（栈，后进先出，或递归调用栈）：一条路走到底
  访问 0 -> 栈: [1,2]
  访问 2（后加的先弹出）-> 栈: [1,4]
  访问 4 -> 栈: [1,5]
  访问 5（走到底了）-> 栈: [1]
  回退，访问 1 -> ...
  -> 一头扎进一条分支，走到头再回退，不会"分层"
```

## 常见错误汇总

1. **无向图判环直接照搬有向图逻辑**：忘记排除父节点，任何一条边都会被误判成环。见 `buggy_cycle_demo.c`。
2. **拓扑排序结果比较用 `==`**：拓扑序不唯一，必须用"每条边 u→v，u 的位置在 v 前面"来验证，不能直接比较两个序列。
3. **BFS 忘记在入队时就标记已访问**（而不是出队时才标记）：如果标记延迟到出队才做，同一个节点可能被多次加入队列，浪费空间且可能重复计数。本实现里 `dist[v] == -1` 这个判断同时承担了"标记访问"和"入队条件"两个职责，入队的瞬间就把 `dist[v]` 设成了非 -1，杜绝了重复入队。
4. **邻接表销毁时只 `free(g->adj)`**：只释放了存指针的数组本身，n 条链表上的节点全部泄漏。必须先遍历每条链表 `free` 节点，再 `free(g->adj)`。

## 最佳实践清单

1. 稀疏图（边数远小于 V²）用邻接表；只有节点数很小、要频繁查询任意两点是否相邻时才考虑邻接矩阵。
2. BFS 求最短路径（无权图）：入队时立刻标记距离，杜绝重复入队。
3. DFS 判环：有向图用三色标记（灰色=在当前路径上）；无向图必须额外传入"父节点"，排除镜像边。
4. 拓扑排序结果的正确性验证方式是"边的相对顺序"，不是"和某个标准答案完全相同"。
5. 图退化成长链时递归 DFS 可能栈溢出，不确定图规模就选迭代 DFS。
6. 邻接表销毁必须先释放每条链表上的节点，再释放头指针数组。
7. sanitizer 抓不到纯逻辑 bug（比如判环算法本身写错），必须配合"和已知正确答案对比"的测试。

## 练习

1. 给 BFS 加一个"多源"版本：从多个起点同时开始扩散，求每个节点到"最近的一个起点"的距离（提示：把所有起点一开始就塞进队列）。
2. 实现 Tarjan 或 Kosaraju 算法求有向图的强连通分量（本实验只做了无向图的连通分量）。
3. `is_bipartite` 目前用 BFS 实现，改写成 DFS 染色版本，并验证在同一批测试用例上结果一致。
4. 思考：如果一个有向图"没有环"，它的拓扑排序一定存在且唯一吗？构造一个反例。
