/* Performance measurement, not correctness testing -- see tests.c for
 * that. Compiled with -O2 (unlike demo.c/tests.c, which stay at -O0 -g
 * for sanitizer-friendly debugging) because these numbers are only
 * meaningful against real optimized code. Every bulk insert/delete is
 * still followed by bplustree_verify() so the shapes being timed are
 * confirmed-legal trees, not just assumed to be. */
#include "bplustree.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* uint32_t，不是 unsigned int：xorshift32 的周期和位移常数是在"精确 32
 * 位、溢出即截断"下推导的，而 unsigned int 的宽度是实现定义的（标准只
 * 保证 >= 16 位）。在主流平台上它恰好 32 位，所以这段代码"能跑"，但那
 * 是平台巧合不是语言保证。种子为 0 是 xorshift 的不动点，会产生恒为 0
 * 的序列，这里一并兜底。 */
static uint32_t xs_state;
static uint32_t xorshift32(void) {
    uint32_t x = xs_state;
    if (x == 0) x = 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return xs_state = x;
}

static void verify_or_die(const BPlusTree *t, const char *ctx) {
    char err[256];
    if (!bplustree_verify(t, err, sizeof err)) {
        fprintf(stderr, "verify failed after %s: %s\n", ctx, err);
        exit(1);
    }
}

static void section_insert_timing(void) {
    printf("=== insert timing across scale and t ===\n");
    int scales[] = {1000, 10000, 100000, 500000};
    int ts[] = {2, 4, 8, 16};
    for (size_t si = 0; si < sizeof(scales) / sizeof(scales[0]); si++) {
        for (size_t ti = 0; ti < sizeof(ts) / sizeof(ts[0]); ti++) {
            int n = scales[si], t_deg = ts[ti];
            /* 种子只跟 n 有关，不能带上 t_deg。这张表的目的是"固定 n，
             * 横向比较 4 个 t 值"，那 4 个 cell 就必须插入**同一串** key：
             * 一旦种子里混进 t_deg，每个 cell 拿到的是不同的随机键集，
             * 测出来的差异里就混进了"输入不同"这个额外变量，不再是纯粹
             * 的 t 的影响。n 很大时两串随机键统计上确实很接近，但这是可
             * 以零成本消掉的噪声，没有理由留着。 */
            xs_state = 12345u + (uint32_t)n;
            BPlusTree tree = bplustree_create(t_deg);

            clock_t start = clock();
            int inserted = 0;
            for (int i = 0; i < n; i++) {
                int k = (int)(xorshift32() % 2000000u);
                if (bplustree_insert(&tree, k, k)) inserted++;
            }
            double secs = (double)(clock() - start) / CLOCKS_PER_SEC;
            verify_or_die(&tree, "insert timing sweep");

            printf("n=%-7d t=%-3d height=%-3d keys=%-8d time=%.4fs (%.2f ops/ms)\n",
                n, t_deg, bplustree_height(&tree), bplustree_count_keys(&tree), secs,
                secs > 0 ? (double)inserted / (secs * 1000.0) : 0.0);
            bplustree_destroy(&tree);
        }
    }
}

