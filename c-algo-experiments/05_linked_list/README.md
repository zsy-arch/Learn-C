# 05_linked_list —— 单链表与二级指针

## 实验目的

回答三个核心问题：

1. 为什么遍历删除要对头节点**特判**？能不能不特判？
2. 反转链表的**三指针法**到底在干什么？
3. 链表的插入是 O(1)，为什么实践中经常比数组还慢？

## 文件

| 文件 | 作用 |
|---|---|
| `demo.c` | 头插/尾插/查找/删除/反转 + 性能对比 |
| `leak_demo.c` | ⚠️ **错误示例**：三种典型内存错误 |

## 编译与运行

```bash
cc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g demo.c -o demo
./demo

# 反例（故意写出内存错误）
cc -std=c17 -Wall -Wextra -Wpedantic -O0 -g leak_demo.c -o leak_demo
./leak_demo safe       # 正确写法
./leak_demo leak       # 内存泄漏
./leak_demo uaf        # free 之后继续用
./leak_demo dangling   # 删除后继续读
```

## 1. 头插 vs 尾插

```text
========== 1. 头插 vs 尾插 ==========
  头插 1,2,3,4:        4 -> 3 -> 2 -> 1 -> NULL
     -> 顺序反过来了，因为每次都插在头部
  尾插 1,2,3,4:        1 -> 2 -> 3 -> 4 -> NULL
  尾插(二级指针):  1 -> 2 -> 3 -> 4 -> NULL

  内存地址（尾插建的表 1->2->3->4）:
    [0] value=1    node@0x100861540  next=0x100861550
    [1] value=2    node@0x100861550  next=0x100861560
    [2] value=3    node@0x100861560  next=0x100861570
    [3] value=4    node@0x100861570  next=0x0
     -> 地址递增（malloc 顺序分配），但不保证连续
     -> 链表的逻辑顺序完全由 next 决定，与地址无关
```

## 2. 为什么需要二级指针

**C 只有值传递。** 头插要修改的是「调用者的 head 指针本身」，
传 `Node *` 只能改节点内容，改不了「head 指向谁」。

> 想修改 `T`，就传 `T *`。这里 `T` 是 `Node *`，所以要传 `Node **`。

## 3. 删除：特判 vs 零特判

```text
========== 3. 删除：特判头节点 vs 二级指针零特判 ==========
  原始:                1 -> 2 -> 3 -> 4 -> 5 -> NULL
  remove_naive(&h1, 1) [删头节点] = 1
  结果:                2 -> 3 -> 4 -> 5 -> NULL
  remove_naive(&h1, 3) [删中间]   = 1
  结果:                2 -> 4 -> 5 -> NULL
  
  同一个表用二级指针版: 1 -> 2 -> 3 -> 4 -> 5 -> NULL
  remove_pp(&h2, 1) [删头节点] = 1
  结果:                2 -> 3 -> 4 -> 5 -> NULL
  remove_pp(&h2, 3) [删中间]   = 1
  结果:                2 -> 4 -> 5 -> NULL

  两种写法结果完全一致，但 remove_pp 少了一个 if 分支。
```

### 核心思想图解

```text
朴素写法：删头和删中间是两套逻辑
  ┌─────────────────────────┐   ┌──────────────────────────┐
  │ if (head->value == v)   │   │ while (prev->next) {     │
  │     head = head->next;  │   │     if (...) {           │
  │     free(...);          │   │         prev->next = ...;│
  │ }                       │   │         free(...);       │
  └─────────────────────────┘   └──────────────────────────┘
         ↑ 特判                          ↑ 另一套

二级指针写法：统一处理
  Node **cur = &head;              /* cur 指向「需要被修改的那个指针变量」 */
  while (*cur != NULL) {
      Node *entry = *cur;
      if (entry->value == v) {
          *cur = entry->next;      /* 前驱的 next（或 head）直接跳过它 */
          free(entry);
      } else {
          cur = &entry->next;      /* 前进：cur 现在指向 entry->next */
      }
  }
```

```text
第一轮:  cur = &head
         head ──→ [1] ──→ [2] ──→ ... → NULL
          ↑
         cur 指向 head 这个「指针变量」本身
         删除 [1]：*cur = entry->next  等价于 head = [2]

第二轮:  cur = &[2].next
         head ──→ [2] ──→ [3] ──→ NULL
                    ↑
                   cur 指向 [2].next 这个「指针变量」
```

**`&head` 和 `&prev->next` 的类型都是 `Node **`** —— 这就是能统一处理的根本原因。

## 4. 反转：三指针法

```text
========== 5. 反转 ==========
  原始:                1 -> 2 -> 3 -> 4 -> 5 -> NULL
  迭代反转后:       5 -> 4 -> 3 -> 2 -> 1 -> NULL
  递归反转后:       1 -> 2 -> 3 -> 4 -> 5 -> NULL
  -> 两次反转，回到原样
  反转空表:          (空表)
  反转单节点:       7 -> NULL
```

核心代码：

