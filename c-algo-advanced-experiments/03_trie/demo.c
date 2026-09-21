/* demo.c —— Trie（前缀树）：插入/查找/前缀/删除/自动补全 的完整演示 */
#include "trie.h"
#include <stdio.h>

static void print_search(TrieNode *root, const char *word) {
    char label[40];
    snprintf(label, sizeof label, "search(\"%s\")", word);
    printf("  %-20s = %s\n", label, trie_search(root, word) ? "true" : "false");
}

static void print_prefix(TrieNode *root, const char *prefix) {
    char label[40];
    snprintf(label, sizeof label, "starts_with(\"%s\")", prefix);
    printf("  %-20s = %s\n", label, trie_starts_with(root, prefix) ? "true" : "false");
}

static void print_autocomplete(TrieNode *root, const char *prefix) {
    WordList list = trie_autocomplete(root, prefix);
    printf("  autocomplete(\"%s\") = {", prefix);
    for (size_t i = 0; i < list.count; i++) {
        printf("%s%s", list.words[i], (i + 1 < list.count) ? ", " : "");
    }
    printf("}  (%zu 个)\n", list.count);
    wordlist_free(&list);
}

static void section1_basic(void) {
    printf("========== 1. 基本插入 / 查找 / 前缀查找 ==========\n");
    TrieNode *root = trie_create();

    const char *words[] = {"cat", "car", "card", "care", "dog", "do"};
    size_t n = sizeof words / sizeof words[0];
    for (size_t i = 0; i < n; i++) {
        trie_insert(root, words[i]);
    }
    printf("  插入: cat, car, card, care, dog, do\n");
    printf("  节点总数（含根）= %zu\n", trie_count_nodes(root));

    print_search(root, "cat");
    print_search(root, "ca");   /* 只是前缀，不是完整单词 */
    print_search(root, "card");
    print_search(root, "cards"); /* 不存在的更长单词 */
    print_search(root, "dog");
    print_search(root, "d");

    print_prefix(root, "ca");
    print_prefix(root, "do");
    print_prefix(root, "xyz");
    print_prefix(root, "");    /* 空前缀 -> 匹配一切 */

    trie_free(root);
}

static void section2_shared_nodes(void) {
    printf("\n========== 2. 前缀共享：cat / car / card / care ==========\n");
    printf("  插入前缀相关的四个单词后，Trie 结构大致如下：\n");
    printf("\n");
    printf("        root\n");
    printf("         |\n");
    printf("         c\n");
    printf("         |\n");
    printf("         a\n");
    printf("        / \\\n");
    printf("       t   r [is_word=true, 对应 \"car\"]\n");
    printf("       |   |\n");
    printf("   [cat]   +---d [is_word=true, 对应 \"card\"]\n");
    printf("           |\n");
    printf("           +---e [is_word=true, 对应 \"care\"]\n");
    printf("\n");
    printf("  \"ca\" 这条路径被 4 个单词共享，只占用 2 个节点（c、a），\n");
    printf("  而不是每个单词各自占用一份 —— 这就是 Trie 省空间的地方。\n");
}

static void section3_delete(void) {
    printf("\n========== 3. 删除 \"car\"：card / care / cat 必须不受影响 ==========\n");
    TrieNode *root = trie_create();
    trie_insert(root, "cat");
    trie_insert(root, "car");
    trie_insert(root, "card");
    trie_insert(root, "care");

    printf("  删除前节点总数 = %zu\n", trie_count_nodes(root));
    printf("  删除前: ");
    print_search(root, "car");
    printf("  删除前: ");
    print_search(root, "card");

    trie_delete(root, "car");

    printf("  执行 trie_delete(root, \"car\") ...\n");
    printf("  删除后节点总数 = %zu   (不变：c/a/r 节点仍被 card/care 占用，只是 r 的 is_word 被清掉)\n",
           trie_count_nodes(root));
    print_search(root, "car");   /* 应该 false */
    print_search(root, "card");  /* 应该 true，没受影响 */
    print_search(root, "care");  /* 应该 true，没受影响 */
    print_search(root, "cat");   /* 应该 true，完全无关的单词更不该受影响 */
    print_prefix(root, "car");   /* starts_with 仍然 true，因为 card/care 都以 car 开头 */

    trie_free(root);
}

static void section4_delete_leaf_path(void) {
    printf("\n========== 4. 删除会真正释放节点的情况：独立单词 \"dog\" ==========\n");
    TrieNode *root = trie_create();
    trie_insert(root, "dog");
    trie_insert(root, "cat"); /* 无关的单词，确认删除 dog 不会影响它 */

    printf("  插入 dog, cat 后节点总数 = %zu\n", trie_count_nodes(root));
    trie_delete(root, "dog");
    printf("  删除 dog 后节点总数     = %zu   (d/o/g 三个节点全部被物理释放)\n", trie_count_nodes(root));
    print_search(root, "dog"); /* false */
    print_search(root, "cat"); /* true，无关单词不受影响 */

    trie_free(root);
}

