# 03_loop —— 循环

## 实验目的

把「边界」「循环不变式」「无符号倒数」「浮点步进」这四个最容易翻车的点各撞一次。

## 文件

| 文件 | 作用 |
|---|---|
| `demo.c` | 正确用法 + 二分查找 + goto cleanup |
| `unsigned_loop.c` | 反例：`for (size_t i = n-1; i >= 0; i--)` 死循环 |

## 编译与运行

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g demo.c -o demo
./demo
```

## 真实输出

```text
== 1. for：三个部分都可以省略 ==
  i=0  i=1  i=2  i=3  i=4

== 2. while：条件先判断，可能一次都不执行 ==
  while(0>0) 循环体执行了 0 次

== 3. do-while：至少执行一次 ==
  do-while 体执行了一次，m=0

== 4. break / continue ==
  1..10 中跳过偶数，遇到 7 停止: 1 3 5 

== 5. 循环不变式（loop invariant）：二分查找 ==
    lo=0 hi=7 mid=3 arr[mid]=7
    lo=4 hi=7 mid=5 arr[mid]=11
  找到 11 在下标 5

== 6. 半开区间 [0, n) 是 C 的默认约定 ==
  len = 7，合法下标是 0..6，arr[7] 就越界了
  写 i <= len 而不是 i < len 就是经典 off-by-one

== 7. 嵌套循环 + 标签式跳出 ==
  7 位于 matrix[1][2]

== 8. goto cleanup 资源回收惯用法 ==
    两块缓冲区都分配成功: a[0]=A b[0]=B
  load_two_buffers(16, 32) 返回 0

== 9. 浮点数不能用来控制循环步进 ==
  for(d=0.0; d<1.0; d+=0.1) 实际跑了 11 次（你以为是 10 次）
  累加十次 0.1 = 0.99999999999999988898
  正确做法：用整数计数，需要时再换算成浮点
```

退出码 `0`。

## 反例：无符号倒数是死循环

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -O0 -g unsigned_loop.c -o unsigned_loop
./unsigned_loop
```

```text
== 错误写法：size_t i 倒着数 ==
  i=2
  i=1
  i=0
  i=18446744073709551615
  i=18446744073709551614
  i=18446744073709551613
  ...安全阀触发，这是死循环！i 现在 = 18446744073709551612

== 正确写法 1：下标 +1 偏移 ==
  arr[2]=30
  arr[1]=20
  arr[0]=10

== 正确写法 2：用有符号类型 ==
  arr[2]=30
  arr[1]=20
  arr[0]=10
```

**重要观察**：默认的 `-Wall -Wextra -Wpedantic` **没有**警告这个 bug。需要显式打开：

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -Wtautological-unsigned-zero-compare -O0 -g unsigned_loop.c -o /dev/null
```

```text
unsigned_loop.c:12:30: warning: result of comparison of unsigned expression >= 0 is always true [-Wtautological-unsigned-zero-compare]
   12 |     for (size_t i = n - 1; i >= 0; i--) {
      |                            ~ ^  ~
1 warning generated.
```

## 机制剖析

### 为什么 `size_t i >= 0` 恒真

`size_t` 是无符号类型，取值范围 `[0, SIZE_MAX]`。「小于 0」这个状态根本不存在。
当 `i == 0` 再执行 `i--`，按标准规定**模 2^64 回绕**到 `SIZE_MAX = 18446744073709551615`，循环永不终止。

输出里一路数到 `18446744073709551612`，正是回绕后继续递减的结果。

### 两个正确写法

```c
/* 写法 1：条件里同时完成判断和自减 */
for (size_t i = n; i-- > 0; ) {
    /* 进入循环体时 i 已经是 n-1, n-2, ... 0 */
}
/* 当 i==0 时，i-- > 0 为假，同时 i 回绕成 SIZE_MAX，但循环已经结束 */

/* 写法 2：干脆用有符号 */
for (int i = (int)n - 1; i >= 0; i--) { ... }
```

### 为什么浮点循环跑了 11 次

`0.1` 在二进制里是无限循环小数，`double` 只能存近似值。累加 10 次的结果是 `0.99999999999999988898`，**小于** `1.0`，于是循环又进了一次。

```text
累加 10 次: 0.99999999999999988898  < 1.0  -> 继续
累加 11 次: 1.09999999999999986677  >= 1.0 -> 退出
```

### `goto cleanup` 为什么合理

C 没有析构函数和 `try/finally`。多资源函数如果每个错误分支都自己 `free`，很快就会漏掉一个。`goto cleanup` 把所有释放集中到一处：

```c
int f(void) {
    int rc = -1;
    char *a = NULL, *b = NULL;

    a = malloc(n1); if (!a) goto cleanup;
    b = malloc(n2); if (!b) goto cleanup;
    /* ... 正常逻辑 ... */
    rc = 0;
cleanup:
    free(b);        /* free(NULL) 是安全的空操作，所以不用判空 */
    free(a);
    return rc;
}
```

关键前提：**所有资源变量在第一个 `goto` 之前就初始化成 NULL**。

## 本章结论

1. C 的惯例是**半开区间 `[0, n)`**，配 `i < n`。`i <= n` 基本都是 bug。
2. 无符号类型倒着数需要特殊写法，`-Wall` 抓不到。
3. 二分查找写 `lo + (hi - lo) / 2`，不写 `(lo + hi) / 2`（防溢出）。
4. 浮点数**永远不要**用来控制循环次数。
5. `break` 只跳一层；跳多层用 `goto` 或把内层抽成函数 `return`。
6. `goto cleanup` 不是坏味道，是 C 里的标准资源管理模式（Linux 内核大量使用）。

## 最佳实践

- 循环变量的作用域尽量小：`for (size_t i = 0; ...)` 而不是在外面先声明。
- 每写一个循环，先用一句话写下**循环不变式**（进入每次迭代前一定成立的性质）。
- 循环条件里不要有副作用（`i++`、函数调用）之外的复杂表达式。
- `goto` 只用于「向前跳到本函数的清理段」，不要用来做循环或向后跳。

## 练习

1. 把 `demo.c` 二分查找里的 `hi = len` 改成 `hi = len - 1`，`while (lo < hi)` 改成 `while (lo <= hi)`，看看哪些用例会出错。
2. 用整数写一个「从 0.0 到 1.0，步长 0.1」的循环，保证正好 10 次。
3. 给 `load_two_buffers` 再加一个资源（比如 `FILE*`），练习扩展 cleanup 段。
