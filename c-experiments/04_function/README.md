# 04_function —— 函数、值传递、栈帧

## 实验目的

用**地址**证明「C 只有值传递」，用**递归时局部变量的地址差**证明栈帧的存在和生长方向。

## 文件

| 文件 | 作用 |
|---|---|
| `demo.c` | 值传递 / 栈帧 / 递归 / 错误处理 / goto cleanup |
| `mathutil.h` + `mathutil.c` + `app.c` | 模块化：头文件、include guard、`static` 内部函数 |
| `dangling_return.c` | 反例：返回指向局部变量的指针 |

## 编译与运行

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g demo.c -o demo
./demo
```

## 真实输出

```text
== 1. C 只有值传递（pass by value）==
  调用前: a = 1, &a = 0x16f7e9dc8
    进入函数: 形参 x = 1, &x = 0x16f7e9d4c
    函数内改成 x = 999（改的是副本）
  调用后: a = 1  <-- 没变！形参是实参的一份拷贝

== 2. 想改调用者的变量，就把「地址」按值传进去 ==
  调用前: a = 1
    进入函数: px = 0x16f7e9dc8, *px = 1
  调用后: a = 999  <-- 变了。传的是 a 的地址的副本

== 3. 栈帧：递归时每层局部变量的地址 ==
    depth=0  &local=0x16f7e9d3c
    depth=1  &local=0x16f7e9cec   与上一层相差 80 字节
    depth=2  &local=0x16f7e9c9c   与上一层相差 80 字节
    depth=3  &local=0x16f7e9c4c   与上一层相差 80 字节
    depth=4  &local=0x16f7e9bfc   与上一层相差 80 字节
  地址递减 => 本平台栈向低地址方向生长

== 4. 递归 ==
  factorial(0) = 1
  factorial(5) = 120
  factorial(10) = 3628800

== 5. 错误处理：返回状态码 + 出参 ==
  safe_div(10,3) -> OK           result=3
  safe_div(10,0) -> ERR_DIV_ZERO result 未被修改，仍是 3
  safe_div(10,2,NULL) -> ERR_NULL_ARG

== 6. goto cleanup 与所有权转移 ==
  build_report 返回 0, report = "report-32"
```

退出码 `0`。

## 关键证据解读

### 证据 1：`&a` 和 `&x` 不是同一个地址

```text
调用前: a = 1,       &a = 0x16f7e9dc8
进入函数: 形参 x = 1, &x = 0x16f7e9d4c
                          ^^^^^^^^^^^ 差了 0x7c = 124 字节
```

形参 `x` 是**另一个对象**，住在被调函数自己的栈帧里。改它当然不会影响 `a`。

### 证据 2：传地址时，`px` 的值 == `&a`

```text
调用前: &a = 0x16f7e9dc8
进入函数: px = 0x16f7e9dc8   <-- 完全相同
```

注意：**`px` 本身仍然是按值传递的**（传的是「地址这个值」的副本）。所以在函数里写 `px = ...` 不会影响调用者的指针，只有写 `*px = ...` 才会影响调用者的 `a`。

### 证据 3：栈帧向低地址生长，每层 80 字节

```text
depth=0  0x16f7e9d3c
depth=1  0x16f7e9cec   -80
depth=2  0x16f7e9c9c   -80
depth=3  0x16f7e9c4c   -80
depth=4  0x16f7e9bfc   -80
```

差值完全一致，说明每一层递归都建立了一个**大小相同**的栈帧。栈帧里装着：保存的 `x29`/`x30`（帧指针和返回地址）、局部变量、传参用的临时空间、以及 arm64 ABI 要求的 16 字节对齐 padding。

栈帧示意（每层）：

```text
高地址
        ┌──────────────────────┐
        │ 调用者的栈帧          │
  x29 → ├──────────────────────┤  ← 本层帧指针
        │ 保存的 x29 / x30     │
        ├──────────────────────┤
        │ local_marker (int)   │
        │ prev_frame  (ptr)    │
        │ padding / 临时        │
  sp  → └──────────────────────┘  ← 本层栈顶
                ↓ 下一层再往低地址走 80 字节
低地址
```

## 模块化实验

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g -c mathutil.c -o mathutil.o
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g -c app.c      -o app.o
cc mathutil.o app.o -o app
./app
```

