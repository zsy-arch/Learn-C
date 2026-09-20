# 07_memory —— 动态内存

## 文件

| 文件 | 验证什么 |
|---|---|
| `m01_regions.c` | 进程内存分区：代码 / 只读数据 / 全局 / 堆 / 栈 |
| `m02_alloc.c` | `malloc` / `calloc` / `realloc` / `free` 正确用法 |
| `m03_realloc_bug.c` | 反例：`p = realloc(p, n)` 失败时泄漏 |
| `m04_leak.c` | 三种典型泄漏 + `leaks` 工具检测 |

统一编译：`cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g mXX.c -o mXX`（全部通过）

---

## m01 —— 内存分区

```text
== 1. 各个区的典型地址 ==
  函数代码 main          0x10297c598   __TEXT  代码段(可执行，只读)
  函数代码 dummy         0x10297c854   __TEXT
  字符串字面量           0x10297c970   __TEXT/__cstring 只读
  const 全局 g_const     0x10297c96c   __DATA_CONST 只读
  全局 g_initialized     0x102984000   __DATA  已初始化
  static g_static        0x102984004   __DATA
  全局 g_uninitialized   0x102984008   __DATA/__bss 运行时清零
  堆 malloc(16)          0x1029b16d0   heap
  堆 malloc(1MB)         0x77cb400000  heap (大块单独 mmap)
  栈 局部变量            0x16d481db8   stack

== 2. 栈的方向 ==
  递归 0 层时栈顶约 0x16d481c68
  递归 20 层时栈顶约 0x16d480228，共下探约 7031 字节
```

### 地址空间示意（按本次运行的真实地址）

```text
高地址
  0x16d481db8  ┌─────────────────────┐
               │  栈 stack           │  ← 向下生长（递归 20 层用掉 7031 B）
               │        ↓            │
               ├─────────────────────┤
               │      ...未映射...    │
  0x77cb400000 ├─────────────────────┤
               │ 大块 mmap (1MB)     │  ← malloc 大块走 mmap，地址跳得很远
               ├─────────────────────┤
  0x1029b16d0  │  堆 heap  ↑         │  ← 向上生长
               ├─────────────────────┤
  0x102984000  │  __DATA (全局/static)│
  0x10297c96c  ├─────────────────────┤
               │  __DATA_CONST       │  ← const 全局
  0x10297c598  │  __TEXT (代码+字面量)│  ← 只读可执行
低地址         └─────────────────────┘
```

几个可以直接读出来的事实：
- `main`（0x...598）和 `dummy`（0x...854）相差 700 字节 —— 函数代码连续排在 `__TEXT`。
- 字符串字面量（0x...970）和代码在同一个 `__TEXT` 段附近，都是只读。
- `g_initialized`（0x102984000）、`g_static`（+4）、`g_uninitialized`（+8）紧挨着。
- 小块 malloc 在 `0x1029b1xxx`，紧跟在 `__DATA` 之后；1 MB 的大块跳到了 `0x77cb400000`，说明走的是 `mmap` 而不是堆顶扩展。
- 栈在 `0x16d481db8`，离堆非常远。递归时地址递减，证实栈向低地址生长。

### 栈 vs 堆

| | 栈 stack | 堆 heap |
|---|---|---|
| 分配方式 | 移动栈指针，1 条指令 | `malloc` 要找空闲块，可能加锁 |
| 释放 | 函数返回自动回收 | 必须手动 `free` |
| 大小 | 有限（本机 soft `ulimit -s` = 8176 KB） | 受物理内存 / 地址空间限制 |
| 生命周期 | 到所在块结束 | 到你 `free` 为止 |
| 碎片 | 无 | 有 |
| 越界后果 | 破坏相邻栈帧 / 返回地址 | 破坏堆元数据 |
| 典型用途 | 小对象、临时变量 | 大对象、生命周期跨函数的对象 |

```bash
$ ulimit -Ss ; ulimit -Hs
8176        # soft limit，KB，约 8 MB
65520       # hard limit，KB，约 64 MB
```

所以 `int big[4*1024*1024];` 作为局部变量（16 MB）会直接栈溢出。
（注意：`make` 的子 shell 可能已经把 soft limit 提到 hard limit，实测 `make env` 报 65520 KB。
以你**实际运行程序的那个 shell** 为准。）

### 三种存储期

| 存储期 | 谁 | 何时创建 | 何时销毁 |
|---|---|---|---|
| automatic 自动 | 局部变量、函数参数 | 进入块 | 离开块 |
| static 静态 | 全局、`static` 变量 | 程序启动前 | 程序结束 |
| allocated 分配 | `malloc`/`calloc`/`realloc` | 调用时 | `free` 时 |

---

## m02 —— 分配函数的正确用法

