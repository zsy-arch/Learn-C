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
    printf("       t   r  [is_word=true, 对应 \"car\"]\n");
    printf("       |       \\\n");
    printf("       |        +---d  [is_word=true, 对应 \"card\"]\n");
    printf("       |        |\n");
    printf("       |        +---e  [is_word=true, 对应 \"care\"]\n");
    printf("       |\n");
    printf("  [is_word=true, 对应 \"cat\"，t 是叶子，没有孩子]\n");
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
 *
 * 它要演示的错误是**逻辑**错误：把被其他单词共享的前缀节点也删掉，
 * 导致其他单词从 Trie 里"消失"。
 *
 * 但要说清楚它造成的实际损害到底是什么，因为很容易说错：
 *   - 没有 use-after-free，也没有 double free：循环里先把父节点的
 *     children[idx] 置 NULL 再 free 父节点自己，而 free 的顺序是从深到浅，
 *     每次写的都是一个还没被 free 的节点。
 *   - 但**有内存泄漏**：被 free 的节点的子树（本例中 "card" 的那个 d 节点）
 *     在父节点消失后就再也没人指向它了。实测 `leaks --atExit -- ./demo`
 *     报 2 leaks / 448 bytes（d 节点 + 下面故意不释放的 root）。
 *     泄漏也是内存安全问题的一种，所以"这不是内存安全 bug"的说法是不对的。
 *
 * 另外下面有两处和"教学要点"无关的缺陷，已经修掉了——留着它们只会让读者
 * 在抄这段代码时踩到与本节主题无关的坑：
 *   1. path/idx 原来是固定的 64 项，而 is_valid_word 允许 255 字符的单词，
 *      对一个长度超过 64 的已存在单词调用它就是栈上越界写。实测（199 字符的
 *      单词）UBSan 报 "index 64 out of bounds for type 'TrieNode *[64]'"，
 *      ASan 报 stack-buffer-overflow / WRITE of size 8。
 *   2. 原来不校验字符，char_to_index('C') == 'C' - 'a' == -30，
 *      于是 cur->children[-30] 直接读到结构体外面去；实测 UBSan 报
 *      "index -30 out of bounds"，ASan 紧接着报 BUS。
 *      正式的 trie_delete 是走 is_valid_word 的，这里也补上，保持一致。 */
static void trie_delete_BROKEN(TrieNode *root, const char *word) {
    if (root == NULL || !is_valid_word(word)) {
        return; /* 和正式 trie_delete 相同的入口校验，见上面第 2 点 */
    }

    /* 容量由 TRIE_MAX_WORD_LEN 推导，和插入路径的上限绑在一起，
     * 不再是一个和实际上限对不上的魔法数字 64。见上面第 1 点。 */
    TrieNode *path[TRIE_MAX_WORD_LEN];
    int idx[TRIE_MAX_WORD_LEN];
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

    /* BUG：不检查任何共享情况，从最深处开始无条件往上 free 整条路径。
     * 注意这一行 free(cur) 就已经泄漏了：cur 可能还有孩子（"card" 的 d 节点
     * 就挂在 cur 下面），free 掉 cur 之后那棵子树就再也无法到达了。 */
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
    print_search(root, "card"); /* 期望 true，但错误实现让它变成了 false */
    printf("  -> \"card\" 本该完好无损，但因为 c/a/r 节点被无条件 free，\n");
    printf("     root 到这条路径的指针也被一路清成了 NULL，\n");
    printf("     结果是 \"card\" 也从 Trie 里\"消失\"了 —— 这就是没做\n");
    printf("     \"这一层是否还被其他单词占用\"检查的后果。\n");
    printf("  节点总数 = %zu（只剩 root），而 \"card\" 的 d 节点既没被 free、\n",
           trie_count_nodes(root));
    printf("     也再没有任何指针指向它 —— 它泄漏了。\n");
    printf("  注意 root 本身并没有变成\"损坏的树\"：它的 26 个孩子全是 NULL，\n");
    printf("     是一棵合法的空 Trie，继续调用任何 API 都是安全的（已用\n");
    printf("     ASan/UBSan 验证）。这个错误实现丢掉的是数据，不是内存安全。\n");

    /* 这里仍然调用 trie_free(root)：
     *
     * 早先这里故意不调用，理由写的是"root 已被破坏成结构不一致的树，
     * 继续操作是未定义行为"。这个理由是错的——实测（探针 + ASan/UBSan）
     * trie_delete_BROKEN 返回后 root->children 全部是 NULL，是一棵完全合法的
     * 空 Trie；free 它既不会 double free 也不会 use-after-free。不调用反而
     * 让 root 自己也跟着泄漏，把"1 个泄漏节点"变成了"2 个"。
     *
     * 真正泄漏掉、且**无法**再补救的只有 d 节点：它已经没有任何指针指向了，
     * 这正是这个错误实现的代价之一，也是本节要展示的东西。 */
    trie_free(root);
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
