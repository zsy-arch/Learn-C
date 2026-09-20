# 08_ub —— 未定义行为与调试

> ⚠️ **本目录所有 `uXX_*.c` 都是「错误示例」。** 它们的存在是为了让你**看见** UB 的后果，绝不可以照抄进生产代码。

## 什么是 undefined behavior

C 标准把行为分成四类：

| 类别 | 含义 | 例子 |
|---|---|---|
| **well-defined** | 标准规定了确切结果 | `unsigned` 溢出回绕 |
| **unspecified** | 有限几种可能，实现不必说明选哪种 | 函数参数求值顺序 |
| **implementation-defined** | 实现必须文档化自己的选择 | `char` 是否有符号 |
| **undefined (UB)** | **标准不施加任何要求** | 有符号溢出、越界、UAF |

UB 的关键不是「结果不确定」，而是**整个程序的行为都不再受任何约束**。
编译器把「程序不含 UB」当成公理来推理，于是 UB 可以让**远处**的代码消失（见 `u08`）。

## 文件

| 文件 | UB 类型 | 抓它的工具 |
|---|---|---|
| `u01_signed_overflow.c` | 有符号整数溢出 | UBSan |
| `u02_oob.c` | 数组越界（栈/堆/off-by-one） | ASan + UBSan |
| `u03_uaf.c` | use-after-free / double free | ASan |
| `u04_uninit.c` | 读未初始化对象 | 编译器警告（MSan 在 macOS 不可用） |
| `u05_nullderef.c` | 解引用空指针 | UBSan + ASan |
| `u06_literal_write.c` | 修改字符串字面量 | ASan（SIGBUS） |
| `u07_misc.c` | 移位越界 / 除零 / 求值顺序 | UBSan |
| `u08_optimizer.c` | UB 如何让优化器删掉你的检查 | 对比 `-O0` / `-O2` |

## 统一的 sanitizer 编译命令

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -O0 -g \
   -fsanitize=address,undefined -fno-omit-frame-pointer \
   uXX_xxx.c -o uXX_xxx_san
./uXX_xxx_san
```

---

## u01 —— 有符号整数溢出

### 不加 sanitizer

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -O0 -g u01_signed_overflow.c -o u01_signed_overflow
./u01_signed_overflow
```

```text
INT_MAX = 2147483647
即将计算 a + 1，其中 a = 2147483647
a + 1 = -2147483648   <-- 没有任何保证，UBSan 会在上一行报错

INT_MIN = -2147483648
-INT_MIN = -2147483648   <-- 同样是 UB
INT_MIN / -1 = -2147483648
```

退出码 0。**看起来「正常回绕」了，但这纯属运气**——arm64 的加法指令恰好这么干。

### 加 UBSan

```text
u01_signed_overflow.c:16:15: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior u01_signed_overflow.c:16:15 
u01_signed_overflow.c:22:15: runtime error: negation of -2147483648 cannot be represented in type 'int'; cast to an unsigned type to negate this value to itself
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior u01_signed_overflow.c:22:15 
u01_signed_overflow.c:28:15: runtime error: division of -2147483648 by -1 cannot be represented in type 'int'
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior u01_signed_overflow.c:28:15 
```

三处 UB 全部被精确定位到行和列。

### 正确写法

```c
/* 1. 编译器内建（GCC/Clang） */
int out;
if (__builtin_add_overflow(a, b, &out)) { /* 溢出 */ }

/* 2. 可移植：加之前先判断 */
if (b > 0 && a > INT_MAX - b) { /* 会溢出 */ }
if (b < 0 && a < INT_MIN - b) { /* 会溢出 */ }

/* 3. 明确需要回绕语义时，用 unsigned */
unsigned r = (unsigned)a + (unsigned)b;   /* 模 2^N，完全有定义 */
```

---

## u02 —— 数组越界

### 不加 sanitizer：**静默通过**（最危险）

