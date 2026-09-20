/* a02_strings.c —— C 字符串：'\0' 结尾的字符数组 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void show(const char *tag, const char *s, size_t bufsize)
{
    printf("  %-16s \"%s\"  strlen=%zu  buf=%zu\n", tag, s, strlen(s), bufsize);
}

int main(void)
{
    puts("== 1. C 字符串 = 以 '\\0' 结尾的 char 数组 ==");
    char s[] = "abc";
    printf("  char s[] = \"abc\";  sizeof=%zu  strlen=%zu\n", sizeof s, strlen(s));
    printf("  字节: ");
    for (size_t i = 0; i < sizeof s; i++) {
        printf("'%c'(0x%02X) ", s[i] ? s[i] : '.', (unsigned char)s[i]);
    }
    puts("\n  最后那个 0x00 就是终止符，strlen 不算它，sizeof 算");

    puts("\n== 2. 没有 '\\0' 就不是字符串 ==");
    char notstr[3] = {'a', 'b', 'c'};     /* 刚好装满，没有空间放 '\0' */
    printf("  char notstr[3] = {'a','b','c'};  对它调用 strlen 会一直往后读 —— UB\n");
    printf("  想安全地打印，用 %%.*s 限定长度: \"%.*s\"\n", 3, notstr);

    puts("\n== 3. strcpy 是缓冲区溢出的头号来源 ==");
    char small[8];
    const char *src_ok  = "1234567";      /* 7 + 1 = 8，刚好 */
    printf("  char small[8];  strcpy(small, \"%s\") 需要 %zu 字节 —— 刚好装下\n",
           src_ok, strlen(src_ok) + 1);
    strcpy(small, src_ok);
    show("strcpy 之后", small, sizeof small);
    printf("  如果源串是 8 个字符，就会写到 small[8] —— 越界（见 a03_overflow.c）\n");

    puts("\n== 4. snprintf 是最推荐的安全写法 ==");
    char dst[8];
    int need = snprintf(dst, sizeof dst, "%s", "0123456789");
    printf("  snprintf(dst, %zu, \"%%s\", \"0123456789\")\n", sizeof dst);
    printf("    返回值 = %d  <-- 这是「本来需要的长度」，不是实际写入的长度\n", need);
    show("dst", dst, sizeof dst);
    printf("    截断判断: if (need < 0 || (size_t)need >= sizeof dst) -> %s\n",
           (need >= 0 && (size_t)need >= sizeof dst) ? "发生了截断" : "完整");
    printf("    snprintf 永远保证以 '\\0' 结尾（只要 size > 0）\n");

    puts("\n== 5. strncpy 不是安全版 strcpy！==");
    char t1[8];
    strncpy(t1, "0123456789", sizeof t1);
    printf("  strncpy(t1, \"0123456789\", 8) 之后 t1 没有 '\\0' 结尾！\n");
    printf("  前 8 字节: ");
    for (size_t i = 0; i < sizeof t1; i++) { printf("%c", t1[i]); }
    printf("\n  必须自己补: t1[sizeof t1 - 1] = '\\0';\n");
    t1[sizeof t1 - 1] = '\0';
    show("补 \\0 之后", t1, sizeof t1);

    char t2[8];
    strncpy(t2, "ab", sizeof t2);
    printf("  另一个坑：源串短时 strncpy 会把剩余空间全填 '\\0'（性能浪费）\n");
    printf("  t2 的 8 个字节: ");
    for (size_t i = 0; i < sizeof t2; i++) { printf("%02X ", (unsigned char)t2[i]); }
    putchar('\n');

    puts("\n== 6. 常用函数速查 ==");
    const char *a = "hello world";
    printf("  strlen(\"%s\")        = %zu\n", a, strlen(a));
    printf("  strchr(a, 'o') - a     = %td  (第一个 'o' 的下标)\n", strchr(a, 'o') - a);
    printf("  strrchr(a, 'o') - a    = %td  (最后一个 'o')\n", strrchr(a, 'o') - a);
    printf("  strstr(a, \"wor\") - a   = %td\n", strstr(a, "wor") - a);
    printf("  strcmp(\"abc\",\"abd\")    = %d  (<0 表示前者小)\n", strcmp("abc", "abd"));
    printf("  strncmp(\"abc\",\"abd\",2) = %d  (只比前 2 个字符)\n", strncmp("abc", "abd", 2));

    puts("\n== 7. memcpy vs memmove ==");
    char buf[16] = "0123456789";
    printf("  原始:                %s\n", buf);
    memmove(buf + 2, buf, 5);          /* 区间重叠，必须用 memmove */
    printf("  memmove(buf+2,buf,5): %s   <-- 重叠时安全\n", buf);
    puts("  memcpy 的前提是「源和目的不重叠」，重叠就是 UB");
    puts("  拿不准就用 memmove，代价很小");

    puts("\n== 8. 动态字符串 ==");
    const char *msg = "dynamic string";
    size_t n = strlen(msg);
    char *heap = malloc(n + 1);        /* 别忘了 +1 给 '\0' */
    if (heap != NULL) {
        memcpy(heap, msg, n + 1);
        show("malloc 版本", heap, n + 1);
        free(heap);
    }
    printf("  strdup 也可以，但它是 POSIX，不在 C17 标准里（C23 才加入）\n");

    puts("\n== 最佳实践 ==");
    puts("  1) 首选 snprintf，永远检查返回值判断是否截断");
    puts("  2) 不要用 strcpy/strcat/sprintf/gets");
    puts("  3) strncpy 不保证 '\\0' 结尾，用它之后一定手动补");
    puts("  4) 分配字符串缓冲区时永远记得 +1");
    puts("  5) 需要二进制安全就自己带长度，别依赖 '\\0'");

    return 0;
}
