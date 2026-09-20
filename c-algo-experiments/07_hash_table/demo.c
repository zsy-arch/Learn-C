/* demo.c —— 哈希表：哈希函数、冲突解决、负载因子、扩容
 *
 * 本实验要回答的问题：
 *   1. 哈希函数怎么设计？为什么常用「乘一个数再取模」？
 *   2. 冲突（collision）不可避免 —— 链地址法和开放寻址法各有什么代价？
 *   3. 负载因子（load factor）是什么？为什么必须扩容？
 *   4. 哈希表真的 O(1) 吗？最坏情况呢？
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "../timing.h"
#include "../bench.c"

/* ================================================================ */
/* 第一部分：几种哈希函数的对比                                        */
/* ================================================================ */

/* H1: 最简单 —— 直接取第一个字符 */
static uint64_t hash_first_char(const char *s)
{
    return (uint64_t)(unsigned char)s[0];
}

/* H2: 字符求和（最差的常见选择） */
static uint64_t hash_sum(const char *s)
{
    uint64_t h = 0;
    for (; *s; s++) { h += (unsigned char)*s; }
    return h;
}

/* H3: 多项式滚动哈希 —— djb2 的简化版
 *
 *   h = h * 31 + c
 *
 * 为什么乘一个数？因为纯相加的哈希对「字母重排」完全不敏感：
 * "abc" 和 "cba" 的和相同！乘一个数让位置信息参与进来。
 */
static uint64_t hash_poly31(const char *s)
{
    uint64_t h = 0;
    for (; *s; s++) { h = h * 31u + (unsigned char)*s; }
    return h;
}

/* H4: djb2 —— Dan Bernstein 的经典哈希 */
static uint64_t hash_djb2(const char *s)
{
    uint64_t h = 5381;
    for (; *s; s++) { h = h * 33u + (unsigned char)*s; }
    return h;
}

/* H5: FNV-1a —— 分布很好，实现也简单
 *
 * 先异或再乘。异或让低位也参与，乘法把影响扩散到高位。
 */
static uint64_t hash_fnv1a(const char *s)
{
    uint64_t h = 1469598103934665603ULL;      /* FNV offset basis (64-bit) */
    for (; *s; s++) {
        h ^= (unsigned char)*s;
        h *= 1099511628211ULL;                 /* FNV prime (64-bit) */
    }
    return h;
}

/* ---------------------------------------------------------------- */
/* 评估一个哈希函数：把一组 key 映射到桶里，统计冲突情况                 */
/* ---------------------------------------------------------------- */
typedef uint64_t (*HashFn)(const char *);

static void eval_hash(const char *name, HashFn fn, const char *const *keys,
                      size_t nkeys, size_t nbuckets)
{
    size_t *count = calloc(nbuckets, sizeof *count);
    if (count == NULL) { return; }

    for (size_t i = 0; i < nkeys; i++) {
        size_t b = (size_t)(fn(keys[i]) % nbuckets);
        count[b]++;
    }

    size_t used = 0, max_chain = 0;
    double sum_sq = 0;
    for (size_t b = 0; b < nbuckets; b++) {
        if (count[b] > 0) { used++; }
        if (count[b] > max_chain) { max_chain = count[b]; }
        sum_sq += (double)count[b] * (double)count[b];
    }

    /* 评估指标：
     *   桶使用率 = used / nbuckets          越高越好（空间利用）
     *   最长链   = max_chain                越小越好（最坏查询）
     *   平均链长 = nkeys / nbuckets         理想值
     *   离散度   = sum(count^2)/nkeys       越接近 1 + 平均链长 越好
     */
    printf("  %-14s 桶使用率=%5.1f%%  最长链=%zu  平均链长=%.2f  离散度=%.3f\n",
           name, 100.0 * (double)used / (double)nbuckets,
           max_chain, (double)nkeys / (double)nbuckets,
           sum_sq / (double)nkeys);

    free(count);
}

/* 生成测试用的 key 集合 */
#define NKEYS 2000
static const char *g_keys[NKEYS];
static char g_keybuf[NKEYS][12];