```text
##### mode=stack
  arr[5] = 1   <-- 越界读，UB
如果你看到这一行，说明这次越界「碰巧」没炸 —— 这才是最危险的情况
exit=0

##### mode=heap
  p[5] = 99   <-- 越界写，UB
exit=0

##### mode=offbyone
  off-by-one 求和 = 1
exit=0
```

三次越界（含一次**越界写**）全部静默通过，退出码 0。堆的元数据可能已经被破坏，但要等到几百次分配之后才会崩，那时根本无从追查。

### 加 ASan：精确定位

```text
##### mode=stack
u02_oob.c:11:58: runtime error: index 5 out of bounds for type 'int[5]'
=================================================================
==50805==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x00016f1b1bf4
READ of size 4 at 0x00016f1b1bf4 thread T0
    #0 0x000100c4cde8 in stack_oob u02_oob.c:11
    #1 0x000100c4ca60 in main u02_oob.c:44

Address 0x00016f1b1bf4 is located in stack of thread T0 at offset 52 in frame
    #0 0x000100c4cb40 in stack_oob u02_oob.c:8

  This frame has 1 object(s):
    [32, 52) 'arr' (line 9) <== Memory access at offset 52 overflows this variable
SUMMARY: AddressSanitizer: stack-buffer-overflow u02_oob.c:11 in stack_oob
```

```text
##### mode=heap
==50820==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x603000001014
WRITE of size 4 at 0x603000001014 thread T0
    #0 0x000102731224 in heap_oob u02_oob.c:20
    #1 0x000102730af8 in main u02_oob.c:47

0x603000001014 is located 0 bytes after 20-byte region [0x603000001000,0x603000001014)
allocated by thread T0 here:
    #0 0x000102f15214 in malloc+0x78
    #1 0x000102730ef4 in heap_oob u02_oob.c:16
SUMMARY: AddressSanitizer: heap-buffer-overflow u02_oob.c:20 in heap_oob
```

```text
##### mode=offbyone
u02_oob.c:31:16: runtime error: index 5 out of bounds for type 'int[5]'
==50823==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x00016b6d1bd4
READ of size 4 at 0x00016b6d1bd4 thread T0
    #0 0x00010472d6a0 in off_by_one u02_oob.c:31
  This frame has 1 object(s):
    [32, 52) 'arr' (line 27) <== Memory access at offset 52 overflows this variable
```

ASan 给出：`arr` 占据 `[32, 52)`，访问的是 offset 52 —— **正好越过尾部 1 个元素**。

**对比表**：

| | 无 sanitizer | ASan |
|---|---|---|
| 栈越界读 | 打印 `1`，exit 0 | 精确报告变量、行号、越界偏移 |
| 堆越界写 | 打印 `99`，exit 0 | 报告「20 字节区域之后 0 字节处」+ 分配点 |
| off-by-one | 打印 `1`，exit 0 | 报告 offset 52 越过 `[32,52)` |

---

## u03 —— use-after-free / double free

### 不加 sanitizer

```text
模式: read
free 前: p=0x1047fd6b0 内容="important data"
free 后读: ""   <-- use-after-free (读)
exit=0
```

字符串变成了空 —— libmalloc 把已释放块的开头拿去做 freelist 指针了。**没有崩溃，只是数据悄悄错了。**

### 加 ASan

```text
##### mode=read
==51165==ERROR: AddressSanitizer: heap-use-after-free on address 0x603000001000
READ of size 2 at 0x603000001000 thread T0
    #2 0x0001043e4a5c in main u03_uaf.c:20

0x603000001000 is located 0 bytes inside of 32-byte region [0x603000001000,0x603000001020)
freed by thread T0 here:
    #1 0x0001043e4a2c in main u03_uaf.c:16
previously allocated by thread T0 here:
    #1 ... u03_uaf.c:13
```

```text
##### mode=write
==51168==ERROR: AddressSanitizer: heap-use-after-free
WRITE of size 11 at 0x603000001000 thread T0
    #0 0x00010149aaa0 in strcpy+0x458
    #1 0x000100c58a88 in main u03_uaf.c:22
```

