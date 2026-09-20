# 05_pointer —— 指针（本项目的核心章节）

## 实验目的

把「指针」这个新手最大的坎拆成 10 个独立实验，每个只验证一个点。

| 文件 | 验证什么 | 严格编译 |
|---|---|---|
| `p01_basics.c` | 指针的本质：变量 / 地址 / 类型 | ✅ `-Werror` |
| `p02_decay.c` | 数组退化，`sizeof(arr)` vs `sizeof(ptr)` | ⚠️ 见下文 |
| `p03_strings.c` | `char *s = "abc"` vs `char s[] = "abc"` | ✅ |
| `p04_swap.c` | 为什么 `swap` 不生效 | ✅ |
| `p05_multilevel.c` | 多级指针 `int**` 的真实用途 | ✅ |
| `p06_funcptr.c` | 函数指针与回调 | ✅ |
| `p07_const.c` | `const int*` / `int* const` / `const int* const` | ✅ |
| `p08_void_null.c` | `void*`、NULL、野指针、悬垂指针 | ✅ |
| `p09_arith.c` | 指针运算、one-past-the-end | ✅ |
| `p10_aliasing.c` | 严格别名规则（**-O0 与 -O2 结果不同！**） | ✅ |

## 统一编译命令

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g pXX_xxx.c -o pXX_xxx
./pXX_xxx
```

---

## p01 —— 指针的本质

```text
== 1. 变量住在内存里，& 取出它的门牌号 ==
  n  的值   = 0x11223344
  n  的地址 = 0x16cf51db8        <-- &n
  p  的值   = 0x16cf51db8        <-- p 保存的就是 n 的地址
  p  的地址 = 0x16cf51db0        <-- 指针自己也是个变量，也有地址
  *p 的值   = 0x11223344    <-- * 顺着地址把值取回来

== 3. 指针类型决定「解引用读几个字节」 ==
  word            = 0xAABBCCDD
  *(unsigned char*)&word = 0xDD    (读 1 字节)
  *(uint16_t*)&word      = 0xCCDD  (读 2 字节)
  *(uint32_t*)&word      = 0xAABBCCDD (读 4 字节)
  字节序: DD CC BB AA  -> 低位在前，说明本机是 little-endian

== 4. 指针类型决定「+1 走多远」 ==
  char*   0x1000 + 1 = 0x1001  (+1)
  int*    0x1000 + 1 = 0x1004  (+4)
  double* 0x1000 + 1 = 0x1008  (+8)
  规律: p + k 的真实地址偏移 = k * sizeof(*p)

== 5. 所有指针本身一样大（本平台 8 字节）==
  sizeof(char*)=8 sizeof(int*)=8 sizeof(double*)=8 sizeof(void*)=8
```

**内存图**（`p` 和 `n` 的关系）：

```text
地址            内容
0x16cf51db0  ┌──────────────────┐
   (&p)      │ 0x16cf51db8      │  p —— 一个 8 字节变量，装着 n 的地址
             └──────────────────┘
                     │
                     └──────────┐
                                ▼
0x16cf51db8  ┌──────────────────┐
   (&n)      │ 0x11223344       │  n —— 一个 4 字节 int
             └──────────────────┘
```

**结论**：指针也是普通变量，只是它存的值恰好是「另一个对象的地址」。
指针的**类型**不影响它自己多大（永远 8 字节），只影响两件事：
1. 解引用时读/写多少个字节；
2. `p + 1` 在地址上走多远。

---

## p02 —— 数组退化（array decay）

这个文件**故意触发警告**，所以不加 `-Werror`：

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -O0 -g p02_decay.c -o p02_decay
```

```text
p02_decay.c:12:101: warning: sizeof on array function parameter will return size of 'int *' instead of 'int[]' [-Wsizeof-array-argument]
p02_decay.c:17:99: warning: sizeof on array function parameter will return size of 'int *' instead of 'int[10]' [-Wsizeof-array-argument]
2 warnings generated.
```

运行输出：

