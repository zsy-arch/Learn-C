/* s03_deepcopy.c —— 浅拷贝 vs 深拷贝：带指针成员的结构体
 * 用 ASan 编译可以看到浅拷贝导致的 double-free / use-after-free。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char  *name;      /* 指针成员：结构体只存地址，不存内容 */
    size_t len;
    int    id;
} Person;

/* 构造：分配并拥有 name 的所有权 */
static int person_init(Person *p, const char *name, int id)
{
    if (p == NULL || name == NULL) { return -1; }
    size_t n = strlen(name);
    char  *buf = malloc(n + 1);
    if (buf == NULL) { return -1; }
    memcpy(buf, name, n + 1);
    p->name = buf;
    p->len  = n;
    p->id   = id;
    return 0;
}

/* 深拷贝：给新对象分配一份自己的 name */
static int person_copy(Person *dst, const Person *src)
{
    if (dst == NULL || src == NULL) { return -1; }
    char *buf = malloc(src->len + 1);
    if (buf == NULL) { return -1; }
    memcpy(buf, src->name, src->len + 1);
    dst->name = buf;
    dst->len  = src->len;
    dst->id   = src->id;
    return 0;
}

static void person_free(Person *p)
{
    if (p == NULL) { return; }
    free(p->name);
    p->name = NULL;        /* 防止悬垂 + 二次 free */
    p->len  = 0;
}

static void person_print(const char *tag, const Person *p)
{
    printf("  %-8s id=%d name=\"%s\"  name 指针=%p\n",
           tag, p->id, p->name ? p->name : "(null)", (void *)p->name);
}

int main(void)
{
    puts("== 1. 浅拷贝：两个结构体共享同一块堆内存 ==");
    Person a;
    if (person_init(&a, "Alice", 1) != 0) { return 1; }
    Person shallow = a;                 /* 结构体赋值 = 逐字节复制 = 指针被复制 */
    person_print("a", &a);
    person_print("shallow", &shallow);
    printf("  两者 name 指针相同? %s  <-- 这就是浅拷贝\n",
           (a.name == shallow.name) ? "是" : "否");

    printf("  通过 shallow 改名字...\n");
    shallow.name[0] = 'X';
    person_print("a", &a);
    printf("  a 的名字也被改了！这通常不是你想要的\n");

    puts("\n== 2. 浅拷贝的致命后果：double free ==");
    puts("  free(a.name); free(shallow.name);  // 同一块内存被释放两次 -> UB");
    puts("  本程序不会真的这么做，请看 s03_doublefree.c + ASan");

    puts("\n== 3. 深拷贝：各自持有独立的堆内存 ==");
    Person deep;
    if (person_copy(&deep, &a) != 0) { person_free(&a); return 1; }
    person_print("a", &a);
    person_print("deep", &deep);
    printf("  两者 name 指针相同? %s  <-- 独立副本\n",
           (a.name == deep.name) ? "是" : "否");
    deep.name[0] = 'Z';
    person_print("a", &a);
    person_print("deep", &deep);
    printf("  改 deep 不影响 a\n");

    puts("\n== 4. 正确释放 ==");
    person_free(&deep);
    person_free(&a);
    shallow.name = NULL;        /* shallow 从来不拥有这块内存，直接放弃指针 */
    puts("  每块 malloc 恰好 free 一次");

    puts("\n== 最佳实践 ==");
    puts("  1) 结构体里一旦出现指针成员，就必须明确「谁拥有这块内存」");
    puts("  2) 为这类结构体配套写 init / copy / free 三件套");
    puts("  3) 不要用 = 复制带所有权的结构体，除非你明确要转移所有权");
    puts("  4) free 之后把指针成员置 NULL");

    return 0;
}
