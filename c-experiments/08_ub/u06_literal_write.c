/* u06_literal_write.c —— UB #6：修改字符串字面量 */
#include <stdio.h>

int main(int argc, char **argv)
{
    const char *mode = (argc > 1) ? argv[1] : "safe";
    printf("模式: %s  (可选: safe | crash)\n", mode);

    /* 正确：数组是字面量内容的一份可写拷贝 */
    char writable[] = "hello";
    writable[0] = 'H';
    printf("char writable[] = \"hello\"; writable[0]='H' -> \"%s\"  OK\n", writable);
    printf("  writable 在栈上: %p\n", (void *)writable);

    /* 危险：这个指针指向只读段 */
    char *literal = (char *)"hello";     /* 需要显式强转才能去掉 const */
    printf("char *literal = \"hello\";  literal 在只读段: %p\n", (void *)literal);

    if (mode[0] == 'c') {
        puts("即将执行 literal[0] = 'H'  —— undefined behavior");
        fflush(stdout);
        literal[0] = 'H';                /* UB：多数平台会 SIGBUS/SIGSEGV */
        puts("居然没崩？那说明这个平台把字面量放在可写页 —— 依然是 UB");
    }

    puts("\n要点:");
    puts("  1) 字符串字面量的类型是 char[N]，但它是「不可修改的」");
    puts("  2) 所以永远写 const char *s = \"...\";，让编译器帮你拦住");
    puts("  3) 需要可写内容就用 char buf[] = \"...\"; 或 strdup/malloc");
    puts("  4) 不要为了消除警告而强转掉 const —— 那是在关掉安全带");
    return 0;
}
