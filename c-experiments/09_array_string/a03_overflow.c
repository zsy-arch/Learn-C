/* a03_overflow.c —— 反例：栈上缓冲区溢出。用 ASan 运行会被精确抓住。 */
#include <stdio.h>
#include <string.h>

struct Frame {
    char buf[8];
    int  canary;      /* 放在 buf 后面，用来观察溢出会踩到谁 */
};

static void unsafe_copy(const char *src)
{
    struct Frame f;
    f.canary = 0x11223344;
    strcpy(f.buf, src);                /* 没有任何长度检查 */
    printf("  buf=\"%s\"  canary=0x%08X %s\n",
           f.buf, (unsigned)f.canary,
           (f.canary == 0x11223344) ? "(完好)" : "(被踩坏了！)");
}

static void safe_copy(const char *src)
{
    struct Frame f;
    f.canary = 0x11223344;
    int need = snprintf(f.buf, sizeof f.buf, "%s", src);
    printf("  buf=\"%s\"  canary=0x%08X %s  %s\n",
           f.buf, (unsigned)f.canary,
           (f.canary == 0x11223344) ? "(完好)" : "(被踩坏了！)",
           (need >= 0 && (size_t)need >= sizeof f.buf) ? "[发生截断]" : "");
}

int main(int argc, char **argv)
{
    const char *mode = (argc > 1) ? argv[1] : "safe";
    const char *src  = (argc > 2) ? argv[2] : "0123456789ABCDEF";

    printf("模式: %s   源串: \"%s\" (%zu 字符)  目标 buf 只有 8 字节\n",
           mode, src, strlen(src));

    if (mode[0] == 'u') {
        puts("== 不安全版本 strcpy ==");
        unsafe_copy(src);
        puts("  即使看起来「没崩」，相邻内存也已经被破坏了");
    } else {
        puts("== 安全版本 snprintf ==");
        safe_copy(src);
    }
    return 0;
}
