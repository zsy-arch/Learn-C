# 06_struct —— 结构体、联合体、枚举、位域

## 文件

| 文件 | 验证什么 |
|---|---|
| `s01_basics.c` | 定义、初始化、结构体赋值是浅拷贝 |
| `s02_layout.c` | 对齐、padding、`offsetof`、成员顺序影响大小 |
| `s03_deepcopy.c` | 指针成员：浅拷贝 vs 深拷贝 |
| `s03_doublefree.c` | 反例：浅拷贝后两边都 free（ASan 抓） |
| `s04_bitfield_union.c` | 位域、联合体、tagged union、字节序 |
| `s05_flexarray.c` | 柔性数组成员 |
| `s06_list.c` | 自引用结构体 + 单链表 + 二级指针技巧 |

统一编译：

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g sXX_xxx.c -o sXX_xxx && ./sXX_xxx
```

全部 `-Werror` 干净通过。

---

## s01 —— 基础与浅拷贝

```text
== 1. 三种初始化写法 ==
  p1 = (1, 2)
  p2 = (10, 20)  <-- 用 .name= 写，顺序无所谓且不怕以后加字段
  p3 = (0, 0)  <-- {0} 把所有成员清零

== 3. 结构体赋值 = 逐字节整体拷贝 ==
  b = a; b.x = 99;  =>  a=(1,2)  b=(99,2)
  a 没被影响，说明是拷贝而不是引用
  &a=0x16cf01d30  &b=0x16cf01d28  两个独立对象

== 5. 数组成员会被一起拷贝（和裸数组不同！）==
  s1:     {name="Alice", age=20, score=91.5}
  s2:     {name="Bob", age=21, score=91.5}
  改 s2 不影响 s1 —— 因为 name 是「数组成员」，随结构体一起被复制
  sizeof(Student) = 32

== 6. 嵌套结构体 ==
  box: (0,0) -> (10,5), 面积 = 50
```

**关键认知**：
- 数组**不能**整体赋值，但把数组包进结构体后**可以**整体赋值（`Student s2 = s1;` 会连 `name[16]` 一起复制）。这是 C 一个很不对称的历史设计。
- 结构体**不能**用 `==` 比较（编译错误），因为 padding 字节内容不确定。
- 初始化优先用**指定初始化器** `{.x = 1, .y = 2}`：顺序无关，以后加字段也不会静默错位。

---

## s02 —— 内存对齐与 padding（硬核）

```text
== 1. 成员顺序影响结构体大小 ==
  struct Bad  { char a; int b; char c; double d; }
    sizeof = 24, alignof = 8
    offsetof: a=0 b=4 c=8 d=16
    有效数据 = 1+4+1+8 = 14 字节，实际占 24 字节，浪费 10 字节

  struct Good { double d; int b; char a; char c; }
    sizeof = 16, alignof = 8
    offsetof: d=0 b=8 a=12 c=13
    同样的数据，只占 16 字节，浪费 2 字节

== 2. 各基本类型的对齐要求 ==
  alignof(char)=1  alignof(short)=2  alignof(int)=4
  alignof(long)=8  alignof(double)=8 alignof(void*)=8

== 4. 尾部填充（tail padding）==
  struct OneChar { char a; }  sizeof = 1
  struct Good 的最后一个成员在 offset 13，但 sizeof=16
  验证: &arr[1] - &arr[0] = 16 字节 = sizeof(struct Good)

== 5. 嵌套结构体的对齐会向外传播 ==
  struct Nested { char tag; struct Bad inner; char end; }
    sizeof = 40, alignof = 8
    offsetof: tag=0 inner=8 end=32

== 6. #pragma pack 强制紧凑（代价：未对齐访问）==
  struct Packed（pack(1)）sizeof = 14
    offsetof: a=0 b=1 c=5 d=6
```

### 字节地图

`struct Bad { char a; int b; char c; double d; }` 共 24 字节：

```text
偏移:  0    1  2  3    4  5  6  7    8    9 10 11 12 13 14 15
      ┌───┬─────────┬────────────┬───┬──────────────────────┐
      │ a │ padding │     b      │ c │       padding        │
      └───┴─────────┴────────────┴───┴──────────────────────┘
        1B     3B         4B       1B          7B

