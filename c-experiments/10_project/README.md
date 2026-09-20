# 10_project —— 综合练习：动态数组 + 单链表 + 字符串工具

把前面九章学到的东西合成一个小型的、可构建、可测试、零泄漏的 C 项目。

## 文件结构

```text
10_project/
├── Makefile          自动依赖、-Werror、asan target
├── vec.h  / vec.c    动态数组（dynamic array）
├── slist.h/ slist.c  单链表（singly linked list）
├── sstr.h / sstr.c   字符串工具（安全版）
├── main.c            58 个断言的测试程序
└── build/            所有中间产物（make clean 可删）
```

## 构建与运行

```bash
make            # 构建
make run        # 构建并运行
make asan       # ASan + UBSan 版本
make clean      # 清理
```

### `make` 的真实输出

```text
mkdir -p build
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wcast-qual -Wwrite-strings -O0 -g -MMD -MP -c vec.c -o build/vec.o
cc ... -c slist.c -o build/slist.o
cc ... -c sstr.c  -o build/sstr.o
cc ... -c main.c  -o build/main.o
cc build/vec.o build/slist.o build/sstr.o build/main.o  -o build/demo
```

**零警告通过了 11 个警告开关**，包括最严格的 `-Wconversion` / `-Wsign-conversion` / `-Wcast-qual` / `-Wwrite-strings`。

### `make run` 的真实输出

```text
./build/demo
=========== 综合练习测试 ===========
== Vec 动态数组 ==
  push 10 个之后: len=10 cap=16（容量按翻倍增长）
  vec_get(&v, 999, &out) -> index out of range（越界被拦住）
  pop 出来的是 81（最后一个 9*9）
  当前内容: 100 1 4 9 16 25 36 49 64 
  shrink_to_fit: cap 16 -> 9
  vec_free 调用两次也安全（内部已置 NULL）

== SList 单链表 ==
  遍历结果: 0 1 2 3 
  删掉所有 2 之后 size=3
  反转后第一个元素是 3
  slist_free 调用两次也安全

== sstr 字符串工具 ==
  sstr_copy(buf[8], "hello") -> "hello" 返回 5
  sstr_copy(buf[8], "0123456789") -> "0123456" 返回 10 (>=8 说明截断)
  sstr_cat -> "abcdefg" 返回 10
  sstr_dup -> "duplicate me" @0x100f816d0
  sstr_trim("   \t hello world \n  ") -> "hello world"
  sstr_upper -> "MIXED CASE 123"
  sstr_lower -> "mixed case 123"
  starts_with / ends_with OK
  sstr_split("a,bb,ccc,dddd", ',') -> 4 段: [a] [bb] [ccc] [dddd]

=========== 结果: 58 passed, 0 failed ===========
```

退出码 `0`。

### `make asan` —— 同样 58/58 通过

```text
=========== 结果: 58 passed, 0 failed ===========
```

ASan + UBSan 下没有报出任何越界、UAF、溢出。

### 泄漏检测

```bash
make clean && make
MallocStackLogging=1 leaks --atExit -- ./build/demo
```

```text
Process 61708: 189 nodes malloced for 31 KB
Process 61708: 0 leaks for 0 total leaked bytes.
```

**0 泄漏。**

---

## 设计要点

### 1. Vec —— 动态数组

```c
typedef struct {
    int   *data;
    size_t len;      /* 当前元素个数 */
    size_t cap;      /* 已分配的元素容量 */
} Vec;
```

**容量翻倍**：push 10 个元素后 `cap=16`（4 → 8 → 16）。
如果每次只 `+1`，n 次 push 需要 O(n²) 次元素拷贝；翻倍后摊还成本是 O(1)。

**双重溢出检查**：

```c
while (newcap < want) {
    if (newcap > SIZE_MAX / 2) { newcap = want; break; }   /* 翻倍不回绕 */
    newcap *= 2;
}
if (newcap > SIZE_MAX / sizeof *v->data) { return VEC_ENOMEM; }  /* 乘法不回绕 */
```

**`realloc` 正确写法**：