```text
##### mode=doublefree
==51171==ERROR: AddressSanitizer: attempting double-free on 0x603000001000 in thread T0:
    #1 0x000104150ac4 in main u03_uaf.c:25
freed by thread T0 here:
    #1 0x000104150a2c in main u03_uaf.c:16
```

```text
##### mode=safe
free 前: p=0x603000001000 内容="important data"
置 NULL 之后 p=0x0，任何误用都会立刻变成可见的空指针错误
```

ASan 的三段式报告（**现在在哪出错 / 上次在哪 free / 最初在哪 malloc**）是排查这类 bug 最有力的工具。

**防御**：`free(p); p = NULL;` 一行代码，把「静默的 UAF」变成「立刻可见的空指针崩溃」。

---

## u04 —— 读未初始化的对象

macOS/arm64 上 **MemorySanitizer 不可用**，只能靠编译器警告 + 观察。

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -O0 -g u04_uninit.c -o u04_uninit
```

```text
u04_uninit.c:26:79: warning: variable 'x' is uninitialized when used here [-Wuninitialized]
   26 |     printf("  未初始化的 x = %d ...", x);
      |                                        ^
u04_uninit.c:25:10: note: initialize the variable 'x' to silence this warning
   25 |     int x;
      |          ^
      |           = 0
u04_uninit.c:30:102: warning: variable 'p' is uninitialized when used here [-Wuninitialized]
2 warnings generated.
```

运行：

```text
== 1. 未初始化的局部变量 ==
  未初始化的 x = -453620320   <-- 值不确定，读它就是 UB

== 2. 未初始化的指针（野指针）==
  未初始化的 p = 0x1   <-- 解引用它会发生什么，没人能保证

== 3. 栈上的「垃圾」其实是上一个函数留下的数据 ==
  干净调用: leak_stack_garbage() = 1252948400
  脏栈之后: leak_stack_garbage() = 1717986916
  如果两次结果不同，说明你读到的是别的函数的残留数据
```

**决定性证据**：同一个函数、同样的输入，两次调用返回不同的值（`1252948400` vs `1717986916`）。
第二次的 `1717986916 = 0x66666664` —— 正是前一个函数 `dirty_the_stack` 写进去的 `0x11111111 * 6 = 0x66666666` 附近的残留。

这在真实程序里就是**信息泄漏漏洞**：你读到的可能是上一次请求的密码、密钥、或者指针。

**注意**：`-Wuninitialized` 只能抓到最简单的情况。一旦经过函数调用、数组、循环，编译器就抓不到了（实验里的 `arr[8]` 就没被警告）。所以**定义即初始化**是唯一可靠的办法。

---

## u05 —— 解引用空指针

```text
##### 无 sanitizer, crash 模式
模式: crash
good_sum(&a)  = 3
good_sum(NULL) = 0   <-- 判空之后很安全
即将调用 bad_sum(NULL) —— 解引用空指针
exit=139        # 139 - 128 = 11 = SIGSEGV
```

```text
##### UBSan + ASan
u05_nullderef.c:10:15: runtime error: member access within null pointer of type 'const struct Node'
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior u05_nullderef.c:10:15 
u05_nullderef.c:10:15: runtime error: load of null pointer of type 'const int'
AddressSanitizer:DEADLYSIGNAL
==51581==ERROR: AddressSanitizer: SEGV on unknown address 0x000000000000 (pc 0x00010430d0bc ...)
==51581==The signal is caused by a READ memory access.
```

UBSan 在**崩溃之前**就报告了行号，这比事后看 core dump 方便得多。

代码里注意这一句：

```c
fflush(stdout);              /* 崩溃前把缓冲区刷出来，否则输出会丢 */
```

不加这句，前面的 `printf` 全部丢失（stdout 全缓冲）。这是调试崩溃时的必备技巧。

---

## u06 —— 修改字符串字面量

```text
##### 无 sanitizer, crash 模式
模式: crash
char writable[] = "hello"; writable[0]='H' -> "Hello"  OK
  writable 在栈上: 0x16b4f9d80
