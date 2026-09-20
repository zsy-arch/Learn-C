/* demo.c —— 动态数组：手写一个可增长的数组
 *
 * 本实验要回答的问题：
 *   1. realloc 到底是怎么扩容的？为什么「翻倍」比「每次 +1」快那么多？
 *   2. 容量和长度有什么区别？为什么要分开存？
 *   3. 扩容时旧指针为什么会失效？
 *   4. 摊还复杂度（amortized complexity）是什么意思？
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "../timing.h"
#include "../bench.c"

/* ================================================================ */
/* 动态数组实现                                                       */
/* ================================================================ */
typedef struct {
    int    *data;
    size_t  len;        /* 已使用长度 */
    size_t  cap;        /* 已分配容量 */
    /* 统计信息：只用于教学演示 */
    size_t  grows;      /* 扩容次数 */
    size_t  moves;      /* 元素搬移总次数（上界估算：假设 realloc 一定会搬） */
} DynArr;

static bool da_init(DynArr *a, size_t initial_cap)
{
    a->len = 0;
    a->cap = initial_cap;
    a->grows = 0;
    a->moves = 0;
    a->data = (initial_cap > 0) ? malloc(initial_cap * sizeof *a->data) : NULL;
    return initial_cap == 0 || a->data != NULL;
}

static void da_free(DynArr *a)
{
    if (a == NULL) { return; }
    free(a->data);
    a->data = NULL;
    a->len = a->cap = 0;
}

/* 确保容量至少为 need。扩容策略由参数控制，方便对比。 */
static bool da_reserve(DynArr *a, size_t need, size_t growth_factor)
{
    if (need <= a->cap) { return true; }

    size_t newcap;
    if (growth_factor == 0) {
        newcap = need;                    /* 精确扩容（每次只加需要的量） */
    } else {
        newcap = (a->cap == 0) ? 1 : a->cap;
        while (newcap < need) {
            /* 溢出保护：翻倍之前先确认不会绕回 */
            if (newcap > SIZE_MAX / 2) { newcap = need; break; }
            newcap *= growth_factor;
        }
    }

    /* 乘法溢出检查 */
    if (newcap > SIZE_MAX / sizeof *a->data) { return false; }

    int *tmp = realloc(a->data, newcap * sizeof *a->data);
    if (tmp == NULL) { return false; }    /* 原数据保持不变，没泄漏 */

    a->data = tmp;
    /* 注意：这是【上界估算】。实际上 realloc 在小幅增长时经常原地扩展，
     * 一个元素都不用搬。真正的代价要用耗时来衡量（见第 2 节的计时）。 */
    a->moves += a->len;
    a->cap  = newcap;
    a->grows++;
    return true;
}

static bool da_push(DynArr *a, int v, size_t growth_factor)
{
    if (!da_reserve(a, a->len + 1, growth_factor)) { return false; }
    a->data[a->len++] = v;
    return true;
}

static int da_get(const DynArr *a, size_t i)
{
    /* 无符号比较：传 -1 会变成巨大值，一样被拦住 */
    if (i >= a->len) { return -1; }
    return a->data[i];
}

static bool da_set(DynArr *a, size_t i, int v)
{
    if (i >= a->len) { return false; }
    a->data[i] = v;
    return true;
}

/* 缩容到刚好装下 */
static bool da_shrink_to_fit(DynArr *a)
{
    if (a->len == a->cap) { return true; }
    if (a->len == 0) { da_free(a); return true; }
    int *tmp = realloc(a->data, a->len * sizeof *a->data);
    if (tmp == NULL) { return false; }
    a->data = tmp;
    a->cap  = a->len;
    return true;
}

