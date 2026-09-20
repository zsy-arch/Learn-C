/* p03_strings.c —— char *s = "abc"  vs  char s[] = "abc" */
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char *lit = "abc";     /* 指针，指向只读的字符串字面量 */
    char        buf[] = "abc";   /* 数组，把字面量内容「拷贝」到栈上 */

    puts("== 1. sizeof 完全不同 ==");
    printf("  const char *lit = \"abc\";  sizeof(lit) = %zu  <-- 指针大小\n", sizeof(lit));
    printf("  char        buf[] = \"abc\"; sizeof(buf) = %zu  <-- 'a','b','c','\\0'\n", sizeof(buf));
    printf("  strlen(lit) = %zu   strlen(buf) = %zu  <-- strlen 数到 '\\0' 为止\n",
           strlen(lit), strlen(buf));

    puts("\n== 2. 它们在内存的哪个区 ==");
    int stack_var = 0;
    printf("  lit 指向的地址 = %p   (只读数据段 __TEXT/__cstring)\n", (const void *)lit);
    printf("  buf 的地址     = %p   (栈)\n", (void *)buf);
    printf("  栈上变量地址   = %p\n", (void *)&stack_var);
    printf("  两者地址相差很远，说明在不同的段\n");

    puts("\n== 3. buf 可以改，lit 指向的内容不能改 ==");
    buf[0] = 'A';
    printf("  buf[0]='A' 之后 buf = \"%s\"\n", buf);
    printf("  lit[0]='A' 会怎样？-> 见 literal_write.c，那是 undefined behavior\n");

    puts("\n== 4. 相同的字面量可能被合并成同一份 ==");
    const char *a = "hello";
    const char *b = "hello";
    printf("  a = %p\n", (const void *)a);
    printf("  b = %p\n", (const void *)b);
    printf("  a == b ? %s  <-- 编译器做了字符串池化（不是标准保证的）\n",
           (a == b) ? "true" : "false");

    char c[] = "hello";
    char d[] = "hello";
    printf("  数组版本 c = %p, d = %p, c==d ? %s  <-- 各自独立的拷贝\n",
           (void *)c, (void *)d, ((void *)c == (void *)d) ? "true" : "false");

    puts("\n== 5. 比较字符串要用 strcmp，不能用 == ==");
    printf("  strcmp(c, d) = %d  (0 表示内容相同)\n", strcmp(c, d));
    printf("  c == d 比较的是「地址」，几乎永远是 false\n");

    puts("\n== 6. 最佳实践 ==");
    puts("  - 指向字面量时一律写 const char *，让编译器帮你拦住写操作");
    puts("  - 需要修改内容时用 char buf[] 或 malloc 出来的缓冲区");
    puts("  - 需要长度时优先 sizeof(数组)-1，其次 strlen");

    return 0;
}
