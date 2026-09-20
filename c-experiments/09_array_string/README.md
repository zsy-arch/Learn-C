# 09_array_string —— 数组与字符串

## 文件

| 文件 | 验证什么 |
|---|---|
| `a01_arrays.c` | 初始化、多维数组、行优先、VLA |
| `a02_strings.c` | `'\0'` 终止、`strcpy`/`strncpy`/`snprintf`、`memcpy` vs `memmove` |
| `a03_overflow.c` | 反例：栈缓冲区溢出（含 fortify / stack protector / ASan 三种表现） |

---

## a01 —— 数组

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g a01_arrays.c -o a01_arrays && ./a01_arrays
```

```text
== 1. 初始化的几种写法 ==
  int a1[5]={1,2,3,4,5} -> [ 1,  2,  3,  4,  5]
  int a2[5]={1,2}       -> [ 1,  2,  0,  0,  0]   <-- 后面补 0
  int a3[5]={0}         -> [ 0,  0,  0,  0,  0]
  int a4[]={1,2,3}      -> [ 1,  2,  3]   长度 = 3（编译器数出来的）
  int a5[5]={[4]=9,[0]=1} -> [ 1,  0,  0,  0,  9]

== 2. 数组不能整体赋值，也不能整体比较 ==
  memcpy(b, a1, sizeof a1) -> [ 1,  2,  3,  4,  5]
  memcmp(b, a1, sizeof a1) = 0 (0 表示逐字节相同)

== 3. 二维数组是「数组的数组」，内存里是行优先连续的 ==
  sizeof(m)    = 48  (3*4*4)
  sizeof(m[0]) = 16  (一行 4 个 int)
  sizeof(m[0][0]) = 4
  行数 = 3, 列数 = 4
  内存中的实际顺序: 1 2 3 4 5 6 7 8 9 10 11 12   <-- 行优先(row-major)
  &m[0][0]=0x16ba69cd0
  &m[1][0]=0x16ba69ce0  差 16 字节 = 一整行
  sum2d(m, 3) = 78

== 4. m[i][j] 的地址计算 ==
  m[i][j] 等价于 *(*(m + i) + j)
  地址 = (char*)m + (i * 列数 + j) * sizeof(元素)
  m[2][1] = 10，手算地址 = 0x16ba69cf4，实际地址 = 0x16ba69cf4

== 5. VLA（变长数组，C99 引入，C11 起为可选特性）==
  int vla[n] (n=4): [ 0,  1,  4,  9]   sizeof(vla) = 16（运行时计算）

== 6. LEN 宏的陷阱 ==
  在 main 里 LEN(a1) = 5  正确
  一旦把 a1 传给函数，函数里 LEN(参数) = 8/4 = 2，完全错误！
```

### 二维数组的内存布局

`int m[3][4]` 在内存里是 **48 个连续字节**，行优先：

```text
偏移:  0    4    8   12   16   20   24   28   32   36   40   44
      ┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┐
      │  1 │  2 │  3 │  4 │  5 │  6 │  7 │  8 │  9 │ 10 │ 11 │ 12 │
      └────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┘
       └────── m[0] ─────┘└────── m[1] ─────┘└────── m[2] ─────┘