```text
mu_add(3, 4)            = 7
mu_clamp(15, 0, 10)     = 10
mu_clamp(-5, 0, 10)     = 0
2..20 里的素数: 2 3 5 7 11 13 17 19
mu_sum(data, 5)        = 15
```

要点：
- `mathutil.c` 里的 `mu_min` / `mu_max` 是 `static`，不会出现在符号表里，也不会和别的文件撞名。
- `mu_sum(const int *arr, size_t n)` 必须显式传长度——数组一旦传参就退化成指针（见 `05_pointer/p02_decay`）。
- `mathutil.c` 第一行 `#include "mathutil.h"`：让编译器检查实现和声明是否一致。

## 反例：返回局部变量的地址

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -O0 -g dangling_return.c -o dangling_return
```

编译警告：

```text
dangling_return.c:9:13: warning: address of stack memory associated with local variable 'local' returned [-Wreturn-stack-address]
    9 |     return &local;
      |             ^~~~~
dangling_return.c:15:12: warning: address of stack memory associated with local variable 'buf' returned [-Wreturn-stack-address]
   15 |     return buf;
      |            ^~~
2 warnings generated.
```

直接运行（**这是最危险的情况**）：

```text
读取悬垂指针 *p = 12345
读取悬垂指针 s  = `]m
```

`*p` 居然「正确」打印出了 12345 —— 因为那块栈内存还没被覆盖。而 `s` 已经变成乱码。**同一个 bug，两种表现，这就是 UB 的本质。**

用 ASan 抓：

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -O0 -g \
   -fsanitize=address,undefined -fno-omit-frame-pointer \
   dangling_return.c -o dangling_return_asan
ASAN_OPTIONS=detect_stack_use_after_return=1 ./dangling_return_asan
```

```text
=================================================================
==28384==ERROR: AddressSanitizer: stack-use-after-return on address 0x000102899020
READ of size 4 at 0x000102899020 thread T0
    #0 0x000100668910 in main dangling_return.c:21
    #1 0x00018102be7c in start+0x1a1c (dyld:arm64e+0x31e7c)

Address 0x000102899020 is located in stack of thread T0 at offset 32 in frame
    #0 0x000100668970 in bad_make_int dangling_return.c:7

  This frame has 1 object(s):
    [32, 36) 'local' (line 8) <== Memory access at offset 32 is inside this variable
SUMMARY: AddressSanitizer: stack-use-after-return dangling_return.c:21 in main
```

ASan 精确指出：读的是 `bad_make_int` 栈帧里的 `local`，而那个栈帧已经返回了。

## 本章结论

1. **C 只有值传递。** 传指针也是值传递，只不过传的值恰好是一个地址。
2. 想在函数里修改 `T`，参数就写 `T*`；想修改 `T*`，参数就写 `T**`。
3. 局部变量的生命周期到函数返回为止，**绝不能**把它的地址返回出去。
4. 需要返回「函数内产生的数据」有三条路：
   - 调用者提供缓冲区（`void f(char *out, size_t outsize)`）—— 最推荐
   - 返回 `malloc` 的指针，并在文档里写明「谁负责 free」
   - 返回结构体（按值，会整体拷贝）
5. 错误处理用「返回状态码 + 出参」，不要用魔数返回值。
6. 多资源函数用 `goto cleanup`。

## 最佳实践清单

- 所有只在本文件用的函数都加 `static`。
- 每个 `.c` 第一行 include 自己的 `.h`。
- 函数入口检查指针参数是否为 `NULL`。
- 输入参数用 `const T *`，输出参数用 `T *`，并在参数顺序上把输出放最后。
- 分配内存的函数，名字里体现所有权（`xxx_new` / `xxx_create`），并配套 `xxx_free`。
- `-Wmissing-prototypes -Wstrict-prototypes` 打开，强制所有非 static 函数都有声明。

## 练习

1. 修改 `swap`：写一个 `swap_any(void *a, void *b, size_t size)`，用 `unsigned char*` 逐字节交换。（提示：`05_pointer/p08_void_null.c` 有答案）
2. 把 `dangling_return.c` 的 `bad_make_int` 改成三种正确写法（出参 / malloc / static 缓冲区），比较各自的优缺点。
3. 给 `mathutil` 加一个 `mu_gcd`，写单元测试，用 `make` 构建。