```text
== 1. 在「看得见数组」的作用域里 ==
  sizeof(arr)            = 20  (5 个 int = 5*4)
  sizeof(arr[0])         = 4
  元素个数 = sizeof(arr)/sizeof(arr[0]) = 5

== 2. 数组名在大多数表达式里「退化」成指向首元素的指针 ==
  arr      = 0x16f9cdda0
  &arr[0]  = 0x16f9cdda0   <-- 和 arr 相同
  &arr     = 0x16f9cdda0   <-- 数值相同，但类型是 int(*)[5]！
  sizeof(p)= 8   <-- 退化之后长度信息就没了

== 3. 类型不同 => +1 走的距离不同 ==
  arr  + 1 = 0x16f9cdda4  (+4 字节, 跳 1 个 int)
  &arr + 1 = 0x16f9cddb4  (+20 字节, 跳整个数组!)

== 4. 传进函数以后，长度信息彻底丢失 ==
    void f(int *a)      : sizeof(a) = 8  <-- 指针大小
    void f(int a[])     : sizeof(a) = 8  <-- 骗人的写法，还是指针
    void f(int a[10])   : sizeof(a) = 8  <-- 那个 10 被编译器无视
    void f(int (*a)[5]) : sizeof(*a) = 20  <-- 真的拿到了数组大小
                          元素个数 = 5

== 5. 三种下标写法完全等价 ==
  arr[2]    = 3
  *(arr+2)  = 3
  p[2]      = 3
  2[arr]    = 3   <-- 合法但请永远不要这么写
```

### 机制剖析

C 标准规定：除了三个例外，**数组类型的表达式会自动转换成「指向首元素的指针」**。

三个例外：
1. `sizeof(arr)` —— 得到整个数组的字节数
2. `&arr` —— 得到 `int (*)[5]`，类型里带着长度
3. 用字符串字面量初始化字符数组时（`char s[] = "abc"`）

### 关键对比表

| 表达式 | 类型 | 值（本次运行） | `+1` 走多远 |
|---|---|---|---|
| `arr` | `int *`（退化后） | `0x16f9cdda0` | +4 字节 |
| `&arr[0]` | `int *` | `0x16f9cdda0` | +4 字节 |
| `&arr` | `int (*)[5]` | `0x16f9cdda0` | **+20 字节** |

三者**数值完全相同，类型完全不同**。这是理解指针最容易卡住的地方。

### `a[i]` 为什么等于 `i[a]`

标准把 `a[i]` **定义**为 `*(a + i)`。加法可交换，所以 `*(i + a)` 也就是 `i[a]`。
知道这一点是为了理解 `[]` 不是「数组专属语法」，而是指针运算的语法糖。

**结论与最佳实践**：
- 只要把数组传给函数，**必须额外传长度**。
- `#define LEN(a) (sizeof(a)/sizeof((a)[0]))` 只能在「看得见数组定义」的作用域里用。
- 函数参数写 `int a[10]` 是在骗人，写 `int *a` 更诚实；真要固定长度就写 `int (*a)[10]`。

---

## p03 —— `char *s` vs `char s[]`

```text
== 1. sizeof 完全不同 ==
  const char *lit = "abc";  sizeof(lit) = 8  <-- 指针大小
  char        buf[] = "abc"; sizeof(buf) = 4  <-- 'a','b','c','\0'
  strlen(lit) = 3   strlen(buf) = 3

== 2. 它们在内存的哪个区 ==
  lit 指向的地址 = 0x100f78744   (只读数据段 __TEXT/__cstring)
  buf 的地址     = 0x16ee85d9c   (栈)
  栈上变量地址   = 0x16ee85d98

== 3. buf 可以改，lit 指向的内容不能改 ==
  buf[0]='A' 之后 buf = "Abc"

== 4. 相同的字面量可能被合并成同一份 ==
  a = 0x100f789ea
  b = 0x100f789ea
  a == b ? true  <-- 编译器做了字符串池化（不是标准保证的）
  数组版本 c = 0x16ee85d80, d = 0x16ee85d78, c==d ? false

== 5. 比较字符串要用 strcmp，不能用 == ==
  strcmp(c, d) = 0  (0 表示内容相同)
```

### 对比表

| | `const char *s = "abc";` | `char s[] = "abc";` |
|---|---|---|
| `s` 是什么 | 一个指针变量（8 字节） | 一个 4 字节的字符数组 |
| `sizeof(s)` | 8 | 4 |
| 数据在哪 | `__TEXT/__cstring` 只读段 | 栈（或所在作用域的存储区） |
| 能否修改内容 | **不能**，UB | 能 |
| 能否改变指向 | 能（`s = other`） | 不能（数组名不是左值） |
| 初始化开销 | 0，只是存个地址 | 每次进作用域都要拷贝 4 字节 |
| 地址差距 | `0x100f78744`（低） | `0x16ee85d9c`（高，栈区） |

