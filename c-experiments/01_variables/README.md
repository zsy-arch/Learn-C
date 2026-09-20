# 01_variables —— 变量、类型、存储期

## 实验目的

用 `sizeof` / `<limits.h>` 把本机的类型模型量出来，并亲手撞一次「有符号 vs 无符号」的墙。

## 文件

| 文件 | 作用 |
|---|---|
| `demo.c` | 主实验，`-Werror` 干净通过 |
| `signcmp.c` | 反例：有符号/无符号混合比较，用来观察 `-Wsign-compare` |

## 编译与运行

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g demo.c -o demo
./demo
```

## 真实输出

```text
== 1. sizeof 基本类型（单位：字节）==
  char=1  short=2  int=4  long=8  long long=8
  float=4 double=8 long double=8 _Bool=1
  void*=8  size_t=8  ptrdiff_t=8  intptr_t=8
  CHAR_BIT = 8  (一个 char 有几个 bit)

== 2. 取值范围 ==
  INT_MIN=-2147483648  INT_MAX=2147483647
  UINT_MAX=4294967295
  LONG_MIN=-9223372036854775808 LONG_MAX=9223372036854775807
  CHAR_MIN=-128 CHAR_MAX=127  -> 本平台 char 是有符号的
  DBL_DIG=15 FLT_DIG=6

== 3. 字面量的类型与进制 ==
  10=10  010(八进制)=8  0x10(十六进制)=16  0b? C17 无二进制字面量
  sizeof('A') = 4  <-- C 里字符字面量是 int，不是 char！
  sizeof("abc") = 4 <-- 3 个字符 + 1 个 '\0'
  1/2 = 0   (整数除法，截断)
  1.0/2 = 0.5 (有一个操作数是 double，整体升成 double)
  7 % 3 = 1   -7 % 3 = -1 (C99 起商向零截断)

== 4. 无符号回绕是「有定义」的，有符号溢出是 UB ==
  UINT_MAX + 1 = 0   <-- 模 2^32 回绕，标准明确规定
  (unsigned char)250 + 10 = 4  <-- 模 2^8 回绕

== 5. 整数提升与「有符号 vs 无符号」陷阱 ==
  int i = -1;  unsigned v = 1u;
  (unsigned)i = 4294967295  <-- -1 的补码被当成巨大的正数
  i < v 的真实结果 = false  <-- 直觉上 -1 < 1 应该为真！
  原因：usual arithmetic conversions 把 int 转成了 unsigned int

== 6. 作用域与存储期 ==
  外层 shadow = 1
  内层块里可以再定义变量 shadow_inner = 2
  离开内层块后 shadow 仍是 1
    counter(): static calls=1, automatic=1
    counter(): static calls=2, automatic=1
    counter(): static calls=3, automatic=1

== 7. 未初始化 vs 已初始化 ==
  静态变量 g_zero_init = 0  (标准保证清零)

== 8. const / static / extern ==
  const int ci = 10  <-- 只读对象，改它是 UB
  g_internal(static, 内部链接) = 42
  g_external(外部链接)         = 7

== 9. bool 与真值 ==
  0.1 + 0.2 == 0.3 ? false   <-- 浮点不能用 == 比较
  0.1 + 0.2 = 0.30000000000000004441
  fabs 差值判断才对：|(0.1+0.2)-0.3| = 0.00000000000000005551
```

退出码 `0`。

## 反例：`-Wsign-compare`

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -O0 -g signcmp.c -o signcmp   # 注意：去掉了 -Werror
./signcmp
```

编译输出：

```text
signcmp.c:10:11: warning: comparison of integers of different signs: 'int' and 'unsigned int' [-Wsign-compare]
   10 |     if (i < u) {
      |         ~ ^ ~
1 warning generated.
```

运行输出：

```text
i < u  不成立  <-- 反直觉，因为 i 被转成了 4294967295
s[0]=a
s[1]=b
s[2]=c
```

## 机制剖析：为什么 `-1 < 1u` 是假

C 的 **usual arithmetic conversions** 规则：当 `int` 和 `unsigned int` 参与同一个二元运算，且两者 rank 相同时，`int` 会被转换成 `unsigned int`。

```text
int i = -1   补码表示: 1111 1111 1111 1111 1111 1111 1111 1111
转成 unsigned:         = 4294967295
4294967295 < 1  ->  false
```

## 本章结论

1. 本机（arm64 macOS）：`int`=4，`long`=8，指针=8，`char` 有符号，little-endian。
2. `sizeof('A') == 4`：C 里字符字面量的类型是 `int`（这点和 C++ 不同）。
3. **无符号回绕有定义，有符号溢出是 UB**。
4. 只要一个表达式里同时出现有符号和无符号，就先停下来想一遍转换规则。
5. 局部变量不会自动清零，静态/全局变量会。
6. 浮点相等比较永远用「差值 < 容差」。

## 最佳实践

- 表示「个数 / 长度 / 下标」时用 `size_t`，并且**全程保持无符号**，不要中途混进 `int`。
- 打印 `size_t` 用 `%zu`，`ptrdiff_t` 用 `%td`，别用 `%d` 蒙混。
- 需要精确宽度就用 `<stdint.h>` 的 `int32_t` / `uint64_t`。
- 定义变量时就初始化，不要「先声明，后面再赋值」。
- 打开 `-Wsign-compare`（`-Wall` 已包含）并把它当错误处理。

## 练习

1. 把 `demo.c` 里 `unsigned char uc = 250; uc = uc + 10;` 改成 `signed char`，重新编译运行，看看 UBSan 会不会报警。
2. 写一个程序验证：`(char)-1` 转成 `int` 后是 `-1`，但 `(unsigned char)-1` 转成 `int` 后是 `255`。
3. 为什么 `for (size_t i = n - 1; i >= 0; i--)` 是死循环？答案在 `03_loop`。