static void section_range_query_scaling(void) {
    printf("\n=== range-query leaf-visit scaling (t=4, varying tree size and range width) ===\n");
    int scales[] = {1000, 10000, 100000, 500000};
    int widths[] = {10, 100, 1000};
    for (size_t si = 0; si < sizeof(scales) / sizeof(scales[0]); si++) {
        int n = scales[si];
        BPlusTree tree = bplustree_create(4);
        for (int i = 0; i < n; i++) bplustree_insert(&tree, i * 2, i * 2);
        verify_or_die(&tree, "range query fixture build");
        int total_leaves = bplustree_count_leaves(&tree);
        int low = (n * 2) / 3;

        for (size_t wi = 0; wi < sizeof(widths) / sizeof(widths[0]); wi++) {
            int width = widths[wi];
            int cap = width + 16;
            int *keys = malloc(sizeof(int) * (size_t)cap);
            int *vals = malloc(sizeof(int) * (size_t)cap);
            if (!keys || !vals) { fprintf(stderr, "out of memory allocating range buffers\n"); exit(1); }
            int visited;
            int found = bplustree_range_query(&tree, low, low + width, keys, vals, cap, &visited);
            printf("n=%-8d total_leaves=%-8d range_width=%-6d found=%-6d visited_leaves=%-6d (%.1f%% of all leaves)\n",
                n, total_leaves, width, found, visited,
                total_leaves > 0 ? (100.0 * visited) / total_leaves : 0.0);
            free(keys);
            free(vals);
        }
        bplustree_destroy(&tree);
    }
}

static void section_delete_timing(void) {
    printf("\n=== delete timing (t=4, 100000 random insert then 100000 random delete of same keys) ===\n");
    int n = 100000;
    xs_state = 424242u;
    BPlusTree tree = bplustree_create(4);
    enum { KEY_SPACE = 5000000 };
    int *keys = malloc(sizeof(int) * (size_t)n);
    /* malloc 必须检查。这是性能程序而不是测试程序，但"测不出数"和"拿着
     * NULL 往下写"是两件完全不同的事——后者是 UB，而且崩在哪一步跟真正
     * 的原因（内存不够）毫无关系，排查起来极其误导。 */
    if (!keys) { fprintf(stderr, "out of memory allocating %d keys\n", n); exit(1); }

    /* 去重用位图，不用对已收集的 key 做线性扫描。
     * 原来的写法是 `for (j = 0; j < distinct; j++) if (keys[j] == k)`，
     * 这是 O(n^2)：n=100000 时实测约 1.2s（-O2）。它不影响任何一个被
     * 计时的数字（去重发生在 clock() 之前），所以不是错误结果，只是这
     * 个"性能测量程序"自己的准备阶段带着一段二次开销，规模再往上抬会很
     * 快失控（n=10^6 时是分钟级）。键空间是固定的 [0, 5000000)，位图只
     * 要 625KB，一次分配换掉整个二次扫描。 */
    unsigned char *seen = calloc((size_t)KEY_SPACE / 8 + 1, 1);
    if (!seen) { fprintf(stderr, "out of memory allocating dedup bitmap\n"); exit(1); }
    int distinct = 0;
    while (distinct < n) {
        int k = (int)(xorshift32() % (uint32_t)KEY_SPACE);
        if (seen[k >> 3] & (unsigned char)(1u << (k & 7))) continue;
        seen[k >> 3] |= (unsigned char)(1u << (k & 7));
        keys[distinct++] = k;
    }
    free(seen);

    clock_t start = clock();
    for (int i = 0; i < distinct; i++) bplustree_insert(&tree, keys[i], keys[i]);
    double insert_secs = (double)(clock() - start) / CLOCKS_PER_SEC;
    verify_or_die(&tree, "delete timing insert phase");

    for (int i = distinct - 1; i > 0; i--) {
        int j = (int)(xorshift32() % (uint32_t)(i + 1));
        int tmp = keys[i]; keys[i] = keys[j]; keys[j] = tmp;
    }

    start = clock();
    for (int i = 0; i < distinct; i++) bplustree_delete(&tree, keys[i]);
    double delete_secs = (double)(clock() - start) / CLOCKS_PER_SEC;
    verify_or_die(&tree, "delete timing delete phase");

    printf("insert %d keys: %.4fs, delete %d keys: %.4fs, final keys=%d (expect 0)\n",
        distinct, insert_secs, distinct, delete_secs, bplustree_count_keys(&tree));

    free(keys);
    bplustree_destroy(&tree);
}

int main(void) {
    section_insert_timing();
    section_range_query_scaling();
    section_delete_timing();
    return 0;
}