```c
Node *prev = NULL;
Node *cur  = *head;
while (cur != NULL) {
    Node *next = cur->next;    /* ① 先保存后继 —— 必须！ */
    cur->next  = prev;         /* ② 掉头 */
    prev       = cur;          /* ③ prev 前进 */
    cur        = next;         /* ④ cur 前进 */
}
*head = prev;                  /* 原来的尾节点成为新头 */
```

**为什么必须有 `next` 这个临时变量？**
因为第 ② 步 `cur->next = prev` 会**覆盖**掉原来的后继，
不先存下来就**再也找不到后面了**。

## 5. 性能对比：cache 的故事

```text
========== 6. ⚠️ 链表不一定比数组快（cache 的故事）==========
  实验 A：顺序遍历求和（5 轮）
    数组（连续内存）:    8.059 ms
    链表（分散内存）:   11.363 ms
    -> 两者差不多！因为两种都是【顺序访问】，
       硬件预取器（prefetcher）能提前把下一块内存拉进 cache。

  实验 B：按下标访问（数组 O(1) vs 链表 O(n)）
    数组按下标访问 (2000 次):    0.022 ms
    链表按下标访问 (2000 次): 1805.934 ms
    -> 数组快几个数量级：链表按下标访问是 O(n)，平均要走 100 万步。
```

**实验 A 的结论可能反直觉**：顺序遍历时两者差不多。

原因是**硬件预取器**：CPU 发现你在顺序访问，会提前把后面的内存拉进 cache。
链表虽然是堆上分散的节点，但 `malloc` 的分配顺序通常也是递增的（见第 1 节的地址输出），
所以预取器依然能猜到下一个节点在哪。

**实验 B 才是链表真正的痛点**：按下标访问是 O(n)。

## 6. 反例：三种内存错误

### 错误 1：`free` 之前没保存 `next`

```c
/* ❌ */
Node *cur = head;
while (cur != NULL) {
    free(cur);
    cur = cur->next;      /* ← use-after-free！ */
}
```

**ASan 报告**：

```text
==36153==ERROR: AddressSanitizer: heap-use-after-free on address 0x6020000009d8
READ of size 8 at 0x6020000009d8 thread T0
    #0 0x000102bb5c0c in free_list_broken leak_demo.c:41

0x6020000009d8 is located 8 bytes inside of 16-byte region [0x6020000009d0,0x6020000009e0)
freed by thread T0 here:
    #1 0x000102bb5b60 in free_list_broken leak_demo.c:40
SUMMARY: AddressSanitizer: heap-use-after-free leak_demo.c:41
```

ASan 精确指出：读的是 `cur->next`（偏移 8 的位置），而这块内存**刚刚被 free**。

**不加 sanitizer 会怎样？**

```text
$ ./leak_demo uaf
  释放整条链表（错误写法）...
exit=134        # SIGABRT
```

程序直接 abort —— **而且 stdout 的输出全丢了**（缓冲区没刷出）。

### 错误 2：删除节点时忘记 `free`

```text
$ MallocStackLogging=1 leaks --atExit -- ./leak_demo leak
Process 35861: 190 nodes malloced for 31 KB
Process 35861: 1 leak for 32 total leaked bytes.

STACK OF 1 INSTANCE OF 'ROOT LEAK: <malloc in push>':
2   leak_demo    0x1024e4518 main + 184  leak_demo.c:94
1   leak_demo    0x1024e4804 push + 28  leak_demo.c:28
====
    1 (32 bytes) ROOT LEAK: <malloc in push 0x74ff02c040> [32]
```

`leaks` 给出了**精确的分配点**（`leak_demo.c:28`）。

> ⚠️ `leaks` **不能**和 ASan 同时用（ASan 替换了 malloc），
> 所以泄漏检测要用**普通编译**的二进制。

### 错误 3：删除后继续用被删的指针

```text
$ ./leak_demo dangling
exit=139        # SIGSEGV
```

**不加 sanitizer 直接段错误**（本机这次运行如此 —— 但这是 UB，
换个内存布局可能就「看起来正常」地打印出旧值）。

## 最佳实践

1. **头插、尾插、删除都传 `Node **`**，统一处理，避免头节点特判。
2. **`free(cur)` 之前必须先保存 `cur->next`。**
3. 反转用三指针：`prev` / `cur` / `next`，缺一不可。
4. 维护 `tail` 指针让尾插 O(1)，但删除时记得同步更新它。
5. 用 `malloc(sizeof *n)` 而不是 `malloc(sizeof(Node))`。
6. **用数组代替链表**，除非你确实频繁在中间增删且已持有节点指针。
7. 泄漏检测用 `leaks`（普通二进制），内存错误用 ASan。

## 练习

1. **快慢指针**找链表中点：为什么 `fast = fast->next->next` 是安全的？
2. **判断链表有环**（Floyd 判圈算法）：找到入环点。
3. 合并两个有序链表，要求 O(1) 额外空间。
4. 给链表加一个「哨兵头节点」（dummy head），重新实现删除 —— 还需要二级指针吗？
5. 为什么实验 A 中链表只比数组慢一点，而实验 B 慢了 8 万倍？