static void gen_keys(void)
{
    for (size_t i = 0; i < NKEYS; i++) {
        /* 形如 "key0000" .. "key1999" */
        snprintf(g_keybuf[i], sizeof g_keybuf[i], "key%04zu", i);
        g_keys[i] = g_keybuf[i];
    }
}

/* 另一组：故意构造字母重排的 key，暴露求和型哈希的弱点 */
#define NANA 64
static const char *g_ana[NANA];
static char g_anabuf[NANA][8];

static void gen_anagrams(void)
{
    /* 生成 a..d 的 3 字符重复排列，共 4^3 = 64 个 */
    size_t idx = 0;
    for (char a = 'a'; a <= 'd' && idx < NANA; a++) {
        for (char b = 'a'; b <= 'd' && idx < NANA; b++) {
            for (char c = 'a'; c <= 'd' && idx < NANA; c++) {
                g_anabuf[idx][0] = a;
                g_anabuf[idx][1] = b;
                g_anabuf[idx][2] = c;
                g_anabuf[idx][3] = '\0';
                g_ana[idx] = g_anabuf[idx];
                idx++;
            }
        }
    }
}

/* ================================================================ */
/* 第二部分：链地址法哈希表（separate chaining）                        */
/* ================================================================ */
/*
 * 每个桶是一条链表。冲突了就挂到链上。
 * 优点：实现简单，负载因子可以 > 1
 * 缺点：每次查找要跟随指针（cache 不友好）；有额外的指针开销
 */
typedef struct HNode {
    char         *key;      /* 拥有这份内存 */
    int           value;
    struct HNode *next;
    uint64_t      hash;     /* 缓存哈希值：rehash 时不用重算 */
} HNode;

typedef struct {
    HNode  **buckets;
    size_t   nbuckets;
    size_t   count;         /* 已存元素数 */
    HashFn   hash;
} HashTable;

#define HT_INIT_BUCKETS 16
#define HT_MAX_LOAD     0.75

static HashTable *ht_new(HashFn fn)
{
    HashTable *t = malloc(sizeof *t);
    if (t == NULL) { return NULL; }
    t->buckets = calloc(HT_INIT_BUCKETS, sizeof *t->buckets);
    if (t->buckets == NULL) { free(t); return NULL; }
    t->nbuckets = HT_INIT_BUCKETS;
    t->count    = 0;
    t->hash     = fn;
    return t;
}

static void ht_free(HashTable *t)
{
    if (t == NULL) { return; }
    for (size_t b = 0; b < t->nbuckets; b++) {
        HNode *c = t->buckets[b];
        while (c != NULL) {
            HNode *next = c->next;
            free(c->key);
            free(c);
            c = next;
        }
    }
    free(t->buckets);
    free(t);
}

/* 扩容：桶数翻倍（通常取素数或 2 的幂，各有讲究） */
static bool ht_grow(HashTable *t)
{
    size_t newn = t->nbuckets * 2;
    HNode **nb = calloc(newn, sizeof *nb);
    if (nb == NULL) { return false; }

    /* 重新分配所有节点 —— 注意：这里用的是【缓存的 hash】，
     * 所以不需要重新计算字符串哈希（虽然还是要取模） */
    for (size_t b = 0; b < t->nbuckets; b++) {
        HNode *c = t->buckets[b];
        while (c != NULL) {
            HNode *next = c->next;
            size_t nb_idx = (size_t)(c->hash % newn);
            c->next = nb[nb_idx];
            nb[nb_idx] = c;
            c = next;
        }
    }
    free(t->buckets);
    t->buckets  = nb;
    t->nbuckets = newn;
    return true;
}

static bool ht_put(HashTable *t, const char *key, int value)
{
    if (t == NULL || key == NULL) { return false; }

    /* 负载因子检查：超过阈值就扩容 */
    if ((double)(t->count + 1) / (double)t->nbuckets > HT_MAX_LOAD) {
        if (!ht_grow(t)) { return false; }
    }

    uint64_t h = t->hash(key);
    size_t   b = (size_t)(h % t->nbuckets);

    for (HNode *c = t->buckets[b]; c != NULL; c = c->next) {
        if (c->hash == h && strcmp(c->key, key) == 0) {
            c->value = value;          /* 已存在：覆盖 */
            return true;
        }
    }

    HNode *n = malloc(sizeof *n);
    if (n == NULL) { return false; }
    size_t klen = strlen(key);
    n->key = malloc(klen + 1);
    if (n->key == NULL) { free(n); return false; }
    memcpy(n->key, key, klen + 1);
    n->value = value;
    n->hash  = h;
    n->next  = t->buckets[b];
    t->buckets[b] = n;
    t->count++;
    return true;
}