偏移: 16 17 18 19 20 21 22 23
      ┌───────────────────────┐
      │          d            │
      └───────────────────────┘
                8B

有效数据 14 B，padding 10 B，利用率 58%
```

`struct Good { double d; int b; char a; char c; }` 共 16 字节：

```text
偏移:  0  1  2  3  4  5  6  7    8  9 10 11   12   13   14 15
      ┌───────────────────────┬────────────┬────┬────┬──────┐
      │           d           │     b      │ a  │ c  │ pad  │
      └───────────────────────┴────────────┴────┴────┴──────┘
              8B                    4B       1B   1B    2B

有效数据 14 B，padding 2 B，利用率 88%
```

### 两条对齐规则

1. **每个成员的 offset 必须是它自己 `alignof` 的整数倍**。
   `struct Bad` 里 `b` 是 `int`（align 4），`a` 占了 offset 0，所以 `b` 只能从 4 开始 → 中间插 3 字节 padding。
2. **结构体总大小必须是「最大成员 alignof」的整数倍**（尾部补齐）。
   `struct Good` 最后一个成员在 offset 13，总大小要凑到 8 的倍数 → 16。

规则 2 的原因：`struct Good arr[2]` 里 `arr[1]` 也必须 8 字节对齐。实验里 `&arr[1] - &arr[0] = 16` 正好验证了这一点。

### 嵌套传播

`struct Nested { char tag; struct Bad inner; char end; }`：
- `struct Bad` 的 `alignof` 是 8，所以 `inner` 必须放在 8 的倍数上 → `offsetof(inner) = 8`（`tag` 后面插 7 字节）
- `inner` 占 24 字节，结束于 32，`end` 放在 32
- 总大小凑到 8 的倍数 → 40

### `#pragma pack(1)` 的代价

`struct Packed` 从 24 降到 14 字节，但：
- `&packed.d` 是一个**未对齐**的 `double*`，解引用它在某些架构上是 UB / 性能惩罚
- 编译器要生成多条指令来做未对齐访问
- 只在解析固定二进制格式（网络协议、文件头）时使用

**最佳实践**：
1. 成员按 `alignof` **从大到小**排列，天然省内存。
2. 想确认布局就用 `offsetof` / `sizeof` / `alignof`，不要靠猜。
3. padding 字节内容不确定 → **不要用 `memcmp` 比较结构体**，也不要把带 padding 的结构体直接 `fwrite` 到文件。
4. 跨平台序列化要**逐字段**读写。

---

## s03 —— 浅拷贝 vs 深拷贝

```text
== 1. 浅拷贝：两个结构体共享同一块堆内存 ==
  a        id=1 name="Alice"  name 指针=0x100ed16d0
  shallow  id=1 name="Alice"  name 指针=0x100ed16d0
  两者 name 指针相同? 是  <-- 这就是浅拷贝
  通过 shallow 改名字...
  a        id=1 name="Xlice"  name 指针=0x100ed16d0
  a 的名字也被改了！这通常不是你想要的

== 3. 深拷贝：各自持有独立的堆内存 ==
  a        id=1 name="Xlice"  name 指针=0x100ed16d0
  deep     id=1 name="Xlice"  name 指针=0x100ed16e0
  两者 name 指针相同? 否  <-- 独立副本
  改 deep 不影响 a
```

### 内存图

```text
浅拷贝 (Person shallow = a;)

   a                shallow
  ┌──────────┐     ┌──────────┐
  │ name  ●──┼──┐  │ name  ●──┼──┐
  │ len   5  │  │  │ len   5  │  │
  │ id    1  │  │  │ id    1  │  │
  └──────────┘  │  └──────────┘  │
                └────────┬───────┘
                         ▼
                  ┌─────────────┐
                  │ "Alice\0"   │   ← 同一块堆内存，两个 owner
                  └─────────────┘      改一个影响另一个
                  0x100ed16d0          free 两次 = double free

深拷贝 (person_copy(&deep, &a))

   a                deep
  ┌──────────┐     ┌──────────┐
  │ name  ●──┼─┐   │ name  ●──┼─┐
  └──────────┘ │   └──────────┘ │
               ▼                ▼
        ┌─────────────┐  ┌─────────────┐
        │ "Xlice\0"   │  │ "Xlice\0"   │  ← 各自独立
        └─────────────┘  └─────────────┘
        0x100ed16d0      0x100ed16e0
```

