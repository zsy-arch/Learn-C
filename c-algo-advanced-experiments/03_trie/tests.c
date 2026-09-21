/* tests.c —— Trie 多组测试用例，覆盖空树/重复/共享前缀/删除边界/大规模随机 */
#include "trie.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            printf("    assertion failed: %s (line %d)\n", #cond, __LINE__); \
            return false; \
        } \
    } while (0)

#define RUN_TEST(fn) \
    do { \
        bool ok = fn(); \
        if (ok) { \
            printf("[PASS] %s\n", #fn); \
            g_pass++; \
        } else { \
            printf("[FAIL] %s\n", #fn); \
            g_fail++; \
        } \
    } while (0)

/* ---------- 1. 空 Trie ---------- */
static bool test_empty_trie(void) {
    TrieNode *root = trie_create();
    CHECK(!trie_search(root, "anything"));
    CHECK(!trie_search(root, "a"));
    CHECK(trie_starts_with(root, ""));   /* 空前缀总是"匹配"，即使树里啥也没有 */
    CHECK(!trie_starts_with(root, "a"));
    trie_free(root);
    return true;
}

/* ---------- 2. 单个单词 ---------- */
static bool test_single_word(void) {
    TrieNode *root = trie_create();
    CHECK(trie_insert(root, "hello"));
    CHECK(trie_search(root, "hello"));
    CHECK(!trie_search(root, "hell"));   /* 前缀但非完整单词 */
    CHECK(!trie_search(root, "helloo"));
    CHECK(!trie_search(root, "world"));
    CHECK(trie_starts_with(root, "he"));
    CHECK(!trie_starts_with(root, "wo"));
    trie_free(root);
    return true;
}

/* ---------- 3. 插入重复单词：不能产生副作用 ---------- */
static bool test_duplicate_insert(void) {
    TrieNode *root = trie_create();
    trie_insert(root, "abc");
    size_t nodes_after_first = trie_count_nodes(root);
    trie_insert(root, "abc");
    trie_insert(root, "abc");
    size_t nodes_after_repeat = trie_count_nodes(root);

    CHECK(nodes_after_first == nodes_after_repeat); /* 节点数不应该增加 */
    CHECK(trie_search(root, "abc"));

    trie_delete(root, "abc"); /* 删一次就该彻底没了，不会因为"插入了三次"需要删三次 */
    CHECK(!trie_search(root, "abc"));
    trie_free(root);
    return true;
}

/* ---------- 4. 前缀共享：cat/car/card/care ---------- */
static bool test_shared_prefix_nodes(void) {
    TrieNode *root = trie_create();
    trie_insert(root, "cat");
    trie_insert(root, "car");
    trie_insert(root, "card");
    trie_insert(root, "care");

    /* root + c + a + t + r + d + e = 7 个节点，即使 4 个单词总长 3+3+4+4=14 */
    CHECK(trie_count_nodes(root) == 7);

    CHECK(trie_search(root, "cat"));
    CHECK(trie_search(root, "car"));
    CHECK(trie_search(root, "card"));
    CHECK(trie_search(root, "care"));
    CHECK(!trie_search(root, "ca"));
    CHECK(!trie_search(root, "cards"));

    trie_free(root);
    return true;
}

/* ---------- 5. 删除不存在的单词：不能崩溃，不能误删 ---------- */
static bool test_delete_nonexistent(void) {
    TrieNode *root = trie_create();
    trie_insert(root, "cat");
    size_t nodes_before = trie_count_nodes(root);

    trie_delete(root, "dog");    /* 完全不存在 */
    trie_delete(root, "ca");     /* 是前缀，不是完整单词 */
    trie_delete(root, "cats");   /* 存在单词的更长版本 */
    trie_delete(root, "");       /* 空字符串：is_valid_word 会拒绝，直接忽略 */

    CHECK(trie_count_nodes(root) == nodes_before); /* 什么都不该发生 */
    CHECK(trie_search(root, "cat"));                /* cat 完好无损 */

    trie_free(root);
    return true;
}

/* ---------- 6. 删除叶子单词：真正释放节点 ---------- */
static bool test_delete_leaf_frees_nodes(void) {
    TrieNode *root = trie_create();
    trie_insert(root, "dog");
    trie_insert(root, "cat"); /* 无关单词，用来确认它不受影响 */

    size_t before = trie_count_nodes(root); /* root+c+a+t+d+o+g = 7 */
    CHECK(before == 7);

    trie_delete(root, "dog");
    size_t after = trie_count_nodes(root); /* d/o/g 三个节点被释放 -> 剩 4 */
    CHECK(after == 4);
    CHECK(!trie_search(root, "dog"));
    CHECK(!trie_starts_with(root, "do"));
    CHECK(trie_search(root, "cat")); /* 无关单词不受影响 */

    trie_free(root);
    return true;
}

/* ---------- 7. 删除共享前缀里的一个单词：不能误删共享节点 ---------- */
static bool test_delete_shared_prefix_word(void) {
    TrieNode *root = trie_create();
    trie_insert(root, "cat");
    trie_insert(root, "car");
    trie_insert(root, "card");
    trie_insert(root, "care");

    size_t before = trie_count_nodes(root);
    trie_delete(root, "car"); /* car 被删，但 c/a/r 节点还被 card/care 用着 */
    size_t after = trie_count_nodes(root);

    CHECK(before == after); /* 节点数不变：只是清了 r 节点的 is_word 标记 */
    CHECK(!trie_search(root, "car"));   /* car 本身查不到了 */
    CHECK(trie_search(root, "card"));   /* card 完全不受影响 */
    CHECK(trie_search(root, "care"));   /* care 完全不受影响 */
    CHECK(trie_search(root, "cat"));    /* cat 更不该受影响 */
    CHECK(trie_starts_with(root, "car")); /* 前缀 "car" 依然存在（因为 card/care） */

    trie_free(root);
    return true;
}

/* ---------- 8. 删除根节点对应的"整棵树清空"场景 ---------- */
static bool test_delete_all_words_empties_trie(void) {
    TrieNode *root = trie_create();
    trie_insert(root, "a");
    trie_insert(root, "ab");
    trie_insert(root, "abc");

    trie_delete(root, "abc");
    trie_delete(root, "ab");
    trie_delete(root, "a");

    CHECK(trie_count_nodes(root) == 1); /* 只剩根节点 */
    CHECK(!trie_search(root, "a"));
    CHECK(!trie_search(root, "ab"));
    CHECK(!trie_search(root, "abc"));
    CHECK(!trie_starts_with(root, "a"));

    trie_free(root);
    return true;
}

/* ---------- 9. 前缀查找：存在/不存在/空前缀 ---------- */
static bool test_starts_with_variants(void) {
    TrieNode *root = trie_create();
    trie_insert(root, "apple");
    trie_insert(root, "app");
    trie_insert(root, "application");

    CHECK(trie_starts_with(root, "app"));
    CHECK(trie_starts_with(root, "appl"));
    CHECK(trie_starts_with(root, "apple"));
    CHECK(!trie_starts_with(root, "banana"));
    CHECK(!trie_starts_with(root, "apples")); /* apple 存在但 apples 不存在 */
    CHECK(trie_starts_with(root, ""));        /* 空前缀匹配一切 */

    trie_free(root);
    return true;
}

static int str_cmp_qsort(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

static bool wordlist_equals_sorted(WordList *list, const char **expected, size_t expected_n) {
    if (list->count != expected_n) {
        printf("    wordlist size mismatch: got %zu, expected %zu\n", list->count, expected_n);
        return false;
    }
    char **got = malloc(list->count * sizeof *got);
    for (size_t i = 0; i < list->count; i++) {
        got[i] = list->words[i];
    }
    qsort(got, list->count, sizeof *got, str_cmp_qsort);

    char **exp_copy = malloc(expected_n * sizeof *exp_copy);
    for (size_t i = 0; i < expected_n; i++) {
        exp_copy[i] = (char *)expected[i];
    }
    qsort(exp_copy, expected_n, sizeof *exp_copy, str_cmp_qsort);

    bool ok = true;
    for (size_t i = 0; i < expected_n; i++) {
        if (strcmp(got[i], exp_copy[i]) != 0) {
            ok = false;
            break;
        }
    }
    free(got);
    free(exp_copy);
    return ok;
}

/* ---------- 10. 自动补全：结果集合与预期完全一致（不依赖顺序） ---------- */
static bool test_autocomplete_matches_expected_set(void) {
    TrieNode *root = trie_create();
    const char *words[] = {"cat", "car", "card", "care", "careful", "dog", "do", "door"};
    for (size_t i = 0; i < sizeof words / sizeof words[0]; i++) {
        trie_insert(root, words[i]);
    }

    WordList ca = trie_autocomplete(root, "ca");
    const char *expect_ca[] = {"cat", "car", "card", "care", "careful"};
    CHECK(wordlist_equals_sorted(&ca, expect_ca, 5));
    wordlist_free(&ca);

    WordList doo = trie_autocomplete(root, "doo");
    const char *expect_doo[] = {"door"};
    CHECK(wordlist_equals_sorted(&doo, expect_doo, 1));
    wordlist_free(&doo);

    WordList none = trie_autocomplete(root, "xyz");
    CHECK(none.count == 0);
    wordlist_free(&none);

    WordList all = trie_autocomplete(root, "");
    CHECK(all.count == 8);
    wordlist_free(&all);

    trie_free(root);
    return true;
}

/* ---------- 11. 无效输入：拒绝空字符串/含非法字符的单词 ---------- */
static bool test_invalid_inputs_rejected(void) {
    TrieNode *root = trie_create();
    CHECK(!trie_insert(root, ""));
    CHECK(!trie_insert(root, NULL));
    CHECK(!trie_insert(root, "Hello"));  /* 大写字母不在合法字符集里 */
    CHECK(!trie_insert(root, "a1b"));    /* 数字也不合法 */
    CHECK(trie_count_nodes(root) == 1);  /* 只有根节点，什么都没插进去 */
    trie_free(root);
    return true;
}

/* ---------- 12. 大规模随机测试：插入 + 验证 + 随机删除 + 再验证 ---------- */
#define RANDOM_WORD_COUNT 5000
#define RANDOM_WORD_MAXLEN 8

static void gen_random_word(char *buf, unsigned *state) {
    /* 简单线性同余生成器，固定种子保证可重现，不依赖 rand() 的实现细节 */
    int len = 3 + (int)(*state % (RANDOM_WORD_MAXLEN - 2));
    for (int i = 0; i < len; i++) {
        *state = (*state) * 1103515245u + 12345u;
        buf[i] = (char)('a' + (*state >> 16) % 26);
    }
    buf[len] = '\0';
}

static bool test_large_random_insert_delete(void) {
    TrieNode *root = trie_create();
    unsigned state = 42; /* 固定种子 */

    char **words = malloc(RANDOM_WORD_COUNT * sizeof *words);
    for (int i = 0; i < RANDOM_WORD_COUNT; i++) {
        words[i] = malloc(RANDOM_WORD_MAXLEN + 1);
        gen_random_word(words[i], &state);
        trie_insert(root, words[i]);
    }

    /* 阶段一：全部应该能查到（重复生成的单词插入多次也无所谓，
     * Trie 里一个单词只对应一份存在状态，不是"插入次数"） */
    for (int i = 0; i < RANDOM_WORD_COUNT; i++) {
        if (!trie_search(root, words[i])) {
            printf("    生成后立即查找失败: \"%s\" (index %d)\n", words[i], i);
            for (int j = 0; j < RANDOM_WORD_COUNT; j++) free(words[j]);
            free(words);
            trie_free(root);
            return false;
        }
    }

    /* 阶段二：删除偶数下标对应的单词。
     *
     * 关键点：Trie 的存在性是按"字符串内容"而不是按"插入操作次数"记的——
     * 就像一个 set，insert(x); insert(x) 之后 x 只有一份，delete(x) 一次
     * 就彻底移除它，不存在"删一次还剩一次"的计数语义。
     * 所以如果 words[i] 和 words[k] (k != i) 内容相同，
     * 删除 words[i] 会让 words[k] 也查不到——这不是 bug，是正确行为。 */

    for (int i = 0; i < RANDOM_WORD_COUNT; i += 2) {
        trie_delete(root, words[i]);
    }

    /* 期望状态：trie_delete 是"按字符串内容删除"，不是"按插入次数删除"。
     * 循环 `for (i = 0; i < N; i += 2) trie_delete(root, words[i])`
     * 只要 words[i] 的内容在某个偶数下标出现过一次，这个 delete 调用
     * 就会把这个 distinct 单词彻底从 Trie 里删掉——即使它在别的
     * 奇数下标又"出现"过，那也只是同一个字符串内容的另一次记录，
     * 不会让它在 Trie 里"复活"。
     *
     * 所以一个 distinct 单词 W 最终存在，当且仅当它在整个数组里
     * **从未在任何偶数下标出现过**（一次都没被 delete 调用点中）。 */
    for (int i = 0; i < RANDOM_WORD_COUNT; i++) {
        bool ever_targeted_by_delete = false;
        for (int j = 0; j < RANDOM_WORD_COUNT; j += 2) {
            if (strcmp(words[j], words[i]) == 0) {
                ever_targeted_by_delete = true;
                break;
            }
        }
        bool should_exist = !ever_targeted_by_delete;
        bool actually_exists = trie_search(root, words[i]);
        if (should_exist != actually_exists) {
            printf("    删除后状态不一致: \"%s\" 期望 %s 实际 %s\n",
                   words[i], should_exist ? "存在" : "不存在", actually_exists ? "存在" : "不存在");
            for (int j = 0; j < RANDOM_WORD_COUNT; j++) free(words[j]);
            free(words);
            trie_free(root);
            return false;
        }
    }

    for (int i = 0; i < RANDOM_WORD_COUNT; i++) free(words[i]);
    free(words);
    trie_free(root);
    return true;
}

/* ---------- 13. trie_free 对 NULL / 空树都要安全 ---------- */
static bool test_free_null_and_empty_safe(void) {
    trie_free(NULL); /* 不能崩溃 */
    TrieNode *root = trie_create();
    trie_free(root); /* 只有根节点的树 */
    return true;
}

/* ---------- 14. 单词长度边界：能插进去的就一定能枚举出来 ----------
 *
 * 这条测试锁的是一个曾经真实存在的缺陷：autocomplete 用固定缓冲区拼单词，
 * 而插入路径不限长度，于是 256 字符的单词 insert=1、search=1、
 * autocomplete=0——数据在树里，但枚举不出来，返回的列表看不出任何异常。
 *
 * 修复方式不是把缓冲区改大（那只是把边界挪个位置），而是让两条路径
 * 共用 TRIE_MAX_WORD_LEN，把"可插入"和"可枚举"绑成同一个条件。 */
static bool make_word(char *buf, size_t len) {
    memset(buf, 'a', len);
    buf[len] = '\0';
    return true;
}

static bool test_word_length_boundary(void) {
    char word[TRIE_MAX_WORD_LEN + 64];

    /* 恰好等于上限：必须可插入，且必须能被枚举出来 */
    TrieNode *root = trie_create();
    make_word(word, TRIE_MAX_WORD_LEN);
    CHECK(trie_insert(root, word));
    CHECK(trie_search(root, word));
    WordList at_max = trie_autocomplete(root, "");
    CHECK(at_max.count == 1);              /* 关键：不是 0 */
    CHECK(!at_max.truncated);
    CHECK(strcmp(at_max.words[0], word) == 0);
    wordlist_free(&at_max);
    trie_free(root);

    /* 超过上限：必须在入口就被拒绝，不能"插进去但查不全" */
    root = trie_create();
    make_word(word, TRIE_MAX_WORD_LEN + 1);
    CHECK(!trie_insert(root, word));          /* 拒绝 */
    CHECK(!trie_search(root, word));          /* 树里确实没有 */
    CHECK(trie_count_nodes(root) == 1);       /* 一个节点都没建 */
    WordList empty = trie_autocomplete(root, "");
    CHECK(empty.count == 0);
    CHECK(!empty.truncated);                  /* 是真的空，不是被截断 */
    wordlist_free(&empty);

    /* 更长的也一样，并且删除这种词是安全的空操作 */
    make_word(word, TRIE_MAX_WORD_LEN + 45);
    CHECK(!trie_insert(root, word));
    trie_delete(root, word);                  /* 不能崩溃 */
    CHECK(trie_count_nodes(root) == 1);
    trie_free(root);

    /* 混合词库：短词正常，超长词被拒，结果集是完整且一致的 */
    root = trie_create();
    CHECK(trie_insert(root, "cat"));
    CHECK(trie_insert(root, "car"));
    make_word(word, TRIE_MAX_WORD_LEN + 10);
    CHECK(!trie_insert(root, word));
    WordList mixed = trie_autocomplete(root, "");
    CHECK(mixed.count == 2);
    CHECK(!mixed.truncated);
    wordlist_free(&mixed);
    trie_free(root);
    return true;
}

/* ---------- 15. trie_collect 的截断必须是可检测的 ----------
 *
 * trie_collect 是公开的（buf_size 由 caller 给），所以缓冲区不够这件事
 * 依然可能发生。要求：一旦发生，truncated 必须置位——
 * 让"结果不完整"从一个静默状态变成一个可断言的状态。 */
static bool test_collect_truncation_is_reported(void) {
    TrieNode *root = trie_create();
    trie_insert(root, "ab");
    trie_insert(root, "abcdefgh");

    /* buf_size = 4：能拼出 "ab"，拼不出 "abcdefgh" */
    WordList small;
    wordlist_init(&small);
    char buf4[4];
    trie_collect(root, buf4, 0, sizeof buf4, &small);
    CHECK(small.count == 1);
    CHECK(strcmp(small.words[0], "ab") == 0);
    CHECK(small.truncated);              /* 关键：丢了东西，必须说出来 */
    wordlist_free(&small);

    /* buf_size 足够：一个都不该丢，也不该误报截断 */
    WordList full;
    wordlist_init(&full);
    char buf64[64];
    trie_collect(root, buf64, 0, sizeof buf64, &full);
    CHECK(full.count == 2);
    CHECK(!full.truncated);              /* 不能误报 */
    wordlist_free(&full);

    /* 边界：buf_size 刚好装得下最长的词 "abcdefgh"(8) + '\0' = 9 */
    WordList exact;
    wordlist_init(&exact);
    char buf9[9];
    trie_collect(root, buf9, 0, sizeof buf9, &exact);
    CHECK(exact.count == 2);
    CHECK(!exact.truncated);
    wordlist_free(&exact);

    /* 差一个字节就该报截断 */
    WordList off_by_one;
    wordlist_init(&off_by_one);
    char buf8[8];
    trie_collect(root, buf8, 0, sizeof buf8, &off_by_one);
    CHECK(off_by_one.count == 1);
    CHECK(off_by_one.truncated);
    wordlist_free(&off_by_one);

    trie_free(root);
    return true;
}

int main(void) {
    printf("========== Trie 测试套件 ==========\n");

    RUN_TEST(test_empty_trie);
    RUN_TEST(test_single_word);
    RUN_TEST(test_duplicate_insert);
    RUN_TEST(test_shared_prefix_nodes);
    RUN_TEST(test_delete_nonexistent);
    RUN_TEST(test_delete_leaf_frees_nodes);
    RUN_TEST(test_delete_shared_prefix_word);
    RUN_TEST(test_delete_all_words_empties_trie);
    RUN_TEST(test_starts_with_variants);
    RUN_TEST(test_autocomplete_matches_expected_set);
    RUN_TEST(test_invalid_inputs_rejected);
    RUN_TEST(test_large_random_insert_delete);
    RUN_TEST(test_free_null_and_empty_safe);
    RUN_TEST(test_word_length_boundary);
    RUN_TEST(test_collect_truncation_is_reported);

    printf("\n========== 汇总 ==========\n");
    printf("通过: %d, 失败: %d, 总计: %d\n", g_pass, g_fail, g_pass + g_fail);

    return g_fail == 0 ? 0 : 1;
}