/* ================================================================ */
int main(void)
{
    puts("================ 动态数组实验 ================\n");

    /* ---------- 1. 基本操作 ---------- */
    puts("========== 1. 基本操作：len vs cap ==========");
    {
        DynArr a;
        if (!da_init(&a, 0)) { return 1; }

        printf("  %-10s %-8s %-8s %s\n", "操作", "len", "cap", "说明");
        for (int i = 1; i <= 9; i++) {
            da_push(&a, i * 10, 2);
            printf("  push(%-4d) %-8zu %-8zu %s\n", i * 10, a.len, a.cap,
                   (a.cap == a.len) ? "<- 刚刚扩容" : "");
        }
        puts("     -> cap 总是 >= len，且按 1,2,4,8,16... 翻倍增长");
        puts("     -> len 是「用了多少」，cap 是「分配了多少」");

        printf("\n  内容: ");
        for (size_t i = 0; i < a.len; i++) { printf("%d ", a.data[i]); }
        putchar('\n');

        printf("  da_get(&a, 3)  = %d\n", da_get(&a, 3));
        printf("  da_get(&a, 99) = %d (越界返回 -1，不崩溃)\n", da_get(&a, 99));
        printf("  da_set(&a, 0, 999) = %s, 现在 a[0]=%d\n",
               da_set(&a, 0, 999) ? "成功" : "失败", da_get(&a, 0));

        size_t cap_before = a.cap;
        da_shrink_to_fit(&a);
        printf("  shrink_to_fit: cap %zu -> %zu (= len)\n", cap_before, a.cap);

        da_free(&a);
        printf("  free 后: len=%zu cap=%zu data=%p\n", a.len, a.cap, (void *)a.data);
    }

    /* ---------- 2. 扩容策略对比 ---------- */
    puts("\n========== 2. 扩容策略对比：翻倍 vs 每次 +1 ==========");
    {
        const size_t N = 100000;

        /* (a) 每次只扩 1 个（等价于每次 push 都 realloc） */
        {
            DynArr a;
            da_init(&a, 0);
            Timer t = timer_start("");
            for (size_t i = 0; i < N; i++) {
                da_push(&a, (int)i, 0);      /* factor=0 -> 精确扩容 */
            }
            double ms = timer_stop(t);
            printf("  %-26s %8.3f ms   扩容 %zu 次，搬移上界 %zu 个元素\n",
                   "每次 +1 (factor=0)", ms, a.grows, a.moves);
            da_free(&a);
        }

        /* (b) 翻倍 */
        {
            DynArr a;
            da_init(&a, 0);
            Timer t = timer_start("");
            for (size_t i = 0; i < N; i++) {
                da_push(&a, (int)i, 2);      /* factor=2 -> 翻倍 */
            }
            double ms = timer_stop(t);
            printf("  %-26s %8.3f ms   扩容 %zu 次，搬移上界 %zu 个元素\n",
                   "翻倍 (factor=2)", ms, a.grows, a.moves);
            da_free(&a);
        }

        /* (c) 1.5 倍 */
        {
            DynArr a;
            da_init(&a, 0);
            Timer t = timer_start("");
            for (size_t i = 0; i < N; i++) {
                /* factor 是整数参数，1.5 倍这里手动算：cap + cap/2 + 1 */
                if (a.len == a.cap) {
                    size_t nc = a.cap + a.cap / 2 + 1;
                    da_reserve(&a, nc, 0);
                }
                da_push(&a, (int)i, 0);
            }
            double ms = timer_stop(t);
            printf("  %-26s %8.3f ms   扩容 %zu 次，搬移上界 %zu 个元素\n",
                   "1.5 倍", ms, a.grows, a.moves);
            da_free(&a);
        }

        printf("\n  N = %zu\n", N);
        puts("  理论分析：");
        puts("    每次 +1  ：第 k 次 push 搬 k 个元素，总计 1+2+...+N = N²/2");
        puts("    翻倍     ：第 k 次扩容搬 k 个元素，总量 1+2+4+...+N < 2N");
        puts("    -> 翻倍策略的【摊还】复杂度是 O(1)，每次 +1 是 O(n)");
        puts("    -> 注意：单次 push 最坏仍是 O(n)（正好赶上扩容），");
        puts("       但 n 次 push 平均下来是 O(1) —— 这就是「摊还」的含义。");
        puts("");
        puts("  ⚠️ 「搬移上界」那一列是【上界】而不是真实值：");
        puts("     realloc 在小幅增长时常常原地扩展，一个元素都不用搬。");
        puts("     真实的代价看【耗时】那一列，那才是硬件实际做了什么。");
    }

    /* ---------- 3. 扩容时指针失效 ---------- */
    puts("\n========== 3. ⚠️ 扩容会让所有旧指针失效 ==========");
    {
        DynArr a;
        da_init(&a, 2);
        da_push(&a, 10, 2);
        da_push(&a, 20, 2);

        int *p_first = &a.data[0];              /* 保存指向第一个元素的指针 */
        uintptr_t before = (uintptr_t)a.data;
        printf("  初始: data=%p, &data[0]=%p, cap=%zu\n",
               (void *)a.data, (void *)p_first, a.cap);

        /* 一次性申请很大的新容量 —— 这样 realloc 一定得搬家，
         * 而不是「原地扩展」（小步扩容时原地扩展很常见） */
        da_reserve(&a, 100000, 2);
        printf("  da_reserve(100000) 后: data=%p, cap=%zu\n",
               (void *)a.data, a.cap);

        bool moved = ((uintptr_t)a.data != before);
        printf("  旧指针 p_first=%p %s\n", (void *)p_first,
               moved ? "（已失效！指向已被 free 的内存）"
                     : "（这次碰巧原地扩展了，换个分配器/尺寸就会搬家）");
        puts("");
        puts("  为什么会失效？realloc 需要更大空间时，如果当前块后面已经被占用，");
        puts("  它就会【在别处分配新块 -> 拷贝旧数据 -> free 旧块】。");
        puts("  此时 p_first 就是【悬垂指针】，读它得到什么完全不可预测。");
        puts("");
        printf("  实测：a.data[0] = %d（用新指针取值是正确的）\n", a.data[0]);
        if (moved) {
            puts("        而 *p_first 指向的是已被 free 的旧块 —— ASan 会直接报错");
        }
        printf("  正确做法：扩容后重新取地址 —— &a.data[0] = %p\n",
               (void *)&a.data[0]);
        puts("");
        puts("  这就是为什么所有动态数组文档都会写：");
        puts("    「任何可能改变容量的操作都会使迭代器/指针失效」");
        puts("  C++ 的 std::vector 也是同样的规则。");
        puts("");
        puts("  防御手段：");
        puts("    1. 不要在扩容后继续用旧指针 —— 用【下标】而不是指针");
        puts("    2. 用下标 + 每次都重新取地址（本实验的 da_get 就是这么做）");
        puts("    3. C++ 里用 index 而不是 iterator/reference");

        da_free(&a);
    }

    /* ---------- 4. 性能：动态数组 vs 预分配数组 ---------- */
    puts("\n========== 4. 性能对比：动态数组 vs 预分配数组 ==========");
    {
        const size_t N = 1000000;

        {
            int *fixed = malloc(N * sizeof *fixed);
            if (fixed == NULL) { return 1; }
            Timer t = timer_start("");
            for (size_t i = 0; i < N; i++) { fixed[i] = (int)i; }
            double ms = timer_stop(t);
            printf("  %-28s %8.3f ms\n", "预分配数组(malloc 一次)", ms);

            volatile long long sum = 0;
            t = timer_start("");
            for (size_t i = 0; i < N; i++) { sum += fixed[i]; }
            ms = timer_stop(t);
            printf("  %-28s %8.3f ms  (和 %lld)\n", "  ^ 遍历", ms, sum);
            free(fixed);
        }

        {
            DynArr a;
            da_init(&a, 0);
            Timer t = timer_start("");
            for (size_t i = 0; i < N; i++) { da_push(&a, (int)i, 2); }
            double ms = timer_stop(t);
            printf("  %-28s %8.3f ms  (扩容 %zu 次)\n", "动态数组(翻倍扩容)", ms, a.grows);

            volatile long long sum = 0;
            t = timer_start("");
            for (size_t i = 0; i < a.len; i++) { sum += a.data[i]; }
            ms = timer_stop(t);
            printf("  %-28s %8.3f ms  (和 %lld)\n", "  ^ 遍历", ms, sum);
            da_free(&a);
        }

        puts("");
        puts("  动态数组的额外开销来自：① realloc 的搬移 ② 最后可能浪费一半容量");
        puts("  但获得了「不知道要先分配多大」的灵活性 —— 这个交换通常是值得的。");
        puts("  注意【遍历】那一行两者差不多：一旦分配完成，访问模式完全一样。");
    }

    /* ---------- 5. 用动态数组实现「读入未知数量的输入」 ---------- */
    puts("\n========== 5. 典型用途：读入数量未知的数据 ==========");
    {
        /* 模拟从「外部」逐个读取数据，直到遇到负数 */
        int stream[] = {5, 3, 9, 1, 7, 2, 8, 4, 6, 0, -1};
        DynArr a;
        da_init(&a, 4);

        printf("  读到: ");
        for (size_t i = 0; stream[i] >= 0; i++) {
            printf("%d ", stream[i]);
            if (!da_push(&a, stream[i], 2)) { break; }
        }
        printf("(遇到 -1 停止)\n");
        printf("  共 %zu 个元素，容量 %zu\n", a.len, a.cap);

        /* 排序（用简洁的插入排序，避免引入 qsort 的比较函数样板） */
        for (size_t i = 1; i < a.len; i++) {
            int key = a.data[i];
            size_t j = i;
            while (j > 0 && a.data[j - 1] > key) { a.data[j] = a.data[j - 1]; j--; }
            a.data[j] = key;
        }
        printf("  排序后: ");
        for (size_t i = 0; i < a.len; i++) { printf("%d ", a.data[i]); }
        putchar('\n');
        puts("  这正是「读入未知行数的文件/网络数据」的标准做法。");

        da_free(&a);
    }

    /* ---------- 6. 边界与陷阱 ---------- */
    puts("\n========== 6. 边界与陷阱 ==========");
    {
        DynArr a;
        da_init(&a, 0);
        printf("  空数组 len=%zu cap=%zu data=%p\n", a.len, a.cap, (void *)a.data);

        printf("  空数组 da_get(&a, 0) = %d（返回 -1 而不是崩溃）\n", da_get(&a, 0));

        /* free 之后再用 */
        da_free(&a);
        da_free(&a);       /* 第二次 free 是安全的（内部已置 NULL） */
        printf("  连续 da_free 两次也安全，因为内部把 data 置成 NULL 了\n");

        puts("");
        puts("  常见错误清单：");
        puts("    ❌ da->data = realloc(da->data, n)   —— 失败时泄漏");
        puts("    ❌ 扩容后继续用旧的 data 指针");
        puts("    ❌ 用 len 当循环上界却在循环里 push（len 一直在变）");
        puts("    ❌ malloc(need * sizeof) 不检查乘法溢出");
        puts("    ❌ 忘记检查 malloc/realloc 返回值");
        puts("");
        puts("  正确写法（本实验 da_reserve 的模式）：");
        puts("      int *tmp = realloc(a->data, newcap * sizeof *a->data);");
        puts("      if (tmp == NULL) { return false; }   // 原数据仍然有效");
        puts("      a->data = tmp;");
    }

    puts("\n========== 7. 复杂度总结 ==========");
    puts("  ┌──────────────┬──────────────┬────────────────────────┐");
    puts("  │ 操作         │ 复杂度       │ 说明                   │");
    puts("  ├──────────────┼──────────────┼────────────────────────┤");
    puts("  │ 按下标访问   │ O(1)         │ 连续内存，这是最大优势 │");
    puts("  │ 尾部追加     │ 摊还 O(1)    │ 偶尔要扩容搬移         │");
    puts("  │ 中间插入     │ O(n)         │ 要搬移后面所有元素     │");
    puts("  │ 中间删除     │ O(n)         │ 同上                   │");
    puts("  │ 查找(未排序) │ O(n)         │ 只能线性扫描           │");
    puts("  │ 查找(已排序) │ O(log n)     │ 二分查找               │");
    puts("  └──────────────┴──────────────┴────────────────────────┘");
    puts("");
    puts("  和链表的对比：");
    puts("    数组：随机访问 O(1)，中间插入删除 O(n)，cache 友好");
    puts("    链表：随机访问 O(n)，已持指针时插入删除 O(1)，cache 差");
    puts("  -> 实践中数组几乎总是更快，除非你确实频繁在中间增删。");

    return 0;
}
