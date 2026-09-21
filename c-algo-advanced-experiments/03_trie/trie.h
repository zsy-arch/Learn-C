/* trie.h —— 前缀树核心实现，demo.c 和 tests.c 共用 */
#ifndef TRIE_H
#define TRIE_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALPHABET_SIZE 26

/* 单词长度上限（不含结尾 '\0'）。
 *
 * 这个常量存在的原因是一个真实的缺陷：自动补全用一个固定大小的栈缓冲区
 * 拼接单词，而插入路径原来对长度**毫无限制**。于是一个 256 字符的单词
 * 可以成功插入、可以被 trie_search 查到，却永远不会出现在
 * trie_autocomplete 的结果里——实测 insert=1、search=1、autocomplete=0。
 *
 * 与其让两条路径各有各的隐含上限，不如把它提成一个具名常量，
 * 在**入口**（is_valid_word）就拦住，让"树里的每个单词都能被枚举出来"
 * 成为一条真正成立的不变量。缓冲区大小也由它推导，两者不可能再对不上。 */
#define TRIE_MAX_WORD_LEN 255

typedef struct TrieNode {
    struct TrieNode *children[ALPHABET_SIZE];
    bool is_word; /* 这个节点是否是某个单词的结尾 */
} TrieNode;

static inline TrieNode *trie_node_create(void) {
    TrieNode *node = calloc(1, sizeof *node); /* calloc: children 全部置 NULL, is_word 置 false */
    if (node == NULL) {
        fprintf(stderr, "trie_node_create: out of memory\n");
        exit(1);
    }
    return node;
}

/* 创建空 Trie：一个不代表任何字符的根节点 */
static inline TrieNode *trie_create(void) {
    return trie_node_create();
}

static inline int char_to_index(char c) {
    return c - 'a';
}

static inline bool is_valid_word(const char *word) {
    if (word == NULL || word[0] == '\0') {
        return false;
    }
    size_t len = 0;
    for (const char *p = word; *p != '\0'; p++) {
        if (*p < 'a' || *p > 'z') {
            return false;
        }
        if (++len > TRIE_MAX_WORD_LEN) {
            return false; /* 超长单词在入口就拒绝，见 TRIE_MAX_WORD_LEN 的说明 */
        }
    }
    return true;
}

/* 插入单词。空指针/空字符串/含非小写字母字符都会被拒绝并返回 false。 */
static inline bool trie_insert(TrieNode *root, const char *word) {
    if (root == NULL || !is_valid_word(word)) {
        return false;
    }
    TrieNode *cur = root;
    for (const char *p = word; *p != '\0'; p++) {
        int idx = char_to_index(*p);
        if (cur->children[idx] == NULL) {
            cur->children[idx] = trie_node_create();
        }
        cur = cur->children[idx];
    }
    cur->is_word = true;
    return true;
}

/* 找到 word 对应路径的末尾节点；不存在则返回 NULL。
 * 内部工具函数，search / starts_with / delete 都基于它。 */
static inline TrieNode *trie_find_node(TrieNode *root, const char *word) {
    if (root == NULL || word == NULL) {
        return NULL;
    }
    TrieNode *cur = root;
    for (const char *p = word; *p != '\0'; p++) {
        if (*p < 'a' || *p > 'z') {
            return NULL;
        }
        int idx = char_to_index(*p);
        if (cur->children[idx] == NULL) {
            return NULL;
        }
        cur = cur->children[idx];
    }
    return cur;
}

static inline bool trie_search(TrieNode *root, const char *word) {
    TrieNode *node = trie_find_node(root, word);
    return node != NULL && node->is_word;
}

static inline bool trie_starts_with(TrieNode *root, const char *prefix) {
    if (root == NULL) {
        return false;
    }
    if (prefix == NULL || prefix[0] == '\0') {
        return true; /* 空前缀匹配一切非空 Trie */
    }
    return trie_find_node(root, prefix) != NULL;
}

static inline bool trie_node_is_empty(const TrieNode *node) {
    if (node->is_word) {
        return false;
    }
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (node->children[i] != NULL) {
            return false;
        }
    }
    return true;
}

/* 递归删除的核心：depth 表示 word[depth] 是当前要走的那一步。
 * 返回值表示"当前这个 node 在处理完之后是否变成了空节点，
 * 空到可以被父节点 free 掉"。
 *
 * 这是本章的重点：不能在找到单词结尾后就往回把整条路径 free 掉，
 * 因为路径上的某个节点可能同时是另一个单词的一部分（前缀共享）。
 * 只有"沿途每一层都确认这一层除了刚才这条路径外什么都没有"，
 * 才能把这一层也交给父节点释放。 */
static inline bool trie_delete_helper(TrieNode *node, const char *word, size_t depth) {
    if (node == NULL) {
        return false; /* 单词本来就不存在，什么都不做 */
    }

    if (word[depth] == '\0') {
        if (!node->is_word) {
            return false; /* 这是某个更长单词的前缀节点，不是完整单词，删除不存在的单词 */
        }
        node->is_word = false; /* 只清标记，不动 children —— 可能还有别的单词经过这里 */
        return trie_node_is_empty(node);
    }

    int idx = char_to_index(word[depth]);
    TrieNode *child = node->children[idx];
    if (trie_delete_helper(child, word, depth + 1)) {
        free(child);
        node->children[idx] = NULL;
    }

    /* 递归返回后，如果自己既不是某单词的结尾也没有任何孩子，
     * 说明自己也是"这次删除唯一的意义"，可以继续往上交给父节点释放 */
    return trie_node_is_empty(node);
}

