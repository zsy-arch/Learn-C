/* main.c —— 综合练习的测试程序（一个极简的手写测试框架） */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vec.h"
#include "slist.h"
#include "sstr.h"

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (cond) {                                                     \
            g_pass++;                                                   \
        } else {                                                        \
            g_fail++;                                                   \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
        }                                                               \
    } while (0)

/* ------------------------- Vec ------------------------- */
static void test_vec(void)
{
    puts("== Vec 动态数组 ==");
    Vec v;
    vec_init(&v);
    CHECK(vec_len(&v) == 0);
    CHECK(vec_cap(&v) == 0);

    for (int i = 0; i < 10; i++) {
        CHECK(vec_push(&v, i * i) == VEC_OK);
    }
    printf("  push 10 个之后: len=%zu cap=%zu（容量按翻倍增长）\n",
           vec_len(&v), vec_cap(&v));
    CHECK(vec_len(&v) == 10);
    CHECK(vec_cap(&v) >= 10);

    int out = -1;
    CHECK(vec_get(&v, 3, &out) == VEC_OK && out == 9);
    CHECK(vec_get(&v, 999, &out) == VEC_ERANGE);
    printf("  vec_get(&v, 999, &out) -> %s（越界被拦住）\n",
           vec_strerror(vec_get(&v, 999, &out)));

    CHECK(vec_set(&v, 0, 100) == VEC_OK);
    CHECK(vec_get(&v, 0, &out) == VEC_OK && out == 100);

    CHECK(vec_insert(&v, 0, -1) == VEC_OK);
    CHECK(vec_get(&v, 0, &out) == VEC_OK && out == -1);
    CHECK(vec_len(&v) == 11);

    CHECK(vec_remove(&v, 0, &out) == VEC_OK && out == -1);
    CHECK(vec_len(&v) == 10);

    CHECK(vec_pop(&v, &out) == VEC_OK && out == 81);
    printf("  pop 出来的是 %d（最后一个 9*9）\n", out);

    printf("  当前内容: ");
    for (size_t i = 0; i < vec_len(&v); i++) {
        int x = 0;
        vec_get(&v, i, &x);
        printf("%d ", x);
    }
    putchar('\n');

    size_t before = vec_cap(&v);
    CHECK(vec_shrink_to_fit(&v) == VEC_OK);
    printf("  shrink_to_fit: cap %zu -> %zu\n", before, vec_cap(&v));
    CHECK(vec_cap(&v) == vec_len(&v));

    vec_free(&v);
    CHECK(vec_len(&v) == 0);
    vec_free(&v);            /* 二次 free 必须安全 */
    puts("  vec_free 调用两次也安全（内部已置 NULL）");
}

/* ------------------------ SList ------------------------ */
static bool collect(int value, void *ctx)
{
    Vec *v = (Vec *)ctx;
    return vec_push(v, value) == VEC_OK;
}

static void test_slist(void)
{
    puts("\n== SList 单链表 ==");
    SList l;
    slist_init(&l);
    CHECK(slist_size(&l) == 0);

    CHECK(slist_push_back(&l, 1));
    CHECK(slist_push_back(&l, 2));
    CHECK(slist_push_back(&l, 3));
    CHECK(slist_push_front(&l, 0));
    CHECK(slist_size(&l) == 4);

    Vec seen;
    vec_init(&seen);
    slist_foreach(&l, collect, &seen);
    printf("  遍历结果: ");
    for (size_t i = 0; i < vec_len(&seen); i++) {
        int x = 0; vec_get(&seen, i, &x); printf("%d ", x);
    }
    putchar('\n');
    CHECK(vec_len(&seen) == 4);
    vec_free(&seen);

    CHECK(slist_contains(&l, 2));
    CHECK(!slist_contains(&l, 99));

    CHECK(slist_push_back(&l, 2));
    CHECK(slist_remove_all(&l, 2) == 2);
    CHECK(slist_size(&l) == 3);
    printf("  删掉所有 2 之后 size=%zu\n", slist_size(&l));

    slist_reverse(&l);
    int head = -1;
    CHECK(slist_pop_front(&l, &head) && head == 3);
    printf("  反转后第一个元素是 %d\n", head);

    slist_free(&l);
    CHECK(slist_size(&l) == 0);
    slist_free(&l);          /* 二次 free 安全 */
    puts("  slist_free 调用两次也安全");
}

/* ------------------------- sstr ------------------------ */
static void test_sstr(void)
{
    puts("\n== sstr 字符串工具 ==");
    char buf[8];

    size_t need = sstr_copy(buf, sizeof buf, "hello");
    printf("  sstr_copy(buf[8], \"hello\") -> \"%s\" 返回 %zu\n", buf, need);
    CHECK(strcmp(buf, "hello") == 0);
    CHECK(need == 5);

    need = sstr_copy(buf, sizeof buf, "0123456789");
    printf("  sstr_copy(buf[8], \"0123456789\") -> \"%s\" 返回 %zu (>=8 说明截断)\n",
           buf, need);
    CHECK(need == 10);
    CHECK(strlen(buf) == 7);
    CHECK(buf[7] == '\0');

    sstr_copy(buf, sizeof buf, "ab");
    need = sstr_cat(buf, sizeof buf, "cdefghij");
    printf("  sstr_cat -> \"%s\" 返回 %zu\n", buf, need);
    CHECK(strlen(buf) == 7);

    char *d = sstr_dup("duplicate me");
    CHECK(d != NULL && strcmp(d, "duplicate me") == 0);
    printf("  sstr_dup -> \"%s\" @%p\n", d, (void *)d);
    free(d);

    char trimbuf[] = "   \t hello world \n  ";
    char *t = sstr_trim(trimbuf);
    printf("  sstr_trim(\"   \\t hello world \\n  \") -> \"%s\"\n", t);
    CHECK(strcmp(t, "hello world") == 0);

    char casebuf[] = "MiXeD Case 123";
    sstr_upper(casebuf);
    printf("  sstr_upper -> \"%s\"\n", casebuf);
    CHECK(strcmp(casebuf, "MIXED CASE 123") == 0);
    sstr_lower(casebuf);
    printf("  sstr_lower -> \"%s\"\n", casebuf);
    CHECK(strcmp(casebuf, "mixed case 123") == 0);

    CHECK(sstr_starts_with("foobar", "foo"));
    CHECK(!sstr_starts_with("foobar", "bar"));
    CHECK(sstr_ends_with("foobar", "bar"));
    CHECK(!sstr_ends_with("foobar", "foo"));
    puts("  starts_with / ends_with OK");

    char csv[] = "a,bb,ccc,dddd";
    char *parts[8];
    size_t n = sstr_split(csv, ',', parts, 8);
    printf("  sstr_split(\"a,bb,ccc,dddd\", ',') -> %zu 段:", n);
    for (size_t i = 0; i < n; i++) { printf(" [%s]", parts[i]); }
    putchar('\n');
    CHECK(n == 4);
    CHECK(strcmp(parts[0], "a") == 0);
    CHECK(strcmp(parts[3], "dddd") == 0);
}

int main(void)
{
    puts("=========== 综合练习测试 ===========");
    test_vec();
    test_slist();
    test_sstr();

    printf("\n=========== 结果: %d passed, %d failed ===========\n", g_pass, g_fail);
    return (g_fail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