地址相差约 `0x6EF0D658`（约 1.7 GB），确实在完全不同的段。

**最佳实践**：
- 指向字面量一律写 `const char *`。
- 需要可写就用 `char buf[]` 或 `malloc`。
- 比较内容用 `strcmp`，`==` 比的是地址。

---

## p04 —— 为什么 `swap` 不生效

```text
== 1. 反例：swap_broken ==
  调用前 x=1 y=2   &x=0x16b905db8 &y=0x16b905db4
    [broken] 入口 &a=0x16b905d3c &b=0x16b905d38  a=1 b=2
    [broken] 出口              a=2 b=1  (副本换好了，没用)
  调用后 x=1 y=2   <-- 没换！

== 2. 正确：swap_ok ==
  调用前 x=1 y=2
    [ok]     入口 pa=0x16b905db8 pb=0x16b905db4
  调用后 x=2 y=1   <-- 换成功了

== 3. 指针也是值：改指针本身也需要多一层 ==
  初始 p -> a(10)
  retarget_broken 后 p -> 10   <-- 没改
  retarget_ok     后 p -> 20   <-- 改了
```

### 决定性证据

```text
调用者:   &x = 0x16b905db8    &y = 0x16b905db4
swap_broken 里: &a = 0x16b905d3c   &b = 0x16b905d38   <-- 完全不同的地址
swap_ok     里:  pa = 0x16b905db8   pb = 0x16b905db4   <-- 和 &x/&y 完全相同
```

`swap_broken` 交换的是它自己栈帧里的两个副本，函数一返回就全丢了。

### 执行过程图

```text
swap_broken(x, y):
  调用者栈帧          被调栈帧
  ┌────────┐        ┌────────┐
  │ x = 1  │  拷贝→  │ a = 1  │  ← 交换发生在这里
  │ y = 2  │  拷贝→  │ b = 2  │
  └────────┘        └────────┘
     不变                交换后随栈帧一起销毁

swap_ok(&x, &y):
  调用者栈帧          被调栈帧
  ┌────────┐        ┌──────────────┐
  │ x = 1  │◄───────│ pa = &x      │  ← *pa 直接改到了调用者的 x
  │ y = 2  │◄───────│ pb = &y      │
  └────────┘        └──────────────┘
```

**口诀**：想在函数里修改 `T` 类型的东西，参数就写 `T*`。想修改 `int`，传 `int*`；想修改 `int*`，传 `int**`。

---

## p05 —— 多级指针

```text
== 1. 一步步看清 int** ==
  v    = 42        &v   = 0x16d661d4c
  p    = 0x16d661d4c  &p   = 0x16d661d40
  pp   = 0x16d661d40  &pp  = 0x16d661d38
  ppp  = 0x16d661d38
  *p   = 42   **pp = 42   ***ppp = 42   <-- 都是同一个 v

  内存示意:
    ppp ---> pp ---> p ---> v(42)

== 2. 场景：函数分配内存并回传 ==
  调用前 buf = 0x0
  调用后 buf = 0x102f9d6d0, 内容 = "buffer of 64 bytes"

== 3. 场景：free 并置空 ==
  free_and_null 后 buf = 0x0  <-- 已置空，不是悬垂指针

== 4. 场景：argv ==
  argc = 3
    argv[0] = "./p05_multilevel"  (argv+0 = 0x16d662430)
    argv[1] = "one"  (argv+1 = 0x16d662438)
    argv[2] = "two"  (argv+2 = 0x16d662440)

== 5. 指针数组 vs 数组指针（读法练习）==
  int *arr_of_ptr[3]  : sizeof = 24 (3 个指针)
  int (*ptr_to_arr)[3]: sizeof = 8 (1 个指针)
  *arr_of_ptr[1]      = 2
  (*ptr_to_arr)[1]    = 8
```

注意地址是严格递减的：`&v=...d4c > &p=...d40 > &pp=...d38`，每级相差 8 或 12 字节，正好装得下一个指针。

**`int**` 的三个真实用途**：
1. 让函数分配内存并回传给调用者（`int alloc(size_t n, char **out)`）
2. `free` 之后顺手置空（`void free_and_null(void **pp)`）
3. 指针数组（`char *argv[]` 即 `char **argv`）

**读法口诀**：从变量名出发，先看右边，再看左边，遇到括号先算括号。