```text
== 1. malloc：分配但不初始化 ==
  malloc(5 * 4) = 0x1056016d0
  malloc 后未初始化 [0, 0, 0, 0, 0]
  手动初始化后     [0, 10, 20, 30, 40]

== 2. calloc：分配 + 清零，还能检查乘法溢出 ==
  calloc 后             [0, 0, 0, 0, 0]

== 3. realloc：扩容 / 缩容 ==
  扩容前 a = 0x1056016d0
  扩容后 a = 0x105601500
  前 5 个元素被保留：0 10 20 30 40 
  补齐之后           [0, 10, 20, 30, 40, -1, ...]

== 5. 演示搬家：所有指向旧内存的指针都会悬垂 ==
  扩容前: arr=0x1056016D0  &arr[2]=0x1056016D8  arr[2]=2
  扩容后: arr=0x105602b80  （搬家了！旧地址上的别名全部作废）
  正确做法：realloc 之后重新计算偏移，&arr[2] = 0x105602b88，arr[2]=2

== 7. 分配大小的整数溢出 ==
  malloc(9223372036854775808 * 4) 的乘法会回绕成 0 —— 会分配一个很小的块！
  检查生效，拒绝这次分配
```

**注意第 1 条的陷阱**：这次运行 `malloc` 后读到的全是 0，但这**不是保证**。
`malloc` 返回的内存内容是**不确定的**，读它就是 UB。新进程刚向 OS 要来的页确实是清零的（安全要求），但被 `free` 过又被复用的块就不是了。**永远不要依赖这个。**

**第 5 条是本节最重要的**：`realloc` 从 `0x1056016D0` 搬到了 `0x105602b80`。任何在 realloc 之前保存的、指向旧块内部的指针（这里是 `&arr[2]`）**全部作废**。
这就是为什么动态数组库的文档都会写「任何修改容量的操作会使所有迭代器失效」。

**第 7 条**：`SIZE_MAX/2+1` 乘以 4 回绕成 0。防御写法：

```c
if (n > SIZE_MAX / sizeof *p) { /* 拒绝 */ }
p = malloc(n * sizeof *p);
```

或者直接用 `calloc(n, sizeof *p)` —— 它内部就做这个检查。

### 四个函数的约定速查

| 调用 | 行为 |
|---|---|
| `malloc(0)` | 返回 NULL 或一个可以安全 `free` 的唯一指针（implementation-defined） |
| `calloc(n, size)` | 分配 `n*size` 并**清零**；内部检查乘法溢出 |
| `realloc(NULL, n)` | 等价于 `malloc(n)` |
| `realloc(p, 0)` | C17 里 implementation-defined，**不要用**；要释放就 `free(p)` |
| `realloc` 失败 | 返回 NULL，**原指针 `p` 仍然有效** |
| `free(NULL)` | 合法，什么都不做 |

---

## m03 —— `realloc` 的经典错误

```text
== 正确写法：realloc 结果先放临时变量 ==
  扩容前 p=0x10559d6d0 内容="abc"
  扩容后 p=0x10559d6e0 内容="abc"  <-- 内容被保留

== 用一个「一定会失败」的超大尺寸来验证 ==
  q=0x10559d6d0 内容="xyz"
  grow_good 失败，但 q 仍然是 0x10559d6d0，内容仍是 "xyz"
  -> 可以正常 free，没有泄漏

== 反例演示：grow_bad ==
  调用前 r=0x10559d6d0 内容="lost"
  调用后 r=0x0  <-- 变成 NULL 了
  原来那 8 字节的地址已经没人知道，永远无法 free —— 内存泄漏
  而且里面的 "lost" 也彻底丢了
```

### 对比

```c
/* ❌ 错误：realloc 失败时，p 被覆盖成 NULL，原内存永远丢失 */
p = realloc(p, newsize);
if (p == NULL) { return -1; }     /* 此时已经泄漏了 */

/* ✅ 正确：先接临时变量 */
char *tmp = realloc(p, newsize);
if (tmp == NULL) { return -1; }   /* p 仍然有效，可继续用或 free */
p = tmp;
```

实验用 `(size_t)-1 / 2`（约 9.2 EB）作为分配尺寸，保证失败，从而稳定复现这个 bug。

**最佳实践**：
1. 永远写 `tmp = realloc(p, n); if (tmp) p = tmp;`
2. 扩容策略用**翻倍**而不是 `+1`，否则 n 次 push 是 O(n²) 次拷贝。
3. `realloc` 之后所有指向旧块的指针立即作废。

---

## m04 —— 内存泄漏与检测

### 平台说明：macOS 上 LeakSanitizer 不可用

```bash
$ cc -std=c17 -g -fsanitize=address lk.c -o lk
$ ASAN_OPTIONS=detect_leaks=1 ./lk
==42564==AddressSanitizer: detect_leaks is not supported on this platform.

$ cc -std=c17 -g -fsanitize=leak lk.c -o lk2
clang: error: unsupported option '-fsanitize=leak' for target 'arm64-apple-darwin27.0.0'
```

Valgrind 在 Apple Silicon 上也不可用（`which valgrind` → not found）。

**替代方案：macOS 自带的 `leaks` 工具。** 注意它**不能**和 ASan 同时用（ASan 替换了 malloc），所以要用**普通编译**的二进制。