### 反例：double free（ASan）

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -O0 -g \
   -fsanitize=address,undefined -fno-omit-frame-pointer \
   s03_doublefree.c -o s03_doublefree_asan
./s03_doublefree_asan
```

```text
=================================================================
==40729==ERROR: AddressSanitizer: attempting double-free on 0x602000000970 in thread T0:
    #0 0x0001028a5308 in free+0x7c (libclang_rt.asan_osx_dynamic.dylib)
    #1 0x00010212089c in main s03_doublefree.c:22

0x602000000970 is located 0 bytes inside of 8-byte region [0x602000000970,0x602000000978)
freed by thread T0 here:
    #0 0x0001028a5308 in free+0x7c
    #1 0x000102120894 in main s03_doublefree.c:21

previously allocated by thread T0 here:
    #0 0x0001028a5214 in malloc+0x78
    #1 0x000102120824 in main s03_doublefree.c:13

SUMMARY: AddressSanitizer: double-free s03_doublefree.c:22 in main
==40729==ABORTING
```

ASan 一次性给出三个位置：**在哪 double free、上一次在哪 free、最初在哪 malloc**。

不加 sanitizer 直接跑：

```bash
./s03_doublefree > /tmp/df.out 2>/tmp/df.err; echo "exit=$?"
```

```text
exit=133        # 133 - 128 = 5 = SIGTRAP
--- stdout ---  # 空！
--- stderr ---  # 空！
```

**两个重要观察**：
1. macOS 的 libmalloc 检测到 double free，直接 `SIGTRAP` 中止（退出码 133）。
2. `printf` 的输出**全部丢失**了 —— stdout 是全缓冲的，进程异常终止时缓冲区没被刷出。
   这就是为什么调试崩溃时要么用 `fprintf(stderr, ...)`（无缓冲/行缓冲），要么在关键点 `fflush(stdout)`。

**最佳实践**：
1. 结构体里一旦出现指针成员，就必须明确「谁拥有这块内存」。
2. 为这类结构体配套写 `init` / `copy` / `free` 三件套。
3. 不要用 `=` 复制带所有权的结构体，除非你明确要**转移**所有权（转移后把源置空）。
4. `free` 之后把指针成员置 NULL。

---

## s04 —— 位域、联合体、tagged union

```text
== 1. 位域：省空间 ==
  struct Flags 用 4 个字段，sizeof = 4 字节
  如果用 4 个 int 要 16 字节
  visible=1 enabled=0 level=9
  把 level 设成 20（4 位放不下）-> 实际值 = 4  <-- 悄悄截断！

== 2. 位域的可移植性陷阱 ==
  用移位解析 0x45: version=4 ihl=5  <-- 可移植写法
  用位域解析 0x45: version=4 ihl=5  <-- 依赖实现！

== 3. 联合体：同一块内存的多种解释 ==
  sizeof(union Value32) = 4  (= 最大成员的大小)
  写入 v.f = 1.0f
    v.u     = 0x3F800000   (IEEE-754 位模式)
    v.i     = 1065353216
    v.bytes = 00 00 80 3F

== 4. 用联合体检测字节序 ==
  0x01020304 的第一个字节 = 0x04 -> little endian

== 5. tagged union：安全地表达「多选一」 ==
  sizeof(Variant) = 16
    int    = 42
    double = 3.14
    string = "hello"

== 6. 枚举的底层类型 ==
  sizeof(enum{A,B,C}) = 4
  Small s = (Small)999; s = 999