static void section5_delete_nonexistent(void) {
    printf("\n========== 5. 删除不存在的单词：不能崩溃，不能误删 ==========\n");
    TrieNode *root = trie_create();
    trie_insert(root, "cat");

    printf("  Trie 中只有 \"cat\"\n");
    trie_delete(root, "dog");   /* 完全不存在的单词 */
    trie_delete(root, "ca");    /* 存在的前缀，但不是完整单词 */
    trie_delete(root, "cats");  /* 存在单词的更长版本 */
    printf("  删除 \"dog\" / \"ca\" / \"cats\" 之后（这三个都不是 Trie 里的完整单词）：\n");
    print_search(root, "cat");  /* 必须仍然 true */
    printf("  -> 三次无效删除全部安全忽略，\"cat\" 完好无损\n");

    trie_free(root);
}

static void section6_autocomplete(void) {
    printf("\n========== 6. 应用：自动补全 ==========\n");
    TrieNode *root = trie_create();
    const char *words[] = {"cat", "car", "card", "care", "careful", "dog", "do", "door"};
    size_t n = sizeof words / sizeof words[0];
    for (size_t i = 0; i < n; i++) {
        trie_insert(root, words[i]);
    }
    printf("  词库: cat, car, card, care, careful, dog, do, door\n");
    print_autocomplete(root, "ca");
    print_autocomplete(root, "car");
    print_autocomplete(root, "do");
    print_autocomplete(root, "z");   /* 没有任何单词以 z 开头 */
    print_autocomplete(root, "");    /* 空前缀 = 全部单词 */

    trie_free(root);
}

/* ========== 7. 错误示例：一个"看起来对"但会破坏共享前缀的删除实现 ========== *
 *
 * ⚠️ 危险示例，仅用于教学，禁止在正式代码中使用。
 *
 * 很多人第一次写 Trie 删除时会这样想："找到单词结尾，然后把这一路走过的
 * 节点全部 free 掉不就行了？" —— 下面就是这个直觉的代码化。
 * 它不是内存安全问题（不会有 use-after-free，因为它至少没有二次释放同一个
 * 指针），而是逻辑正确性问题：它会把被其他单词共享的前缀节点也删掉，
 * 导致其他单词从 Trie 里"消失"。 */
static void trie_delete_BROKEN(TrieNode *root, const char *word) {
    TrieNode *path[64];
    int idx[64];
    int depth = 0;

    TrieNode *cur = root;
    for (const char *p = word; *p != '\0'; p++) {
        int i = char_to_index(*p);
        if (cur->children[i] == NULL) {
            return; /* 单词不存在 */
        }
        path[depth] = cur;
        idx[depth] = i;
        cur = cur->children[i];
        depth++;
    }
    if (!cur->is_word) {
        return;
    }

    /* BUG：不检查任何共享情况，从最深处开始无条件往上 free 整条路径 */
    free(cur);
    for (int i = depth - 1; i >= 0; i--) {
        path[i]->children[idx[i]] = NULL;
        /* 如果 path[i] 还被别的单词用着（比如它还有其他孩子，或者
         * 它自己就是另一个单词的结尾），这里也不会检查，
         * 直接继续往上 free —— 这就是 bug 所在 */
        if (i > 0) {
            free(path[i]);
        }
    }
}

static void section7_broken_delete(void) {
    printf("\n========== 7. ⚠️ 错误示例：无条件往上 free 整条路径 ==========\n");
    TrieNode *root = trie_create();
    trie_insert(root, "car");
    trie_insert(root, "card"); /* 与 car 共享 c-a-r 路径 */

    printf("  插入 car, card（共享 c-a-r 路径）\n");
    printf("  删除前: ");
    print_search(root, "card");

    printf("  调用 trie_delete_BROKEN(root, \"car\") ...\n");
    trie_delete_BROKEN(root, "car");

    printf("  删除后: ");
    print_search(root, "card"); /* 期望 true，但错误实现会让它变成 false 或更糟 */
    printf("  -> \"card\" 本该完好无损，但因为 c/a/r 节点被无条件 free，\n");
    printf("     现在查找 \"card\" 会从一个已经不存在于树里的路径开始找，\n");
    printf("     结果是 \"card\" 也从 Trie 里\"消失\"了 —— 这就是没做\n");
    printf("     \"这一层是否还被其他单词占用\"检查的后果。\n");
    printf("  本演示到此为止，不再使用 root（它的内部已被破坏，\n");
    printf("     继续操作是未定义行为，这里选择直接放弃它、不调用 trie_free）。\n");

    /* 注意：root 内部结构已经被破坏（部分节点被 free 但父节点的指针
     * 没有被正确清空 / 部分子树被连带误删），这里故意不再调用 trie_free(root)，
     * 避免示范"对一棵已经损坏的树做进一步操作"。这是本实验唯一一处
     * "故意不释放"的内存，且是演示用的错误函数造成的，不代表 trie.h
     * 的正常 API 有泄漏——tests.c 和下面的 leaks 检测都是针对正常 API 的。
     */
}

int main(void) {
    section1_basic();
    section2_shared_nodes();
    section3_delete();
    section4_delete_leaf_path();
    section5_delete_nonexistent();
    section6_autocomplete();
    section7_broken_delete();
    printf("\n全部演示结束。\n");
    return 0;
}