```c
int *tmp = realloc(v->data, newcap * sizeof *v->data);
if (tmp == NULL) { return VEC_ENOMEM; }   /* 原 v->data 保持有效，未泄漏 */
v->data = tmp;
```

**边界检查用无符号比较**：

```c
if (i >= v->len) { return VEC_ERANGE; }
```

`i` 是 `size_t`，即使调用者传进来一个「负数」，转成 `size_t` 后也是巨大值，一样被拦住。
输出验证：`vec_get(&v, 999, &out) -> index out of range`。

**`vec_free` 幂等**：内部把 `data` 置 NULL、`len`/`cap` 清零，所以调用两次也安全。

**`memmove` 而非 `memcpy`**：`vec_insert` / `vec_remove` 移动的区间是重叠的。

### 2. SList —— 单链表

```c
typedef struct { SNode *head; SNode *tail; size_t size; } SList;
```

**维护 `tail` 让尾插变 O(1)**，代价是删除时必须同步更新 `tail`：

```c
if (l->tail == entry) { l->tail = prev; }
```

**二级指针遍历删除**（见 `06_struct/README.md` 的详细图解）：

```c
SNode **cur = &l->head;
while (*cur != NULL) {
    SNode *entry = *cur;
    if (entry->value == v) {
        *cur = entry->next;      /* 统一处理「删头」和「删中间」 */
        free(entry);
    } else {
        cur = &entry->next;
    }
}
```

**回调式遍历**：

```c
void slist_foreach(const SList *l, bool (*fn)(int value, void *ctx), void *ctx);
```

带 `void *ctx` —— 这是 `qsort` 最大的设计缺陷（它没有 ctx），实际项目中回调一定要留上下文参数。
测试里用它把链表内容收集进一个 `Vec`，展示两个模块如何组合。

### 3. sstr —— 字符串工具

**`sstr_copy` 实现 BSD `strlcpy` 语义**：

| 返回值 | 含义 |
|---|---|
| `< dstsize` | 完整拷贝 |
| `>= dstsize` | 发生截断，返回值是「完整拷贝需要的长度」 |

无论哪种情况，`dst` **一定**以 `'\0'` 结尾（只要 `dstsize > 0`）。
测试验证：`sstr_copy(buf[8], "0123456789")` 返回 10，`strlen(buf)==7`，`buf[7]=='\0'`。

**`ctype.h` 函数必须先转 `unsigned char`**：

```c
*s = (char)toupper((unsigned char)*s);
```

`char` 在本平台是**有符号**的（`CHAR_MIN=-128`，见 `01_variables`）。
传一个负的 `char` 给 `toupper` 是 UB（标准要求实参必须能表示成 `unsigned char` 或 `EOF`）。

**不依赖 POSIX**：

- 自己写 `sstr_dup` 而不用 `strdup`（`strdup` 在 C17 里不存在，C23 才加入）
- 自己写 `sstr_nlen` 而不用 `strnlen`（POSIX 扩展）

这样 `-std=c17 -Wpedantic` 才能干净通过。

**`sstr_split` 是破坏性的**：它把分隔符原地替换成 `'\0'`，`out[i]` 指向 `s` 内部。
调用者必须知道：**`s` 的生命周期必须长于 `out`**，而且 `s` 不能是字符串字面量。

---

## Makefile 讲解

```make
CFLAGS  := $(STD) $(WARN) $(DEBUG) -MMD -MP
```

### 关键技巧 1：自动头文件依赖

`-MMD` 让编译器在生成 `.o` 的同时生成 `.d` 依赖文件：

```make
# build/vec.d 的内容大致是：
build/vec.o: vec.c vec.h
```

最后一行 `-include $(DEPS)` 把它们都包含进来。
效果：**改了 `vec.h`，`make` 会自动重编所有 include 它的 `.c`**。
`-MP` 额外为每个头文件生成一个空目标，避免删掉头文件后 make 报错。

### 关键技巧 2：产物隔离

```make
BUILD  := build
OBJS   := $(SRCS:%.c=$(BUILD)/%.o)

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)
```

`| $(BUILD)` 是 **order-only prerequisite**：只保证目录存在，目录的时间戳变化不会触发重编译。

### 关键技巧 3：严格警告集

