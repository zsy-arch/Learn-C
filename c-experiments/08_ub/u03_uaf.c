/* u03_uaf.c —— UB #3：释放后使用（use after free）与二次释放 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *mode = (argc > 1) ? argv[1] : "read";
    printf("模式: %s  (可选: read | write | doublefree | safe)\n", mode);

    char *p = malloc(32);
    if (p == NULL) { return 1; }
    strcpy(p, "important data");
    printf("free 前: p=%p 内容=\"%s\"\n", (void *)p, p);

    free(p);
    /* 从这一刻起，p 是悬垂指针。它的「值」还在，但指向的对象已经不存在。 */

    if (strcmp(mode, "read") == 0) {
        printf("free 后读: \"%s\"   <-- use-after-free (读)\n", p);
    } else if (strcmp(mode, "write") == 0) {
        strcpy(p, "overwrite!");     /* use-after-free (写)，可能破坏其他分配 */
        printf("free 后写: \"%s\"\n", p);
    } else if (strcmp(mode, "doublefree") == 0) {
        free(p);                     /* double free */
        puts("double free 完成");
    } else {
        p = NULL;                    /* 正确做法 */
        printf("置 NULL 之后 p=%p，任何误用都会立刻变成可见的空指针错误\n", (void *)p);
    }

    return 0;
}