| 声明 | 读作 | `sizeof` |
|---|---|---|
| `int *a[3]` | a 是数组[3] → 元素是指针 → 指向 int | 24 |
| `int (*a)[3]` | a 是指针 → 指向数组[3] → 元素是 int | 8 |
| `int **a` | a 是指针 → 指向指针 → 指向 int | 8 |
| `int *f(void)` | f 是函数 → 返回 int* | — |
| `int (*f)(void)` | f 是指针 → 指向函数 → 返回 int | 8 |

---

## p06 —— 函数指针与回调

```text
== 2. 基本用法 ==
  fp = add;  fp(3,4)   = 7
  (*fp)(3,4)           = 7   <-- 显式解引用，效果相同
  fp = &sub; fp(3,4)   = -1
  fp 的大小 = 8 字节, 值 = 0x102114a00

== 3. 函数指针数组 / 分发表 ==
  add(6, 3) = 9
  sub(6, 3) = 3
  mul(6, 3) = 18

== 4. 回调：把策略传进算法 ==
  map(square): 1 4 9 16 25 
  map(negate): -1 -4 -9 -16 -25 

== 5. 标准库 qsort 就是回调 ==
  升序: 1 3 7 19 42 88 
  降序: 88 42 19 7 3 1 
  字符串升序: apple banana orange pear
```

### 声明读法

```text
int (*fp)(int, int);
    │  │   └────────── 参数列表
    │  └────────────── fp 是指针
    └───────────────── 返回 int

读作：fp 是一个指针，指向一个「接收两个 int、返回 int」的函数。

int *fp(int, int);
读作：fp 是一个函数，接收两个 int，返回 int*。   ← 少了括号，意思完全变了
```

### `qsort` 比较函数的写法

```c
static int cmp_int_asc(const void *pa, const void *pb)
{
    int a = *(const int *)pa;
    int b = *(const int *)pb;
    return (a > b) - (a < b);      /* 不要写 a - b，会溢出 */
}
```

`return a - b` 在 `a=INT_MAX, b=-1` 时溢出，是 UB。`(a > b) - (a < b)` 只返回 -1/0/1，永远安全。

**最佳实践**：
- 用 `typedef int (*BinOp)(int, int);` 给函数指针起名字，可读性提升巨大。
- 函数名会自动退化成函数指针，`fp = add` 和 `fp = &add` 等价；调用时 `fp(x)` 和 `(*fp)(x)` 也等价。
- 回调函数一般要带一个 `void *ctx` 参数用来传上下文（`qsort` 没有，这是它的设计缺陷，`qsort_r` 补上了）。
- 函数指针和对象指针之间的转换（`void*` ↔ 函数指针）在 ISO C 里不保证可行，`-Wpedantic` 会警告。

---

## p07 —— `const` 与指针

```text
== 从右往左读声明 ==
  const int *p        : p 是指针 -> 指向 const int   (数据只读，指针可改)
  int const *p        : 和上面完全一样
  int * const p       : p 是 const 指针 -> 指向 int   (指针只读，数据可改)
  const int * const p : 两个都只读

== 1. const int *p  —— 指向常量的指针 ==
  *p1 = 1
  p1 改指向 b, *p1 = 2   <-- 指针可以换目标

== 2. int * const p —— 常量指针 ==
  *p2 = 100 之后 a = 100   <-- 数据可以改
  但 p2 = &b 是编译错误

== 3. const int * const p —— 全都锁死 ==
  *p3 = 100  (只能读)
```

### 速查表

| 声明 | `*p = x` | `p = &y` | 中文名 |
|---|:---:|:---:|---|
| `int *p` | ✅ | ✅ | 普通指针 |
| `const int *p` / `int const *p` | ❌ | ✅ | 指向常量的指针 |
| `int * const p` | ✅ | ❌ | 常量指针 |
| `const int * const p` | ❌ | ❌ | 指向常量的常量指针 |

**判断技巧**：看 `const` 在 `*` 的**左边还是右边**。
- `const` 在 `*` 左边 → 管**数据**（不能通过这个指针改数据）
- `const` 在 `*` 右边 → 管**指针**（指针自己不能改）

### `const` ≠ 常量

```text
  const int n = 5; 它是「只读变量」，不是编译期常量
  需要编译期常量请用 enum 或 #define，或 C23 的 constexpr
```

在 C 里 `const int n = 5;` **不能**用作数组长度（会变成 VLA）或 `case` 标签。这点和 C++ 不同。