```

### 位域三条纪律

1. **位数放不下会静默截断**：`level : 4` 赋值 20 得到 4（`20 & 0xF`）。写字面量时 clang 会给 `-Wbitfield-constant-conversion`，但用变量赋值就抓不到了。
2. **位排列顺序是 implementation-defined**：同一段位域代码在不同编译器/平台上，哪个字段占高位哪个占低位可能相反。
3. **不能对位域取地址**（`&fl.level` 是编译错误）。

所以：**解析网络协议/文件格式时用移位和掩码，不要用位域**。

```c
/* 可移植 */
unsigned version = (raw >> 4) & 0x0Fu;
unsigned ihl     =  raw       & 0x0Fu;
```

### 联合体在 C 里可以做 type punning

C 标准（6.5.2.3 脚注 97）明确允许读取 union 中「非活跃」的成员，值由对象表示决定。
**C++ 不允许**，那里只能用 `memcpy` 或 `std::bit_cast`。

### tagged union

```c
typedef enum { VT_INT, VT_DOUBLE, VT_STRING } ValueType;
typedef struct {
    ValueType type;      /* 标签 */
    union { long i; double d; const char *s; } as;
} Variant;
```

`sizeof(Variant) = 16`：4 字节 tag + 4 字节 padding + 8 字节 union。
**关键约束**：`type` 和 union 的活跃成员必须永远一致，这要靠你自己维护 —— C 没有编译器检查。

### 枚举没有类型安全

```text
Small s = (Small)999; s = 999
```

C 的枚举变量可以装任何整数。它只是「有名字的整型常量」，不是独立类型。

---

## s05 —— 柔性数组成员

```text
== 1. 柔性数组成员不计入 sizeof ==
  sizeof(Packet)    = 16  (len + tag + padding，data[] 占 0)
  offsetof(Packet, data) = 12
  sizeof(PacketPtr) = 24  (len + tag + 一个指针)

== 2. 一次分配，头和数据连续 ==
  p        = 0x100b716d0
  p->data  = 0x100b716dc   <-- 紧跟在头部后面，偏移 12
  tag=7 len=20 data="hello flexible array"
  实际分配了 sizeof(Packet)+len+1 = 37 字节

== 3. 对照：指针成员版本 ==
  q        = 0x100b716d0
  q->data  = 0x100b716f0   <-- 在堆上另一处，离得很远
  两块内存相距 32 字节
```

### 内存布局对比

```text
柔性数组版（一次 malloc，一次 free）:
  0x100b716d0
  ┌────────────┬──────┬──────┬──────────────────────────────┐
  │ len (8B)   │tag 4B│pad 0 │ data[21]  "hello flexible..." │
  └────────────┴──────┴──────┴──────────────────────────────┘
   ↑ p                        ↑ p->data (offset 12)
   一块连续内存，CPU cache 友好

指针成员版（两次 malloc，两次 free）:
  0x100b716d0                    0x100b716f0
  ┌────────────┬──────┬────────┐  ┌────────────────────────┐
  │ len (8B)   │tag 4B│ptr 8B ●┼─→│ "hello flexible array" │
  └────────────┴──────┴────────┘  └────────────────────────┘
   ↑ q                              ↑ q->data
   两次分配，两次间接寻址
```

注意 `offsetof(Packet, data) = 12`：`size_t len`（8）+ `int tag`（4）= 12，`char data[]` 的 align 是 1，所以正好接在 12。
但 `sizeof(Packet) = 16` —— 因为结构体总大小要凑到 `alignof(size_t)=8` 的倍数。
所以 `malloc(sizeof(Packet) + n + 1)` 实际上多分配了 4 字节，这是安全的（宁多勿少）。

### 三种写法的历史

| 写法 | 标准状态 |
|---|---|
| `char data[1];` | C89 "struct hack"，越界访问，技术上是 UB |
| `char data[0];` | GCC 扩展，非标准 |
| `char data[];` | **C99 标准柔性数组，用这个** |

### 使用限制

- 必须是**最后一个**成员，且前面至少还有一个成员。
- 含柔性数组的结构体**不能**放进数组、不能作为另一个结构体的非最后成员。
- 结构体赋值 `*a = *b` **不会**复制柔性数组部分。
- 适合「创建后长度不变」的对象；需要频繁 realloc 且外部持有指针时不适合。

---

## s06 —— 自引用结构体与链表

```text
== 1. 自引用结构体 ==
  sizeof(Node) = 16  (int + padding + 指针)