static bool ht_get(const HashTable *t, const char *key, int *out)
{
    if (t == NULL || key == NULL) { return false; }
    uint64_t h = t->hash(key);
    size_t   b = (size_t)(h % t->nbuckets);
    for (const HNode *c = t->buckets[b]; c != NULL; c = c->next) {
        if (c->hash == h && strcmp(c->key, key) == 0) {
            if (out != NULL) { *out = c->value; }
            return true;
        }
    }
    return false;
}

static bool ht_del(HashTable *t, const char *key)
{
    if (t == NULL || key == NULL) { return false; }
    uint64_t h = t->hash(key);
    size_t   b = (size_t)(h % t->nbuckets);

    HNode **cur = &t->buckets[b];       /* 二级指针：避免头节点特判 */
    while (*cur != NULL) {
        HNode *entry = *cur;
        if (entry->hash == h && strcmp(entry->key, key) == 0) {
            *cur = entry->next;
            free(entry->key);
            free(entry);
            t->count--;
            return true;
        }
        cur = &entry->next;
    }
    return false;
}

static void ht_stats(const HashTable *t, const char *tag)
{
    size_t used = 0, maxchain = 0;
    for (size_t b = 0; b < t->nbuckets; b++) {
        size_t len = 0;
        for (const HNode *c = t->buckets[b]; c != NULL; c = c->next) { len++; }
        if (len > 0) { used++; }
        if (len > maxchain) { maxchain = len; }
    }
    printf("  %-22s count=%5zu nbuckets=%5zu 负载因子=%.3f 使用桶=%zu 最长链=%zu\n",
           tag, t->count, t->nbuckets,
           (double)t->count / (double)t->nbuckets, used, maxchain);
}

/* ================================================================ */
/* 第三部分：开放寻址法（open addressing，线性探测）                     */
/* ================================================================ */
/*
 * 不存指针，key 直接放在桶数组里。冲突了就往后找下一个空位。
 *
 * 优点：内存紧凑（cache 友好），没有指针开销
 * 缺点：
 *   - 删除麻烦（删掉会在探测链上留一个「洞」，后续查找会断）
 *     解决：用「墓碑」（tombstone）标记
 *   - 负载因子必须 < 1，通常控制在 0.5~0.7，否则性能急剧下降
 */
#define OA_EMPTY 0
#define OA_USED  1
#define OA_TOMB  2

typedef struct {
    char    *key;
    int      value;
    uint64_t hash;
    int      state;         /* EMPTY / USED / TOMB */
} OASlot;

typedef struct {
    OASlot *slots;
    size_t  cap;
    size_t  used;           /* USED 的数量 */
    size_t  tombs;          /* TOMB 的数量 */
    HashFn  hash;
} OpenTable;

static OpenTable *oa_new(size_t cap, HashFn fn)
{
    OpenTable *t = malloc(sizeof *t);
    if (t == NULL) { return NULL; }
    t->slots = calloc(cap, sizeof *t->slots);
    if (t->slots == NULL) { free(t); return NULL; }
    t->cap   = cap;
    t->used  = 0;
    t->tombs = 0;
    t->hash  = fn;
    return t;
}

static void oa_free(OpenTable *t)
{
    if (t == NULL) { return; }
    for (size_t i = 0; i < t->cap; i++) {
        if (t->slots[i].state == OA_USED) { free(t->slots[i].key); }
    }
    free(t->slots);
    free(t);
}

static bool oa_rehash(OpenTable *t, size_t newcap)
{
    OASlot *old = t->slots;
    size_t oldcap = t->cap;

    OASlot *ns = calloc(newcap, sizeof *ns);
    if (ns == NULL) { return false; }
    t->slots = ns;
    t->cap   = newcap;
    t->used  = 0;
    t->tombs = 0;

    for (size_t i = 0; i < oldcap; i++) {
        if (old[i].state != OA_USED) { continue; }
        /* 重新插入（新表里没有墓碑，所以一定能放下） */
        size_t b = (size_t)(old[i].hash % newcap);
        while (ns[b].state == OA_USED) { b = (b + 1) % newcap; }
        ns[b] = old[i];
        t->used++;
    }
    free(old);
    return true;
}

