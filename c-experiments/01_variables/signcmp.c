/* signcmp.c —— 反例：有符号/无符号混合比较。
 * 这个文件故意不加 -Werror 编译，用来「看警告长什么样」。 */
#include <stdio.h>

int main(void)
{
    int      i = -1;
    unsigned u = 1u;

    if (i < u) {                 /* 编译器会给 -Wsign-compare 警告 */
        printf("i < u  成立\n");
    } else {
        printf("i < u  不成立  <-- 反直觉，因为 i 被转成了 %u\n", (unsigned)i);
    }

    /* 另一个经典坑：strlen 返回 size_t（无符号） */
    const char *s = "abc";
    size_t len = 3;
    for (int k = 0; k < (int)len; k++) {     /* 正确：显式转成有符号再比 */
        printf("s[%d]=%c\n", k, s[k]);
    }
    return 0;
}