static inline void trie_delete(TrieNode *root, const char *word) {
    if (root == NULL || !is_valid_word(word)) {
        return;
    }
    /* 根节点本身永远不释放（它不代表任何字符），所以对根调用时忽略返回值 */
    trie_delete_helper(root, word, 0);
}

static inline void trie_free(TrieNode *node) {
    if (node == NULL) {
        return;
    }
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        trie_free(node->children[i]);
    }
    free(node);
}

/* ---- 自动补全：收集 Trie 中所有以 prefix 开头的单词 ---- */

typedef struct WordList {
    char **words;
    size_t count;
    size_t capacity;
    /* 结果是否不完整。
     *
     * 只看 count 是分不清"确实没有更多匹配"和"有匹配但被缓冲区截断了"的——
     * 两种情况都返回一个偏小的 count，caller 拿到的是一个看起来完全正常的列表。
     * 有了这个标志，静默丢数据就变成了可检测的状态。
     *
     * 正常路径下（单词长度由 is_valid_word 兜住）它恒为 false；
     * 只有 trie_collect 被直接调用且 buf_size 小于实际单词长度时才会置位。 */
    bool truncated;
} WordList;

static inline void wordlist_init(WordList *list) {
    list->words = NULL;
    list->count = 0;
    list->capacity = 0;
    list->truncated = false;
}

static inline void wordlist_push(WordList *list, const char *buf, size_t len) {
    if (list->count == list->capacity) {
        size_t new_cap = list->capacity == 0 ? 8 : list->capacity * 2;
        char **grown = realloc(list->words, new_cap * sizeof *grown);
        if (grown == NULL) {
            fprintf(stderr, "wordlist_push: out of memory\n");
            exit(1);
        }
        list->words = grown;
        list->capacity = new_cap;
    }
    char *copy = malloc(len + 1);
    if (copy == NULL) {
        fprintf(stderr, "wordlist_push: out of memory\n");
        exit(1);
    }
    memcpy(copy, buf, len);
    copy[len] = '\0';
    list->words[list->count++] = copy;
}

static inline void wordlist_free(WordList *list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->words[i]);
    }
    free(list->words);
    list->words = NULL;
    list->count = 0;
    list->capacity = 0;
    list->truncated = false;
}

/* DFS 收集：buf 是当前拼出来的字符串前缀，depth 是它的长度 */
static inline void trie_collect(TrieNode *node, char *buf, size_t depth, size_t buf_size, WordList *out) {
    if (node == NULL) {
        return;
    }
    if (node->is_word) {
        wordlist_push(out, buf, depth);
    }
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (node->children[i] == NULL) {
            continue;
        }
        if (depth + 1 < buf_size) {
            buf[depth] = (char)('a' + i);
            trie_collect(node->children[i], buf, depth + 1, buf_size, out);
        } else {
            /* 缓冲区放不下了。原来这里是 if 条件的一部分，条件不成立就
             * 直接跳过——结果是子树里的单词被**无声无息**地丢掉，
             * 返回的 WordList 和"本来就没有更多单词"长得一模一样。
             * 现在至少把这个事实记录下来。 */
            out->truncated = true;
        }
    }
}

/* 给定前缀，返回所有以它开头的单词（自动补全）。找不到前缀返回空列表。
 *
 * 缓冲区由 TRIE_MAX_WORD_LEN 推导，而插入路径又被同一个常量兜住，
 * 所以"树里存在但枚举不出来"的情况不可能再发生。 */
static inline WordList trie_autocomplete(TrieNode *root, const char *prefix) {
    WordList result;
    wordlist_init(&result);
    if (root == NULL) {
        return result;
    }

    char buf[TRIE_MAX_WORD_LEN + 1];
    size_t prefix_len = (prefix == NULL) ? 0 : strlen(prefix);
    if (prefix_len >= sizeof buf) {
        /* 前缀本身就超过了合法单词长度，不可能有任何单词以它开头。
         * 这是"确实没有匹配"，不是截断，所以 truncated 保持 false。 */
        return result;
    }

    TrieNode *start = (prefix_len == 0) ? root : trie_find_node(root, prefix);
    if (start == NULL) {
        return result;
    }

    if (prefix_len > 0) {
        memcpy(buf, prefix, prefix_len);
    }
    trie_collect(start, buf, prefix_len, sizeof buf, &result);
    return result;
}

/* ---- 统计工具：数一数 Trie 里到底有多少个节点，方便验证共享效果 ---- */
static inline size_t trie_count_nodes(const TrieNode *node) {
    if (node == NULL) {
        return 0;
    }
    size_t total = 1;
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        total += trie_count_nodes(node->children[i]);
    }
    return total;
}

#endif /* TRIE_H */
