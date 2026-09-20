# 02_branch —— 分支

## 实验目的

搞清楚短路求值到底跳过了什么、悬空 else 绑定给谁、switch 的 fallthrough 有多容易写错。

## 文件

| 文件 | 作用 |
|---|---|
| `demo.c` | 正确用法，`-Werror` 干净通过 |
| `pitfall.c` | 三个反例：悬空 else、意外 fallthrough、`=` 写成 `==` |

## 编译与运行

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g demo.c -o demo
./demo
```

## 真实输出（节选）

```text
== 2. 短路求值：&& 左边为假，右边根本不执行 ==
  表达式: probe("A",0) && probe("B",1)
    [求值] A -> 0
  结果 = 0（注意 B 没有被求值）
  表达式: probe("C",1) || probe("D",1)
    [求值] C -> 1
  结果 = 1（注意 D 没有被求值）

== 3. 短路求值最重要的用途：先判空指针，再解引用 ==
  p 是 NULL，&& 短路保护了 *p，没有崩溃
  p 非空且 *p == 99

== 5. switch：有意的 fallthrough（合并 case）==
  'a' 是元音
  'b' 是辅音
  'c' 是辅音
  'd' 是辅音
  'e' 是元音

== 7. 枚举状态机 ==
  LIGHT_COUNT = 3（枚举成员默认从 0 递增）
  step 0: RED    -> GREEN
  step 1: GREEN  -> YELLOW
  step 2: YELLOW -> RED
  step 3: RED    -> GREEN
  step 4: GREEN  -> YELLOW
  step 5: YELLOW -> RED
```

退出码 `0`。

## 反例实验

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -Wimplicit-fallthrough -O0 -g pitfall.c -o pitfall
./pitfall
```

编译警告（**真实输出**）：

```text
pitfall.c:11:5: warning: add explicit braces to avoid dangling else [-Wdangling-else]
   11 |     else
      |     ^
pitfall.c:21:5: warning: unannotated fall-through between switch labels [-Wimplicit-fallthrough]
   21 |     case 2:
      |     ^
pitfall.c:21:5: note: insert '__attribute__((fallthrough));' to silence this warning
pitfall.c:21:5: note: insert 'break;' to avoid fall-through
2 warnings generated.
```

运行输出：

```text
== 悬空 else ==
 dangling_else(1, -1):
  <-- 这行真正的含义是: a>0 但 b<=0
 dangling_else(-1, 1): (什么都不打印)

== 意外 fallthrough ==
 accidental_fallthrough(1):
  case 1 执行
  case 2 也被执行了（fallthrough）

== 把 == 写成 = ==
  if (x = 5) 恒为真，而且把 x 改成了 5
```

## 机制剖析

### 悬空 else

源码（缩进在骗人）：

```c
if (a > 0)
    if (b > 0)
        printf("a>0 且 b>0\n");
else                           // 看起来配 if(a>0)
    printf("...\n");
```

编译器实际看到的是：

```c
if (a > 0) {
    if (b > 0) {
        printf("a>0 且 b>0\n");
    } else {                   // else 绑定「最近的未配对 if」
        printf("...\n");
    }
}
```

所以 `dangling_else(-1, 1)` 什么都不打印（外层 if 就没进去），而 `dangling_else(1, -1)` 打印了 else 分支。

### 短路求值的求值顺序

`&&` 和 `||` 是 C 里少数几个**规定了求值顺序**的运算符，并且在左右操作数之间有一个 sequence point：

```text
A && B   ->  先算 A；A 为假就直接得 0，B 完全不求值
A || B   ->  先算 A；A 为真就直接得 1，B 完全不求值
```

这就是 `if (p != NULL && p->x == 1)` 安全、而 `if (p->x == 1 && p != NULL)` 会崩的原因。

注意：`&`、`|`、`,`（逗号运算符除外）、函数参数**都不保证顺序**。

## 本章结论

| 坑 | 后果 | 防御 |
|---|---|---|
| 悬空 else | 逻辑完全跑偏 | 永远写 `{}` |
| 意外 fallthrough | 多执行一段代码 | 每个 case 都写 `break`；有意穿透加 `__attribute__((fallthrough))` 或注释 |
| `if (x = 5)` | 恒为真且改了值 | 开 `-Wparentheses`（`-Wall` 内含）；故意赋值时加双层括号 |
| 先解引用后判空 | 段错误 | 用 `&&` 把判空放前面 |
| switch 漏 default | 新增枚举值时静默漏处理 | `switch` 枚举时**不写 default**，让 `-Wswitch` 提醒你补 case |

## 最佳实践

1. `if`/`else`/`for`/`while` 的body 一律加 `{}`，哪怕只有一行。
2. `switch` 覆盖枚举时**不要写 `default`**：漏了成员编译器会警告；写了 `default` 就永远收不到这个提醒。
3. 用 `LIGHT_COUNT` 这种「哨兵成员」放在枚举末尾，自动得到枚举个数。
4. 复杂条件抽成命名良好的 `static` 函数或中间变量，别堆成一行。
5. 常量写在比较式左边（`if (5 == x)`）可以防手滑，但现代编译器有警告，可读性优先。

## 练习

1. 把 `pitfall.c` 的 `dangling_else` 加上大括号，重新编译，确认警告消失且行为符合直觉。
2. 在 `demo.c` 的枚举里加一个 `LIGHT_BLINK`，**不改** `light_name`，编译看看 `-Wswitch` 报什么。
3. 用 `switch` + 函数指针表分别实现一个计算器，比较两者可读性（提示：见 `05_pointer/p06_funcptr.c`）。