### 检测命令与真实输出

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g m04_leak.c -o m04_leak
MallocStackLogging=1 leaks --atExit -- ./m04_leak
```

```text
leaks Report Version: 4.0, multi-line stacks
Process 46998: 192 nodes malloced for 31 KB
Process 46998: 3 leaks for 560 total leaked bytes.

STACK OF 1 INSTANCE OF 'ROOT LEAK: <malloc in leak_early_return>':
3   dyld                     0x18102be80 start + 6688
2   m04_leak                 0x100310494 main + 52  m04_leak.c:59
1   m04_leak                 0x100310554 leak_early_return + 24  m04_leak.c:20
0   libsystem_malloc.dylib   0x181228630 _malloc_zone_malloc_instrumented_or_legacy + 152 
====
    1 (320 bytes) ROOT LEAK: <malloc in leak_early_return 0x7cbf02c000> [320]

STACK OF 1 INSTANCE OF 'ROOT LEAK: <malloc in leak_forget>':
2   m04_leak                 0x100310488 main + 40  m04_leak.c:57
1   m04_leak                 0x100310508 leak_forget + 20  m04_leak.c:10
====
    1 (160 bytes) ROOT LEAK: <malloc in leak_forget 0x7cbf028000> [160]

STACK OF 1 INSTANCE OF 'ROOT LEAK: <malloc in leak_overwrite>':
2   m04_leak                 0x100310498 main + 56  m04_leak.c:59
1   m04_leak                 0x1003105c0 leak_overwrite + 24  m04_leak.c:34
====
    1 (80 bytes) ROOT LEAK: <malloc in leak_overwrite 0x7cbec28000> [80]
```

三处泄漏全部被抓到，并且给出了**精确的文件名和行号**（`m04_leak.c:10` / `:20` / `:34`）。

### 一个有意思的细节

程序请求的是 `128 + 256 + 64 = 448` 字节，`leaks` 报告 `560` 字节（`160 + 320 + 80`）。
差额来自 **allocator 的 size class 向上取整**：128→160、256→320、64→80。
这提醒我们：`malloc(n)` 的实际内存开销大于 `n`，频繁分配小块非常浪费。

### 三种泄漏模式

```c
/* 1. 忘记 free */
static void leak_forget(void) {
    char *p = malloc(128);
    strcpy(p, "...");
    /* 缺 free(p) */
}

/* 2. 提前 return 绕过 free */
static int leak_early_return(int fail) {
    char *buf = malloc(256);
    if (fail) { return -2; }      /* 绕过了下面的 free */
    free(buf);
    return 0;
}

/* 3. 指针被覆盖 */
static void leak_overwrite(void) {
    char *p = malloc(64);
    p = malloc(64);               /* 第一块的地址被冲掉 */
    free(p);
}
```

**正确模板**（单一出口 + `goto cleanup`）：

```c
static int no_leak(int fail) {
    int   rc  = -1;
    char *buf = malloc(256);
    if (buf == NULL) { goto cleanup; }
    if (fail) { rc = -2; goto cleanup; }
    rc = 0;
cleanup:
    free(buf);
    return rc;
}
```

### 检测工具速查（本机实测）

| 工具 | 本机可用 | 用途 |
|---|:---:|---|
| AddressSanitizer | ✅ | 越界、UAF、double free、栈溢出 |
| UndefinedBehaviorSanitizer | ✅ | 有符号溢出、移位越界、空指针、除零 |
| LeakSanitizer | ❌ | macOS arm64 不支持 |
| Valgrind | ❌ | Apple Silicon 不支持 |
| `leaks --atExit` | ✅ | 内存泄漏（**不能**配合 ASan） |
| `MallocStackLogging=1` | ✅ | 让 `leaks` 输出分配点的调用栈 |
| `gdb` / `lldb` | ✅ | 断点调试 |

---

## 全章最佳实践

1. 每次 `malloc`/`calloc`/`realloc` 都检查返回值。
2. `malloc(n * sizeof *p)` 之前检查 `n > SIZE_MAX / sizeof *p`；或直接用 `calloc(n, sizeof *p)`。
3. 用 `sizeof *p` 而不是 `sizeof(Type)`，改类型时不会漏。
4. `realloc` 结果先接临时变量。
5. `free` 之后立刻置 NULL。
6. 多资源函数用 `goto cleanup`，所有资源变量先初始化成 NULL。
7. 谁分配谁释放；跨模块传递所有权时在头文件注释里写清楚。
8. 大数组放堆上，不要放栈上。
9. CI 里跑 ASan + UBSan，本地定期跑 `leaks`。

## 练习

1. 给 `m02_alloc.c` 加一个 `safe_malloc_array(size_t n, size_t size)`，内部做溢出检查。
2. 写一个「分配计数器」：用宏把 `malloc`/`free` 包起来，程序结束时打印未释放的块数。
3. 在 `10_project` 的 `Vec` 上跑 `leaks`，确认 0 泄漏（本项目已验证：`0 leaks for 0 total leaked bytes`）。