**最佳实践**：
- 所有「只读输入」参数写 `const T *`。这既是文档，也让编译器帮你查错。
- `int *` → `const int *` 的隐式转换是安全允许的；`int **` → `const int **` **不允许**（会破坏 const 安全性）。
- 不要用强制转换去掉 `const`，那等于关掉安全带。

---

## p08 —— `void*`、NULL、野指针、悬垂指针

```text
== 1. void* 可以接住任何对象指针 ==
  int        (4 字节): 04 03 02 01 
  double     (8 字节): 00 00 00 00 00 00 F8 3F 
  char[3]    (3 字节): 48 69 00 
  *(int*)vp = 16909060   <-- 用之前必须转回正确的类型

== 2. 泛型 swap ==
  swap int:    x=2 y=1
  swap double: p=2 q=1

== 3. 空指针 NULL ==
  NULL 打印出来是 0x0
  if (np) 为 假；空指针在布尔上下文里是假

== 5. 悬垂指针（dangling pointer）：指向已失效对象 ==
  free 之前: heap=0x104f01720 内容="alive"
  free 之后: heap 仍然是 0x104f01720，但这块内存已经不属于你了
  置 NULL 之后: heap=0x0，再误用会立刻暴露而不是静默出错
```

### 三种「坏指针」

| 名称 | 定义 | 典型成因 | 防御 |
|---|---|---|---|
| 野指针 wild | 从未初始化过 | `int *p;` 然后直接用 | 定义即初始化（`= NULL`） |
| 空指针 null | 值为 NULL | `malloc` 失败 | 每次分配后检查 |
| 悬垂指针 dangling | 指向已失效对象 | `free` 之后 / 返回局部变量地址 / `realloc` 搬家 | `free` 后置 NULL；不返回局部地址 |

### `void*` 的规则

- 任何对象指针都可以隐式转成 `void*`，也可以从 `void*` 转回来（往返无损）。
- **不能解引用** `void*`，因为不知道大小。
- 在 C 里 `malloc` 的返回值**不需要**强制转换（C++ 才需要）。多此一举的转换还会掩盖「忘记 include stdlib.h」的错误。
- 函数指针和 `void*` 的互转在 ISO C 里不保证可行。

---

## p09 —— 指针运算与边界

```text
== 1. 指针 + 整数 ==
  p=0x16d955da0  *p=10  (p - begin = 0)
  p=0x16d955da4  *p=20  (p - begin = 1)
  p=0x16d955da8  *p=30  (p - begin = 2)
  p=0x16d955dac  *p=40  (p - begin = 3)
  p=0x16d955db0  *p=50  (p - begin = 4)

== 2. 指针 - 指针 = 元素个数（类型是 ptrdiff_t）==
  end - begin = 5  个元素
  字节差 = 20  (= 5 * sizeof(int))

== 3. one-past-the-end 的规则 ==
  arr+5 = 0x16d955db4  <-- 合法：可以计算、可以比较
  *(arr+5)     <-- 非法：解引用是 undefined behavior
  arr+6        <-- 非法：连「算出这个地址」本身都是 UB

== 6. 用 char* 做字节级步进 ==
  sizeof(struct S) = 24
  原始字节: 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 40 78 00 00 00 00 00 00 00 

== 7. 整数与指针互转（可移植性警告）==
  pv        = 0x16d955d3c
  uintptr_t = 0x16D955D3C
  转回来 *back = 7  (uintptr_t 往返是标准保证的)
```

### one-past-the-end 规则

标准允许指针指向「数组最后一个元素的再后面一个位置」，但**不允许解引用**它：

```text
  arr[0] arr[1] arr[2] arr[3] arr[4]   (一个虚拟位置)
   ↑                                ↑
  arr                            arr + 5
  合法可解引用                     合法可比较，不可解引用

  arr + 6  ← 连「计算这个值」都是 UB
```

这条规则就是所有 `for (p = begin; p != end; p++)` 循环的合法性依据。

### 指针差的类型

`p1 - p2` 的类型是 `ptrdiff_t`（有符号），打印用 `%td`。
结果单位是「元素个数」，不是字节数。要字节数就先转成 `char*`。

---

## p10 —— 严格别名规则（本章最重要的实验）