&m[0][0] = 0x16ba69cd0
&m[1][0] = 0x16ba69ce0   (+16 = 一整行)
&m[2][1] = 0x16ba69cf4   (= base + (2*4 + 1) * 4 = base + 36)
```

手算地址和实际地址完全一致，验证了公式 `addr = base + (i * cols + j) * sizeof(elem)`。

**为什么二维数组作参数必须写列数**：
`void f(int m[][4])` 里那个 `4` 是**必需**的，因为编译器要靠它算 `m[i][j]` 的偏移。
第一维可以省略，因为 `m` 退化成了 `int (*)[4]`，第一维本来就不在类型里。

### VLA 的三个注意点

1. 在**栈**上分配，`n` 很大时直接栈溢出（没有任何错误提示，直接段错误）。
2. MSVC 完全不支持。
3. C11 起是**可选特性**，实现可以定义 `__STDC_NO_VLA__` 表示不支持。

生产代码里建议用 `malloc` 代替 VLA。

### 数组 vs 结构体的不对称

| 操作 | 裸数组 | 含数组成员的结构体 |
|---|:---:|:---:|
| 整体赋值 `a = b` | ❌ 编译错误 | ✅ 可以 |
| 作参数按值传递 | ❌ 退化成指针 | ✅ 整体拷贝 |
| 作返回值 | ❌ 不允许 | ✅ 可以 |
| `sizeof` | ✅ 真实大小 | ✅ 真实大小 |

**技巧**：想让数组能整体赋值/传值，把它包进一个结构体。

---

## a02 —— 字符串

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g a02_strings.c -o a02_strings && ./a02_strings
```

```text
== 1. C 字符串 = 以 '\0' 结尾的 char 数组 ==
  char s[] = "abc";  sizeof=4  strlen=3
  字节: 'a'(0x61) 'b'(0x62) 'c'(0x63) '.'(0x00) 

== 2. 没有 '\0' 就不是字符串 ==
  char notstr[3] = {'a','b','c'};  对它调用 strlen 会一直往后读 —— UB
  想安全地打印，用 %.*s 限定长度: "abc"

== 3. strcpy 是缓冲区溢出的头号来源 ==
  char small[8];  strcpy(small, "1234567") 需要 8 字节 —— 刚好装下
  strcpy 之后    "1234567"  strlen=7  buf=8

== 4. snprintf 是最推荐的安全写法 ==
  snprintf(dst, 8, "%s", "0123456789")
    返回值 = 10  <-- 这是「本来需要的长度」，不是实际写入的长度
  dst              "0123456"  strlen=7  buf=8
    截断判断: if (need < 0 || (size_t)need >= sizeof dst) -> 发生了截断
    snprintf 永远保证以 '\0' 结尾（只要 size > 0）

== 5. strncpy 不是安全版 strcpy！==
  strncpy(t1, "0123456789", 8) 之后 t1 没有 '\0' 结尾！
  前 8 字节: 01234567
  必须自己补: t1[sizeof t1 - 1] = '\0';
  补 \0 之后    "0123456"  strlen=7  buf=8
  另一个坑：源串短时 strncpy 会把剩余空间全填 '\0'（性能浪费）
  t2 的 8 个字节: 61 62 00 00 00 00 00 00 

== 6. 常用函数速查 ==
  strlen("hello world")        = 11
  strchr(a, 'o') - a     = 4  (第一个 'o' 的下标)
  strrchr(a, 'o') - a    = 7  (最后一个 'o')
  strstr(a, "wor") - a   = 6
  strcmp("abc","abd")    = -1  (<0 表示前者小)
  strncmp("abc","abd",2) = 0  (只比前 2 个字符)

== 7. memcpy vs memmove ==
  原始:                0123456789
  memmove(buf+2,buf,5): 0101234789   <-- 重叠时安全
  memcpy 的前提是「源和目的不重叠」，重叠就是 UB

== 8. 动态字符串 ==
  malloc 版本    "dynamic string"  strlen=14  buf=15
```

### `snprintf` 返回值的正确用法

```c
int need = snprintf(dst, dstsize, fmt, ...);
if (need < 0) {
    /* 编码错误 */
} else if ((size_t)need >= dstsize) {
    /* 发生了截断：need 是「不截断的话需要多少字符」 */
}
```

实验里 `snprintf(dst, 8, "%s", "0123456789")` 返回 **10**（源串长度），而不是 7（实际写入数）。
这个设计很有用：你可以先用 `snprintf(NULL, 0, fmt, ...)` 问出需要多大的缓冲区。

### 为什么 `strncpy` 不是「安全版 strcpy」