static bool oa_put(OpenTable *t, const char *key, int value)
{
    if (t == NULL || key == NULL) { return false; }

    /* 开放寻址的负载因子必须留有余量（算上墓碑） */
    if ((double)(t->used + t->tombs + 1) / (double)t->cap > 0.5) {
        if (!oa_rehash(t, t->cap * 2)) { return false; }
    }

    uint64_t h = t->hash(key);
    size_t b = (size_t)(h % t->cap);
    size_t first_tomb = (size_t)-1;

    for (size_t probe = 0; probe < t->cap; probe++) {
        OASlot *s = &t->slots[b];
        if (s->state == OA_EMPTY) {
            /* 找到空位。如果之前路过墓碑，优先用那个墓碑位置 */
            OASlot *dst = (first_tomb != (size_t)-1) ? &t->slots[first_tomb] : s;
            if (dst->state == OA_TOMB) { t->tombs--; }
            size_t klen = strlen(key);
            dst->key = malloc(klen + 1);
            if (dst->key == NULL) { return false; }
            memcpy(dst->key, key, klen + 1);
            dst->value = value;
            dst->hash  = h;
            dst->state = OA_USED;
            t->used++;
            return true;
        }
        if (s->state == OA_USED && s->hash == h && strcmp(s->key, key) == 0) {
            s->value = value;          /* 已存在 */
            return true;
        }
        if (s->state == OA_TOMB && first_tomb == (size_t)-1) {
            first_tomb = b;
        }
        b = (b + 1) % t->cap;          /* 线性探测 */
    }
    return false;                      /* 表满（理论上被负载因子挡住了） */
}

static bool oa_get(const OpenTable *t, const char *key, int *out)
{
    if (t == NULL || key == NULL) { return false; }
    uint64_t h = t->hash(key);
    size_t b = (size_t)(h % t->cap);

    for (size_t probe = 0; probe < t->cap; probe++) {
        const OASlot *s = &t->slots[b];
        if (s->state == OA_EMPTY) { return false; }   /* 碰到空位 -> 一定不存在 */
        if (s->state == OA_USED && s->hash == h && strcmp(s->key, key) == 0) {
            if (out != NULL) { *out = s->value; }
            return true;
        }
        b = (b + 1) % t->cap;
    }
    return false;
}

static bool oa_del(OpenTable *t, const char *key)
{
    if (t == NULL || key == NULL) { return false; }
    uint64_t h = t->hash(key);
    size_t b = (size_t)(h % t->cap);

    for (size_t probe = 0; probe < t->cap; probe++) {
        OASlot *s = &t->slots[b];
        if (s->state == OA_EMPTY) { return false; }
        if (s->state == OA_USED && s->hash == h && strcmp(s->key, key) == 0) {
            free(s->key);
            s->key = NULL;
            s->state = OA_TOMB;        /* 关键：不能设成 EMPTY！ */
            t->used--;
            t->tombs++;
            return true;
        }
        b = (b + 1) % t->cap;
    }
    return false;
}

static void oa_stats(const OpenTable *t, const char *tag)
{
    size_t empty = 0, used = 0, tomb = 0;
    for (size_t i = 0; i < t->cap; i++) {
        switch (t->slots[i].state) {
        case OA_EMPTY: empty++; break;
        case OA_USED:  used++;  break;
        case OA_TOMB:  tomb++;  break;
        default: break;
        }
    }
    printf("  %-22s cap=%5zu used=%4zu tomb=%3zu empty=%5zu 负载=%.3f\n",
           tag, t->cap, used, tomb, empty, (double)used / (double)t->cap);
}