== 2. 头插 ==
  push_front     size=3  3 -> 2 -> 1 -> NULL

== 3. 尾插（二级指针遍历）==
  push_back      size=6  3 -> 2 -> 1 -> 7 -> 8 -> 9 -> NULL

== 4. 节点在堆上的实际地址 ==
    node@0x1034596f0 value=3 next=0x1034596e0
    node@0x1034596e0 value=2 next=0x1034596d0
    node@0x1034596d0 value=1 next=0x103459700
    node@0x103459700 value=7 next=0x103459710
    node@0x103459710 value=8 next=0x103459720
    node@0x103459720 value=9 next=0x0

== 5. 删除（含删除头节点，无需特判）==
  before remove  size=7  3 -> 2 -> 1 -> 7 -> 8 -> 9 -> 3 -> NULL
  删除了 2 个值为 3 的节点
  after remove   size=5  2 -> 1 -> 7 -> 8 -> 9 -> NULL

== 6. 反转 ==
  reversed       size=5  9 -> 8 -> 7 -> 1 -> 2 -> NULL

== 7. 释放整条链表 ==
  after free     size=0  NULL
```

`sizeof(Node) = 16`：`int value`（4）+ 4 字节 padding + `Node *next`（8）。
观察节点地址：前三个是 `...6f0, ...6e0, ...6d0`（头插，地址递减），后三个是 `...700, ...710, ...720`（尾插，地址递增）。这反映了 malloc 的分配顺序，**不是**链表的逻辑顺序 —— 链表的顺序完全由 `next` 决定。

### 二级指针遍历技巧（本节最有价值的部分）

朴素写法需要特判头节点：

```c
/* 啰嗦版 */
if (head != NULL && head->value == v) {
    Node *t = head; head = head->next; free(t);
}
Node *prev = head;
while (prev != NULL && prev->next != NULL) {
    if (prev->next->value == v) { ... } else { prev = prev->next; }
}
```

二级指针版本完全不用特判：

```c
Node **cur = &l->head;          /* cur 指向「需要被修改的那个指针」 */
while (*cur != NULL) {
    Node *entry = *cur;
    if (entry->value == v) {
        *cur = entry->next;     /* 直接改写前驱的 next（或 head 本身） */
        free(entry);
    } else {
        cur = &entry->next;     /* 前进 */
    }
}
```

图解：

```text
第一轮:  cur = &head
         head ──→ [3] ──→ [2] ──→ [1] ──→ NULL
          ↑
         cur 指向 head 这个「指针变量」本身
         删除 [3]：*cur = entry->next  等价于 head = [2]

第二轮:  cur = &[2].next
         head ──→ [2] ──→ [1] ──→ NULL
                    ↑
                   cur 指向 [2].next 这个「指针变量」
         删除 [1]：*cur = NULL
```

统一了「修改 head」和「修改某个节点的 next」这两种情况。

**最佳实践**：
1. 遍历删除用 `Node **cur = &head`，省掉所有头节点特判。
2. `free(cur)` 之前一定先把 `cur->next` 存下来。
3. 用 `malloc(sizeof *n)` 而不是 `malloc(sizeof(Node))`，改类型名时不会错。
4. `size` 自己维护，不要每次 O(n) 数一遍。
5. 维护 `tail` 指针让尾插变 O(1)，但删除时记得同步更新 `tail`。

## 练习

1. 用 `offsetof` 写一个宏 `container_of(ptr, type, member)`（Linux 内核的核心宏）。
2. 把 `struct Bad` 的成员重排到最省空间，用程序验证 `sizeof`。
3. 给 `s06_list.c` 加一个 `list_sort`，用归并排序（链表归并不需要额外空间）。
4. 用 tagged union 实现一个简单的 JSON 值类型（null / bool / number / string / array）。