`strncpy` 的原始设计目的是给**定长字段**（比如老式文件系统的 14 字节文件名）用的，不是为了防溢出：

| 情况 | `strncpy(dst, src, n)` 的行为 |
|---|---|
| `strlen(src) >= n` | 拷贝 n 个字符，**不加 `'\0'`** ← 危险 |
| `strlen(src) < n` | 拷贝后把剩余空间**全填 `'\0'`** ← 浪费 |

实验输出直接验证了两点：
- `t1` 被填满 8 个字符 `01234567`，没有终止符
- `t2` 内容是 `61 62 00 00 00 00 00 00` —— `"ab"` 之后全是 0

**结论**：把 `strncpy` 忘掉。用 `snprintf`，或者自己写 `strlcpy` 语义的函数（见 `10_project/sstr.c`）。

### `memcpy` vs `memmove`

```text
  原始:                 0123456789
  memmove(buf+2,buf,5): 0101234789
```

`memcpy` 的 contract 是「源和目的不重叠」，重叠就是 UB（编译器可能用向量指令乱序拷贝）。
`memmove` 保证正确处理重叠，代价只是多一次方向判断。**拿不准就用 `memmove`。**

### 黑名单 / 白名单

| ❌ 不要用 | ✅ 用这个 | 理由 |
|---|---|---|
| `gets` | `fgets` | `gets` 无法限制长度，C11 已删除 |
| `strcpy` | `snprintf` / `memcpy`+长度检查 | 无边界检查 |
| `strcat` | `snprintf` | 同上 |
| `sprintf` | `snprintf` | 同上 |
| `strncpy` | `snprintf` | 不保证 `'\0'` 结尾 |
| `atoi` | `strtol` | 无法区分「0」和「解析失败」 |

---

## a03 —— 缓冲区溢出（四种表现）

这个实验在本机呈现了**四种完全不同的现象**，非常有教学价值。

### 实验代码结构

```c
struct Frame {
    char buf[8];
    int  canary;      /* 放在 buf 后面，用来观察溢出踩到谁 */
};
```

### 表现 1：安全版本（`snprintf`）

```bash
./a03_overflow safe "0123456789ABCDEF"
```

```text
模式: safe   源串: "0123456789ABCDEF" (16 字符)  目标 buf 只有 8 字节
== 安全版本 snprintf ==
  buf="0123456"  canary=0x11223344 (完好)  [发生截断]
```

截断了，但 canary 完好，程序继续正常运行。

### 表现 2：默认编译 —— macOS FORTIFY 直接中止

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g a03_overflow.c -o a03_overflow
./a03_overflow unsafe "0123456789"
```

```text
Trace/BPT trap: 5
exit=133      # 133 - 128 = 5 = SIGTRAP
```

**为什么？** 看符号表：

```bash
$ nm -u ./a03_overflow
___snprintf_chk
___stack_chk_fail
___stack_chk_guard
___strcpy_chk         ← 注意这个
_printf
_puts
_strlen
```

Apple SDK 的头文件默认开启了 `_FORTIFY_SOURCE`，把 `strcpy` 换成了 `__strcpy_chk`。
它在运行时知道目标缓冲区大小（编译期算出来的 8），发现要写 11 字节就直接中止。

**注意：stdout 的输出全部丢失了**（缓冲区没刷出来）。

### 表现 3：关掉 FORTIFY —— 静默破坏（最危险）

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -O0 -g \
   -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 a03_overflow.c -o a03_nofortify
nm -u ./a03_nofortify | grep strcpy      # 现在是普通 _strcpy
./a03_nofortify unsafe "0123456789"
```

```text
模式: unsafe   源串: "0123456789" (10 字符)  目标 buf 只有 8 字节
== 不安全版本 strcpy ==
  buf="0123456789"  canary=0x11003938 (被踩坏了！)
  即使看起来「没崩」，相邻内存也已经被破坏了
exit=0
```