/* ================================================================ */
int main(void)
{
    puts("================ 哈希表实验 ================\n");

    /* ---------- 1. 哈希函数质量对比 ---------- */
    puts("========== 1. 哈希函数质量对比（2000 个 key，1000 个桶）==========");
    {
        gen_keys();
        gen_anagrams();      /* 两组测试数据都先准备好 */
        puts("  key 形式: \"key0000\" .. \"key1999\"");
        eval_hash("first_char", hash_first_char, g_keys, NKEYS, 1000);
        eval_hash("sum",        hash_sum,        g_keys, NKEYS, 1000);
        eval_hash("poly31",     hash_poly31,     g_keys, NKEYS, 1000);
        eval_hash("djb2",       hash_djb2,       g_keys, NKEYS, 1000);
        eval_hash("fnv1a",      hash_fnv1a,      g_keys, NKEYS, 1000);
        puts("");
        puts("  first_char: 所有 key 都以 'k' 开头 -> 全部落进同一个桶！最长链 = 2000");
        puts("  sum:        'key0000'..'key1999' 的字符和都接近，分布也很差");
        puts("  poly31/djb2/fnv1a: 分布均匀，最长链接近理论值");
    }

    /* ---------- 2. 字母重排测试 ---------- */
    puts("\n========== 2. 字母重排（anagram）测试：暴露求和型哈希的弱点 ==========");
    {
        puts("  key 是 a..d 的 3 字符重复排列，共 64 个");
        eval_hash("sum",    hash_sum,    g_ana, NANA, 256);
        eval_hash("poly31", hash_poly31, g_ana, NANA, 256);
        eval_hash("djb2",   hash_djb2,   g_ana, NANA, 256);
        puts("");
        puts("  sum 版本：\"abc\" 和 \"cba\" 的字符和都是 294 —— 它们必然冲突！");
        puts("  乘一个数（poly31/djb2）让字符的【位置】参与运算，就不会这样。");
    }

    /* ---------- 3. 链地址法哈希表 ---------- */
    puts("\n========== 3. 链地址法哈希表 ==========");
    {
        HashTable *t = ht_new(hash_djb2);
        if (t == NULL) { return 1; }

        ht_stats(t, "初始状态:");

        for (size_t i = 0; i < NKEYS; i++) {
            char key[16];
            snprintf(key, sizeof key, "key%04zu", i);
            ht_put(t, key, (int)i);
        }
        ht_stats(t, "插入 2000 个之后:");

        int v = 0;
        printf("  get(\"key0000\") = %s %d\n",
               ht_get(t, "key0000", &v) ? "命中" : "未命中", v);
        printf("  get(\"key1999\") = %s %d\n",
               ht_get(t, "key1999", &v) ? "命中" : "未命中", v);
        printf("  get(\"nokey\")   = %s\n",
               ht_get(t, "nokey", &v) ? "命中" : "未命中");

        puts("\n  覆盖已有 key（值应该更新，count 不变）:");
        size_t before = t->count;
        ht_put(t, "key0000", 999);
        ht_get(t, "key0000", &v);
        printf("    key0000 -> %d, count %zu -> %zu (%s)\n",
               v, before, t->count, (before == t->count) ? "正确" : "错误");

        puts("\n  删除测试:");
        printf("    del(\"key0000\") = %d\n", ht_del(t, "key0000"));
        printf("    get(\"key0000\") = %s\n",
               ht_get(t, "key0000", &v) ? "仍然存在(错误)" : "已删除(正确)");
        printf("    del(\"key0000\") 再来一次 = %d (第二次应该失败)\n",
               ht_del(t, "key0000"));

        ht_stats(t, "删除 1 个之后:");

        /* 删掉一半再验证 */
        for (size_t i = 0; i < NKEYS; i += 2) {
            char key[16];
            snprintf(key, sizeof key, "key%04zu", i);
            ht_del(t, key);
        }
        ht_stats(t, "删掉一半之后:");

        int still_ok = 1;
        for (size_t i = 1; i < NKEYS; i += 2) {
            char key[16];
            snprintf(key, sizeof key, "key%04zu", i);
            if (!ht_get(t, key, &v) || v != (int)i) { still_ok = 0; break; }
        }
        printf("  剩下的奇数 key 全部能找到: %s\n", still_ok ? "是" : "否");

        ht_free(t);
    }

    /* ---------- 4. 开放寻址法 ---------- */
    puts("\n========== 4. 开放寻址法（线性探测 + 墓碑）==========");
    {
        OpenTable *t = oa_new(16, hash_djb2);
        if (t == NULL) { return 1; }

        for (size_t i = 0; i < 100; i++) {
            char key[16];
            snprintf(key, sizeof key, "key%04zu", i);
            oa_put(t, key, (int)i);
        }
        oa_stats(t, "插入 100 个之后:");

        int v = 0;
        printf("  get(\"key0042\") = %s %d\n",
               oa_get(t, "key0042", &v) ? "命中" : "未命中", v);

        puts("\n  删除 30 个（观察墓碑数量）:");
        for (size_t i = 0; i < 30; i++) {
            char key[16];
            snprintf(key, sizeof key, "key%04zu", i);
            oa_del(t, key);
        }
        oa_stats(t, "删除 30 个之后:");

        int found = 0;
        for (size_t i = 30; i < 100; i++) {
            char key[16];
            snprintf(key, sizeof key, "key%04zu", i);
            if (oa_get(t, key, &v)) { found++; }
        }
        printf("  剩下 70 个 key 找到 %d 个\n", found);

        puts("");
        puts("  ★ 墓碑的关键作用：");
        puts("    如果删除时把槽位设成 EMPTY，那么查找遇到 EMPTY 就会停止探测。");
        puts("    但被删元素后面的元素可能就是因为冲突才被放到更后面的位置 ——");
        puts("    提前停止就会【找不到】它们。所以必须用 TOMB 把探测链续上。");

        oa_free(t);
    }

    /* ---------- 5. 性能：哈希 vs 线性搜索 ---------- */
    puts("\n========== 5. 性能对比：哈希表查找 vs 线性搜索 ==========");
    {
        const size_t N = 20000;
        char **keys = malloc(N * sizeof *keys);
        if (keys == NULL) { return 1; }
        for (size_t i = 0; i < N; i++) {
            keys[i] = malloc(16);
            if (keys[i] == NULL) { return 1; }
            snprintf(keys[i], 16, "key%05zu", i);
        }

        HashTable *t = ht_new(hash_fnv1a);
        if (t == NULL) { return 1; }
        for (size_t i = 0; i < N; i++) {
            ht_put(t, keys[i], (int)i);
        }
        ht_stats(t, "建表完成:");

        /* 哈希查找 N 次 */
        {
            volatile long long sum = 0;
            Timer tm = timer_start("");
            int v;
            for (size_t i = 0; i < N; i++) {
                if (ht_get(t, keys[i], &v)) { sum += v; }
            }
            double ms = timer_stop(tm);
            printf("  哈希查找 %zu 次: %10.3f ms  (校验和 %lld)\n", N, ms, sum);
        }

        /* 线性搜索 N/20 次（n^2 的代价，控制一下时间） */
        {
            const size_t Q = N / 20;
            volatile long long sum = 0;
            Timer tm = timer_start("");
            for (size_t i = 0; i < Q; i++) {
                for (size_t j = 0; j < N; j++) {
                    if (strcmp(keys[j], keys[i]) == 0) { sum += (long long)j; break; }
                }
            }
            double ms = timer_stop(tm);
            printf("  线性搜索 %zu 次: %10.3f ms  (校验和 %lld)\n", Q, ms, sum);
            printf("  -> 按每次查询平均，哈希是 O(1)，线性是 O(n)。\n");
        }

        for (size_t i = 0; i < N; i++) { free(keys[i]); }
        free(keys);
        ht_free(t);
    }

    puts("\n========== 6. 小结 ==========");
    puts("  1. 哈希函数要让字符的【位置】参与运算（乘一个数），纯求和会被字母重排击败");
    puts("  2. 链地址法：实现简单，负载因子可 >1，但指针跳转 cache 不友好");
    puts("  3. 开放寻址法：内存紧凑，但删除必须用墓碑，负载因子要 <0.5~0.7");
    puts("  4. 负载因子 = 元素数 / 桶数；超过阈值必须扩容，否则退化成链表");
    puts("  5. 哈希表平均 O(1)，【最坏 O(n)】—— 恶意构造的 key 可以让它退化成链表");
    puts("     这就是为什么 Web 框架要对用户输入做随机化哈希（如 SipHash）");

    return 0;
}