char *literal = "hello";  literal 在只读段: 0x104904625
即将执行 literal[0] = 'H'  —— undefined behavior
exit=138        # 138 - 128 = 10 = SIGBUS
```

```text
##### ASan
AddressSanitizer:DEADLYSIGNAL
==51937==ERROR: AddressSanitizer: BUS on unknown address (pc 0x000104f9cccc ...)
==51937==The signal is caused by a WRITE memory access.
    #0 0x000104f9cccc in main u06_literal_write.c:22
```

地址差距说明一切：`writable` 在 `0x16b4f9d80`（栈），`literal` 指向 `0x104904625`（`__TEXT` 只读页）。
往只读页写 → MMU 产生 `SIGBUS`。

**唯一正确的做法**：

```c
const char *s = "hello";       /* 指向字面量：加 const，编译器帮你拦住 */
char buf[] = "hello";          /* 需要可写：让它成为数组（栈上的拷贝） */
char *heap = malloc(6);        /* 或者堆上 */
```

**绝不要**为了消除警告而写 `(char *)"hello"` —— 那是在关掉安全带。

---

## u07 —— 移位、除零、求值顺序

### 移位越界

```text
u07_misc.c:17:19: runtime error: shift exponent 32 is too large for 32-bit type 'int'
u07_misc.c:24:22: runtime error: left shift of negative value -1
模式: shift
  1 << 32 是 UB（移位量必须 < 类型位宽）
  本次得到 1
  -1 << 1 在 C17 里也是 UB（左移有符号负数）
  本次得到 -2
```

`1 << 32` 得到了 **1** —— arm64 的移位指令只取低 5 位，32 → 0，所以 `1 << 0 = 1`。
在 x86 上也是取低 5 位，但在别的架构上可能得到 0。这就是为什么标准不定义它。

**规则**：移位量必须在 `[0, 位宽)` 内。需要位操作时**一律用 unsigned 类型**。

### 除以零

```text
u07_misc.c:35:19: runtime error: division by zero
模式: divzero
  即将计算 1 / 0
  结果 0
```

有意思的是 arm64 的 `sdiv` 指令对除零**返回 0 而不是异常**（x86 的 `idiv` 会触发 `#DE` → `SIGFPE`）。
同一段 UB 代码，x86 上崩溃、arm64 上静默返回 0 —— **这就是 UB 不可移植的活样本**。

### 求值顺序

```text
  i = i++ + ++i;      // UB
  arr[i] = i++;       // UB
  printf("%d %d", i++, i++);  // 参数求值顺序未指定
```

C17 引入了 sequenced-before 模型：在两个 sequence point 之间，对同一个标量对象的修改次数超过一次，或者「修改」和「用于计算新值之外的读」同时发生，就是 UB。

**规则**：一个语句里对同一个变量最多出现一次副作用。拿不准就拆成两行。

### 常见 UB 清单（程序内置）

```text
   1. 有符号整数溢出
   2. 数组/指针越界访问
   3. 解引用空指针 / 野指针 / 悬垂指针
   4. 读取未初始化的对象
   5. free 两次 / free 非堆指针
   6. 修改字符串字面量或 const 对象
   7. 移位量 >= 位宽，或左移负数
   8. 整数除以零 / 取模零
   9. 同一表达式内无序地多次修改同一对象
  10. 违反严格别名规则
  11. memcpy 源和目的重叠（应该用 memmove）
  12. 非 void 函数走到结尾却没 return
  13. printf 格式串与实参类型不匹配
  14. 未对齐的指针解引用
```

---

## u08 —— UB 让优化器删掉你的检查（本章最重要）

### 案例 1：先解引用后判空 —— 判空被整段删除

