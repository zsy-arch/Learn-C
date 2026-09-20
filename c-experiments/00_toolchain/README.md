# 00_toolchain —— 从源码到可执行文件

## 实验目的

亲眼看到 C 编译的四个阶段（预处理 / 编译 / 汇编 / 链接），以及「声明 vs 定义」在链接期意味着什么。

## 文件

| 文件 | 作用 |
|---|---|
| `hello.c` | 单文件程序，用来拆解四个阶段 |
| `greet.h` / `greet.c` / `main.c` | 多文件工程，演示头文件、include guard、外部链接 |
| `link_error.c` | 反例：只有声明没有定义，编译过、链接挂 |

## 环境

```text
Darwin 27.0.0 arm64 (Apple Silicon)
Apple clang version 21.0.0 (clang-2100.3.30.1)
```

## 阶段 1：预处理 `-E`

```bash
cc -std=c17 -E hello.c -o hello.i
wc -l hello.i
tail -12 hello.i
```

```text
hello.i 行数:      579
# 3 "hello.c" 2

int main(void)
{
    puts("Hello, C17!");
    printf("SQUARE(3) = %d\n", ((3) * (3)));
    printf("__STDC_VERSION__ = %ldL\n", 201710L);
    return 0;
}
```

**结论**：
- 12 行源码被 `#include <stdio.h>` 撑成了 579 行。
- 宏是**纯文本替换**：`SQUARE(3)` 变成了 `((3) * (3))`，`GREETING` 变成了字面量。
- `__STDC_VERSION__` 展开成 `201710L`，确认当前是 C17。

## 阶段 2：编译 `-S`（C → 汇编）

```bash
cc -std=c17 -S -O0 hello.c -o hello.s
sed -n '/^_main:/,/^$/p' hello.s | head -20
```

```text
_main:                                  ; @main
	sub	sp, sp, #32
	stp	x29, x30, [sp, #16]             ; 16-byte Folded Spill
	add	x29, sp, #16
	mov	w8, #0                          ; =0x0
	str	w8, [sp, #8]
	stur	wzr, [x29, #-4]
	adrp	x0, l_.str@PAGE
	add	x0, x0, l_.str@PAGEOFF
	bl	_puts
	mov	x9, sp
	mov	x8, #9                          ; =0x9
	str	x8, [x9]
	adrp	x0, l_.str.1@PAGE
	add	x0, x0, l_.str.1@PAGEOFF
```

**结论**：
- `sub sp, sp, #32` 就是「开栈帧」，`stp x29, x30` 保存帧指针和返回地址。
- **`mov x8, #9`**：`SQUARE(3)` 的乘法在编译期就算完了，运行时没有乘法指令。即使 `-O0` 也会做常量折叠。
- 字符串通过 `adrp/add` 的 PC 相对寻址取地址，这是 arm64 的标准做法。

## 阶段 3：汇编 `-c`（汇编 → 目标文件）

```bash
cc -std=c17 -c hello.c -o hello.o
file hello.o
nm hello.o
```

```text
hello.o: Mach-O 64-bit object arm64
0000000000000000 T _main
                 U _printf
                 U _puts
0000000000000068 s l_.str
```

**结论**：`T` = 本文件定义的全局符号，`U` = **undefined**，等着链接器去别处找。

## 阶段 4：链接

```bash
cc hello.o -o hello_full
./hello_full
```

```text
Hello, C17!
SQUARE(3) = 9
__STDC_VERSION__ = 201710L
```

## 多文件工程

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g -c main.c  -o main.o
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g -c greet.c -o greet.o
cc main.o greet.o -o app
./app
```

```text
Hello, Alice! (第 1 次问候)
Hello, Bob! (第 2 次问候)
g_greet_count = 2
```

符号表：

```text
### nm main.o
                 U _g_greet_count     <- 需要外部提供
                 U _greet             <- 需要外部提供
0000000000000000 T _main

### nm greet.o
0000000000000488 S _g_greet_count     <- 这里定义
0000000000000000 T _greet             <- 这里定义
```

**结论**：`main.c` 里 `#include "greet.h"` 写了两次也没出问题，因为 include guard 生效了。头文件只提供**声明**，真正的**定义**在 `greet.c`，两者在链接期才配对。

## 反例：缺少定义

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -O0 -g link_error.c -o link_error
```

```text
Undefined symbols for architecture arm64:
  "_missing_function", referenced from:
      _main in link_error-a45e62.o
ld: symbol(s) not found for architecture arm64
clang: error: linker command failed with exit code 1
```

**现象 → 原因 → 修复**

| | |
|---|---|
| 现象 | 编译没报错，链接报 `Undefined symbols` |
| 原因 | `int missing_function(int);` 只是**承诺**它存在，没人兑现 |
| 修复 | 提供定义，或者链接上包含定义的 `.o` / 库 |

## 本章要点

1. `声明` 告诉编译器「类型长什么样」；`定义` 才真正产生代码或分配存储。
2. 头文件里放声明，`.c` 里放定义；每个头文件都要有 include guard。
3. 「编译错误」和「链接错误」是两个完全不同的阶段，错误信息长得也完全不同。
4. 想看编译器到底干了什么，`-E` / `-S` / `nm` / `objdump -d` 四件套足够用。