```make
WARN := -Wall -Wextra -Wpedantic -Werror \
        -Wshadow -Wconversion -Wsign-conversion \
        -Wstrict-prototypes -Wmissing-prototypes \
        -Wpointer-arith -Wcast-qual -Wwrite-strings
```

| 选项 | 抓什么 |
|---|---|
| `-Wshadow` | 内层变量遮蔽外层同名变量 |
| `-Wconversion` | 隐式类型转换可能丢失精度 |
| `-Wsign-conversion` | 有符号/无符号隐式转换 |
| `-Wstrict-prototypes` | `f()` 而不是 `f(void)` |
| `-Wmissing-prototypes` | 非 static 函数没有事先声明 |
| `-Wpointer-arith` | 对 `void*` 做算术（GNU 扩展） |
| `-Wcast-qual` | 强转丢掉 `const` |
| `-Wwrite-strings` | 字符串字面量类型变成 `const char[]` |

`-Wconversion` 和 `-Wsign-conversion` 在老项目里会刷屏，但**新项目从第一天就开**成本极低，收益极大。

### 关键技巧 4：sanitizer target 不复用 `.o`

```make
asan: | $(BUILD)
	$(CC) $(STD) $(WARN) $(DEBUG) $(SAN) $(SRCS) -o $(TARGET_ASAN)
	./$(TARGET_ASAN)
```

ASan 的编译选项和普通版本不同，**不能复用同一批 `.o`**，否则链接出来的东西行为不一致。
所以这里直接从 `.c` 一步编译。

---

## 测试框架

40 行实现一个够用的断言框架：

```c
static int g_pass = 0, g_fail = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (cond) { g_pass++; }                                         \
        else {                                                          \
            g_fail++;                                                   \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
        }                                                               \
    } while (0)
```

宏的三个要点：
1. **`do { ... } while (0)`**：让宏在 `if (x) CHECK(y); else ...` 里也能正确工作。
2. **`#cond`**：字符串化，失败时打印出原始表达式。
3. **`__FILE__` / `__LINE__`**：定位到具体行。

`main` 返回 `g_fail == 0 ? EXIT_SUCCESS : EXIT_FAILURE`，可以直接接进 CI。

---

## 这个项目用到了前面哪些知识

| 知识点 | 来自 | 用在哪 |
|---|---|---|
| 头文件 + include guard | 00, 04 | 三个模块的 `.h` |
| `size_t` 与无符号比较 | 01 | `vec_get` 的边界检查 |
| `goto cleanup` | 03, 04 | （本项目用早返回，因为资源单一） |
| 值传递 / 出参 | 04 | 所有 `xxx(..., T *out)` |
| 数组退化 | 05 | 所有函数都显式传长度 |
| `const T *` 输入参数 | 05 | `vec_get(const Vec *v, ...)` |
| 函数指针 + ctx | 05 | `slist_foreach` |
| 自引用结构体 | 06 | `SNode` |
| 二级指针遍历 | 06 | `slist_remove_all` |
| `realloc` 正确写法 | 07 | `vec_reserve` |
| 分配大小溢出检查 | 07 | `vec_reserve` |
| `free` 后置 NULL | 07 | `vec_free` / `slist_free` |
| `snprintf`/`strlcpy` 语义 | 09 | `sstr_copy` / `sstr_cat` |
| `memmove` 处理重叠 | 09 | `vec_insert` / `vec_remove` |
| `ctype` 要转 `unsigned char` | 01, 09 | `sstr_upper` / `sstr_lower` |

---

## 练习

1. 把 `Vec` 改成泛型：`typedef struct { void *data; size_t len, cap, elemsize; } Vec;`，接口用 `void *`。
2. 给 `SList` 加一个 `slist_sort`，用链表归并排序（不需要额外空间）。
3. 给 `sstr` 加一个 `sstr_join(char **parts, size_t n, char sep, char *out, size_t outsize)`。
4. 把 `Makefile` 改成支持 `make test` 自动跑 `asan` + `leaks` 两轮。
5. 故意在 `vec_insert` 里把 `memmove` 改成 `memcpy`，用 ASan 跑，看能不能抓到。