```c
static int deref_then_check(int *p)
{
    int v = *p;                 /* 如果 p 可能是 NULL，这里就是 UB */
    if (p == NULL) { return -1; }   /* 优化器：既然上面没崩，p 一定非空 */
    return v;
}
```

对比汇编（**真实输出**）：

```bash
cc -std=c17 -S -O0 uopt.c -o -   # 摘取 _deref_then_check
cc -std=c17 -S -O2 uopt.c -o -
```

`-O0`：

```text
_deref_then_check:
	sub	sp, sp, #32
	str	x0, [sp, #16]
	ldr	x8, [sp, #16]
	ldr	w8, [x8]           ; v = *p
	str	w8, [sp, #12]
	ldr	x8, [sp, #16]
	cbnz	x8, LBB0_2         ; ← 判空分支存在
	b	LBB0_1
LBB0_1:
	mov	w8, #-1            ; return -1
	str	w8, [sp, #28]
	b	LBB0_3
LBB0_2:
	ldr	w8, [sp, #12]
	str	w8, [sp, #28]
	b	LBB0_3
LBB0_3:
	ldr	w0, [sp, #28]
	add	sp, sp, #32
	ret
```

`-O2`：

```text
_deref_then_check:
	ldr	w0, [x0]
	ret
```

**整个函数只剩两条指令。`if (p == NULL) return -1;` 彻底消失了。**

### 案例 2：用「溢出后变负」检查溢出 —— 检查失效

```c
static int bad_overflow_check(int a, int b)
{
    int sum = a + b;            /* 溢出时是 UB */
    if (sum < a) { return -1; } /* 你以为这里能拦住 */
    return sum;
}
```

`-O2` 汇编：

```text
_bad_overflow_check:
	add	w8, w1, w0
	cmn	w1, #1            ; 把 b 和 -1 比较
	csinv	w0, w8, wzr, gt   ; b > -1 ? sum : -1
	ret
```

编译器把 `a + b < a` **代数化简**成了 `b < 0`（因为「有符号加法不溢出」是它的公理）。

运行结果（**同一份源码，两个优化级别**）：

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -g -O0 u08_optimizer.c -o u08_O0
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -g -O2 u08_optimizer.c -o u08_O2
./u08_O0 2000000000 2000000000
./u08_O2 2000000000 2000000000
```

| 优化级别 | `bad_overflow_check(2e9, 2e9)` |
|---|---|
| `-O0` | **-1**（检查「生效」了） |
| `-O2` | **-294967296**（检查被删，返回了溢出后的垃圾值） |

完整输出（`-O2`）：

```text
== 案例 2：错误的溢出检查 a=2000000000 b=2000000000 ==
  a + b 的数学真值 = 4000000000（已超出 int 范围）
  bad_overflow_check(a, b) = -294967296
  期望：返回 -1 表示「检测到溢出」。
  实际：-O2 下编译器把 (a+b < a) 化简成了 (b < 0)，检查失效。
```

### 正确写法

```text
== 正确做法 ==
  __builtin_add_overflow 正确报告了溢出
  可移植版本也正确报告了溢出
```

```c
/* 方案 A：编译器内建 */
if (__builtin_add_overflow(a, b, out)) { return -1; }

/* 方案 B：可移植，加法前判断 */
if (b > 0 && a > INT_MAX - b) { return -1; }
if (b < 0 && a < INT_MIN - b) { return -1; }
*out = a + b;
```

### 结论

> **UB 不是「结果不确定」，而是「整个程序的行为都不再有任何约束」。**
> 优化器把「程序不含 UB」当成公理来推理，所以 UB 能让远处的代码消失。
> 永远不要用 UB 来做检查，要在触发 UB **之前**就拦住它。

这也解释了一个常见现象：「我的程序 debug 版好好的，release 版就崩」。
这通常不是编译器的 bug，而是你的代码里有 UB。

---

## 调试工具实操

### AddressSanitizer / UndefinedBehaviorSanitizer

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -O0 -g \
   -fsanitize=address,undefined -fno-omit-frame-pointer \
   prog.c -o prog_san
./prog_san
```

