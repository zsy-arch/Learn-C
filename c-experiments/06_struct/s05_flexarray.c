/* s05_flexarray.c —— 柔性数组成员（flexible array member, C99）
 * 把「头部 + 变长数据」放进一次 malloc，减少一次分配、提高局部性。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

/* 柔性数组：最后一个成员写成 []，不占 sizeof */
typedef struct {
    size_t len;
    int    tag;
    char   data[];       /* 必须是最后一个成员，且前面至少还有一个成员 */
} Packet;

static Packet *packet_new(int tag, const char *payload)
{
    size_t n = strlen(payload);
    /* 一次分配：头部 + n + 1 个字节 */
    Packet *p = malloc(sizeof(Packet) + n + 1);
    if (p == NULL) { return NULL; }
    p->len = n;
    p->tag = tag;
    memcpy(p->data, payload, n + 1);
    return p;
}

/* 对照组：用指针成员实现，需要两次 malloc、两次 free */
typedef struct {
    size_t len;
    int    tag;
    char  *data;
} PacketPtr;

static PacketPtr *packetptr_new(int tag, const char *payload)
{
    PacketPtr *p = malloc(sizeof(PacketPtr));
    if (p == NULL) { return NULL; }
    size_t n = strlen(payload);
    p->data = malloc(n + 1);
    if (p->data == NULL) { free(p); return NULL; }
    memcpy(p->data, payload, n + 1);
    p->len = n;
    p->tag = tag;
    return p;
}

static void packetptr_free(PacketPtr *p)
{
    if (p != NULL) { free(p->data); free(p); }
}

int main(void)
{
    puts("== 1. 柔性数组成员不计入 sizeof ==");
    printf("  sizeof(Packet)    = %zu  (len + tag + padding，data[] 占 0)\n",
           sizeof(Packet));
    printf("  offsetof(Packet, data) = %zu\n", offsetof(Packet, data));
    printf("  sizeof(PacketPtr) = %zu  (len + tag + 一个指针)\n", sizeof(PacketPtr));

    puts("\n== 2. 一次分配，头和数据连续 ==");
    Packet *p = packet_new(7, "hello flexible array");
    if (p == NULL) { return 1; }
    printf("  p        = %p\n", (void *)p);
    printf("  p->data  = %p   <-- 紧跟在头部后面，偏移 %td\n",
           (void *)p->data, (char *)p->data - (char *)p);
    printf("  tag=%d len=%zu data=\"%s\"\n", p->tag, p->len, p->data);
    printf("  实际分配了 sizeof(Packet)+len+1 = %zu 字节\n",
           sizeof(Packet) + p->len + 1);
    free(p);      /* 只需要 free 一次 */

    puts("\n== 3. 对照：指针成员版本 ==");
    PacketPtr *q = packetptr_new(7, "hello flexible array");
    if (q == NULL) { return 1; }
    printf("  q        = %p\n", (void *)q);
    printf("  q->data  = %p   <-- 在堆上另一处，离得很远\n", (void *)q->data);
    printf("  两块内存相距 %td 字节\n",
           (char *)q->data > (char *)q
               ? (char *)q->data - (char *)q
               : (char *)q - (char *)q->data);
    packetptr_free(q);

    puts("\n== 4. 什么时候用柔性数组 ==");
    puts("  适合: 创建后长度不再变化的「头部+变长负载」，如网络包、消息、字符串对象");
    puts("  不适合: 需要 realloc 改长度且有别处持有指针的场景（realloc 会搬家）");
    puts("  注意: 含柔性数组的结构体不能直接放进数组，也不能按值赋值复制数据部分");

    puts("\n== 5. 历史写法对比 ==");
    puts("  char data[1];   // C89 的 struct hack，越界访问，技术上是 UB");
    puts("  char data[0];   // GCC 扩展，非标准");
    puts("  char data[];    // C99 标准柔性数组，用这个");

    return 0;
}