**看 canary 的变化**：`0x11223344` → `0x11003938`
- `0x38` = `'8'`
- `0x39` = `'9'`
- `0x00` = `'\0'`

正好是溢出的 3 个字节（`'8'`, `'9'`, `'\0'`）按 little-endian 覆盖了 canary 的低 3 字节。
**程序退出码 0，看起来一切正常，但数据已经被静默破坏了。**

### 表现 4：溢出更多 —— stack protector 抓住

```bash
./a03_nofortify unsafe "0123456789ABCDEF"
```

```text
Abort trap: 6
exit=134      # 134 - 128 = 6 = SIGABRT
```

溢出 9 字节，踩到了编译器插入的 stack canary（`__stack_chk_guard`），函数返回前检查失败 → `__stack_chk_fail` → `abort()`。

### 表现 5：ASan —— 精确定位

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -O0 -g \
   -fsanitize=address,undefined -fno-omit-frame-pointer \
   a03_overflow.c -o a03_san
./a03_san unsafe "0123456789ABCDEF"
```

```text
=================================================================
==56151==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x00016dca5bec
WRITE of size 17 at 0x00016dca5bec thread T0
    #0 0x000102a12aa0 in strcpy+0x458
    #1 0x000102158d54 in unsafe_copy a03_overflow.c:14
    #2 0x000102158bbc in main a03_overflow.c:41

Address 0x00016dca5bec is located in stack of thread T0 at offset 44 in frame
    #0 0x000102158c08 in unsafe_copy a03_overflow.c:11

  This frame has 1 object(s):
    [32, 44) 'f' (line 12) <== Memory access at offset 44 overflows this variable
SUMMARY: AddressSanitizer: stack-buffer-overflow a03_overflow.c:14 in unsafe_copy
```

### 五种表现总结

| 编译/运行方式 | 现象 | 退出码 | 能定位吗 |
|---|---|---|:---:|
| `snprintf` 安全版 | 截断，canary 完好 | 0 | 不需要 |
| 默认（FORTIFY 开） | SIGTRAP，无输出 | 133 | ❌ 只知道崩了 |
| FORTIFY 关，溢出 2B | **静默破坏 canary** | 0 | ❌ 完全看不出 |
| FORTIFY 关，溢出 9B | SIGABRT（stack protector） | 134 | ❌ |
| ASan | 精确报告文件、行号、越界偏移 | 1 | ✅ |

**这就是为什么必须用 sanitizer**：同一个 bug，在不同编译配置下有五种截然不同的表现，其中最危险的那种（静默破坏）退出码还是 0。

---

## 全章最佳实践

1. 数组传参必带长度；`LEN` 宏只能在定义处用。
2. 二维数组作参数时列数必写；或者干脆传一维 + 自己算下标。
3. 生产代码用 `malloc` 代替 VLA。
4. 字符串缓冲区分配时**永远记得 +1** 给 `'\0'`。
5. 首选 `snprintf`，并检查返回值判断截断。
6. 忘掉 `strcpy` / `strcat` / `sprintf` / `strncpy` / `gets`。
7. 拿不准重叠就用 `memmove`。
8. 需要二进制安全（可能含 `'\0'`）就自己带长度，别依赖终止符。
9. macOS 默认开 FORTIFY，别以为「没崩就是对的」—— 换个平台可能就静默破坏了。

## 练习

1. 实现 `size_t my_strlcpy(char *dst, const char *src, size_t dstsize)`，语义与 BSD `strlcpy` 一致（答案见 `10_project/sstr.c` 的 `sstr_copy`）。
2. 写一个函数把 `int m[3][4]` 转置成 `int t[4][3]`，注意二维数组参数怎么写。
3. 用 `%.*s` 安全地打印一个「不以 `'\0'` 结尾的字符数组」。
4. 在 `a03_overflow.c` 里把 `canary` 改成一个函数指针，观察溢出后调用它会发生什么（**只在 ASan 下做**）。