这个实验**在 -O0 和 -O2 下输出不同**，是 UB 危害的铁证。

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -g -O0 p10_aliasing.c -o p10_O0 && ./p10_O0
cc -std=c17 -Wall -Wextra -Wpedantic -g -O2 p10_aliasing.c -o p10_O2 && ./p10_O2
cc -std=c17 -Wall -Wextra -Wpedantic -g -O2 -fno-strict-aliasing p10_aliasing.c -o p10_O2ns && ./p10_O2ns
```

### 三次运行的真实输出

| 编译选项 | `bad_punning` 返回值 |
|---|---|
| `-O0` | **1073741824** |
| `-O2` | **1** |
| `-O2 -fno-strict-aliasing` | **1073741824** |

完整输出（`-O0`）：

```text
== 1. 违反严格别名的后果依赖优化级别 ==
  bad_punning 返回 1073741824
  storage 的字节 = 0x40000000
```

完整输出（`-O2`）：

```text
== 1. 违反严格别名的后果依赖优化级别 ==
  bad_punning 返回 1
  storage 的字节 = 0x40000000
```

### 为什么

```c
static int bad_punning(int *pi, float *pf)
{
    *pi = 1;              /* 写 int */
    *pf = 2.0f;           /* 写 float */
    return *pi;           /* 读 int */
}
```

标准规定：不能通过和对象实际类型不兼容的左值访问对象（`char`/`unsigned char` 等少数例外）。
所以编译器**有权假设** `pi` 和 `pf` 不指向同一块内存。于是在 `-O2` 下它推理：

```text
*pi = 1;        // pi 指向的值现在是 1
*pf = 2.0f;     // pf 和 pi 无关，不影响 *pi
return *pi;     // 那就直接返回 1，不用重新 load
```

而实际上它们指向同一块内存，`*pf = 2.0f` 把那 4 字节写成了 `0x40000000`。
`-O0` 老老实实重新 load，读到 `0x40000000 = 1073741824`。

**注意最后一行输出**：两种优化级别下 `storage 的字节` 都是 `0x40000000`。
内存**确实**被改了，只是 `-O2` 的返回值没反映出来。这正是 UB 的典型形态：**局部看起来一致，整体行为不一致**。

### 三种合法的 type punning

```text
== 2. 三种合法的 type punning ==
  f = 1
  memcpy 版本 : 0x3F800000
  union  版本 : 0x3F800000
  字节   版本 : 0x3F800000
  IEEE-754 单精度 1.0 应为 0x3F800000
```

三种都正确，任选：

```c
/* 1. memcpy —— 最推荐，编译器会优化成一条 mov，零成本 */
uint32_t bits;
memcpy(&bits, &f, sizeof bits);

/* 2. union —— C 明确允许读非活跃成员（C++ 不允许） */
union { float f; uint32_t u; } u = { .f = f };
uint32_t bits = u.u;

/* 3. unsigned char* 逐字节 —— 永远合法，但要自己处理字节序 */
const unsigned char *p = (const unsigned char *)&f;
```

**最佳实践**：
- 需要按位解释对象时用 `memcpy`（首选）或 `union`。
- 永远不要写 `*(float*)&some_int`。
- 遍历对象表示只用 `unsigned char*`。
- `-fno-strict-aliasing` 只能抢救遗留代码，不是解决方案（它会关掉一批优化）。

---

## 全章要点总结

1. 指针 = 一个存放地址的普通变量。它的类型决定**解引用宽度**和**步长**。
2. 数组在几乎所有表达式里退化成指针；退化后 `sizeof` 就拿不到长度了。
3. C 只有值传递；传指针也是值传递，只不过传的值是地址。
4. `char *s = "abc"` 和 `char s[] = "abc"` 是两个完全不同的东西。
5. `const` 在 `*` 左边管数据，在右边管指针。
6. `void*` 是类型擦除，用之前必须转回原类型。
7. 尾后指针可以算、可以比，不可以解引用。
8. 严格别名规则不是理论，`-O0` 和 `-O2` 的输出实测不同。

## 练习

1. 写一个函数 `void reverse(int *arr, size_t n)`，在函数内用 `sizeof(arr)` 试图求长度，观察为什么不行。
2. 声明并解释：`char *(*f[3])(const char *, int);`（提示：f 是数组[3] → 元素是指针 → 指向函数 → 返回 `char*`）
3. 把 `p10_aliasing.c` 的 `bad_punning` 改成 `memcpy` 版本，验证 `-O0` 和 `-O2` 结果一致。
4. 用 `void*` 写一个通用的 `bubble_sort(void *base, size_t n, size_t size, int (*cmp)(const void*, const void*))`。
