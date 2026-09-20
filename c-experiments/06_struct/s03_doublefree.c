/* s03_doublefree.c —— 反例：浅拷贝后两边都 free。用 ASan 运行会立刻报错。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *name;
} Person;

int main(void)
{
    Person a;
    a.name = malloc(8);
    if (a.name == NULL) { return 1; }
    strcpy(a.name, "Alice");

    Person b = a;              /* 浅拷贝：b.name 和 a.name 指向同一块 */

    printf("a.name=%p  b.name=%p\n", (void *)a.name, (void *)b.name);

    free(a.name);              /* 第一次释放，合法 */
    free(b.name);              /* 第二次释放同一块内存 -> UB (double free) */

    puts("如果你看到这行，说明 double free 没有被立刻抓住 —— 更可怕");
    return 0;
}