常用环境变量：

```bash
ASAN_OPTIONS=detect_stack_use_after_return=1 ./prog_san   # 抓栈上的 UAF
ASAN_OPTIONS=halt_on_error=0 ./prog_san                   # 报错后继续跑
UBSAN_OPTIONS=print_stacktrace=1 ./prog_san               # UBSan 也打调用栈
UBSAN_OPTIONS=halt_on_error=1 ./prog_san                  # UBSan 第一个错误就停
```

代价：内存约 3 倍，速度约 2 倍慢。**只在测试/CI 用，不要发布。**

### gdb（本机已安装，`/opt/homebrew/bin/gdb`）

```bash
gdb ./prog
(gdb) break main
(gdb) run
(gdb) next / step
(gdb) print var
(gdb) print *ptr
(gdb) print arr[0]@5      # 打印数组前 5 个元素
(gdb) x/16xb &obj         # 以字节形式查看内存
(gdb) backtrace
(gdb) info locals
(gdb) watch x             # x 被修改时断下
```

macOS 上 gdb 需要代码签名，第一次用可能要 `codesign`。lldb 开箱即用：

```bash
lldb ./prog
(lldb) b main
(lldb) r
(lldb) p var
(lldb) memory read -f x -s 1 -c 16 &obj
(lldb) bt
```

### Valgrind

**本机不可用**（Apple Silicon 不支持）。在 Linux/x86 上：

```bash
valgrind --leak-check=full --track-origins=yes ./prog
```

### 本机可用工具总表

| 工具 | 状态 | 能抓什么 |
|---|:---:|---|
| ASan | ✅ | 堆/栈/全局越界、UAF、double free、内存泄漏（非 macOS） |
| UBSan | ✅ | 有符号溢出、移位、除零、空指针、对齐、数组下标 |
| LSan | ❌ | macOS arm64 不支持 |
| MSan | ❌ | 仅 Linux/x86-64 |
| TSan | ✅ | 数据竞争（`-fsanitize=thread`，不能和 ASan 同用） |
| Valgrind | ❌ | Apple Silicon 不支持 |
| `leaks --atExit` | ✅ | 内存泄漏（**不能**配合 ASan） |
| gdb / lldb | ✅ | 交互调试 |
| 编译器警告 | ✅ | 很多问题在编译期就能发现 —— **最便宜的工具** |

---

## 全章最佳实践

1. **开发和 CI 必开 `-Wall -Wextra -Wpedantic -Werror`。** 警告是最便宜的 UB 检测器。
2. 测试阶段必跑 `-fsanitize=address,undefined`。
3. 定义即初始化；`free` 后置 NULL；数组传参必带长度。
4. 不要用 UB 做检查（溢出检查、空指针检查都要**事前**做）。
5. 需要回绕语义就用 `unsigned`；需要位操作也用 `unsigned`。
6. 崩溃调试时在关键点 `fflush(stdout)` 或直接用 `stderr`。
7. 「debug 能跑，release 崩」几乎总是自己代码里有 UB。
8. 记住退出码：`128+N` 表示被信号 N 杀死。`139`=SIGSEGV，`138`=SIGBUS，`134`=SIGABRT，`133`=SIGTRAP。

## 练习

1. 把 `u02_oob.c` 的 `int idx = 5;` 改成字面量 `arr[5]`，看 `-Warray-bounds` 能不能在编译期抓到。
2. 用 `__builtin_mul_overflow` 改写 `07_memory` 里的分配大小检查。
3. 找出下面这段代码的 UB 并修复：
   ```c
   char *get_name(void) { char buf[32]; sprintf(buf, "user%d", id); return buf; }
   ```
4. 在 `-O0` 和 `-O2` 下分别跑 `u08_optimizer`，用 `objdump -d` 对比两份汇编。
