/* p01_basics.c —— 指针的本质：变量、地址、类型 */
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

int main(void)
{
    puts("== 1. 变量住在内存里，& 取出它的门牌号 ==");
    int   n  = 0x11223344;
    int  *p  = &n;

    printf("  n  的值   = 0x%08X\n", (unsigned)n);
    printf("  n  的地址 = %p        <-- &n\n", (void *)&n);
    printf("  p  的值   = %p        <-- p 保存的就是 n 的地址\n", (void *)p);
    printf("  p  的地址 = %p        <-- 指针自己也是个变量，也有地址\n", (void *)&p);
    printf("  *p 的值   = 0x%08X    <-- * 顺着地址把值取回来\n", (unsigned)*p);

    puts("\n== 2. 通过指针写，就是改原变量 ==");
    *p = 7;
    printf("  *p = 7 之后, n = %d\n", n);

    puts("\n== 3. 指针类型决定「解引用读几个字节」 ==");
    uint32_t word = 0xAABBCCDDu;
    unsigned char *pb = (unsigned char *)&word;
    uint16_t      *ph = (uint16_t *)&word;
    uint32_t      *pw = &word;

    printf("  word            = 0x%08X\n", word);
    printf("  *(unsigned char*)&word = 0x%02X    (读 1 字节)\n", *pb);
    printf("  *(uint16_t*)&word      = 0x%04X  (读 2 字节)\n", *ph);
    printf("  *(uint32_t*)&word      = 0x%08X (读 4 字节)\n", *pw);
    printf("  字节序: ");
    for (size_t i = 0; i < sizeof(word); i++) {
        printf("%02X ", pb[i]);
    }
    printf(" -> 低位在前，说明本机是 little-endian\n");

    puts("\n== 4. 指针类型决定「+1 走多远」 ==");
    printf("  sizeof(char)=%zu  sizeof(int)=%zu  sizeof(double)=%zu\n",
           sizeof(char), sizeof(int), sizeof(double));
    char   *cp = (char *)0x1000;
    int    *ip = (int *)0x1000;
    double *dp = (double *)0x1000;
    printf("  char*   0x1000 + 1 = %p  (+%zu)\n", (void *)(cp + 1), sizeof(char));
    printf("  int*    0x1000 + 1 = %p  (+%zu)\n", (void *)(ip + 1), sizeof(int));
    printf("  double* 0x1000 + 1 = %p  (+%zu)\n", (void *)(dp + 1), sizeof(double));
    printf("  规律: p + k 的真实地址偏移 = k * sizeof(*p)\n");

    puts("\n== 5. 所有指针本身一样大（本平台 8 字节）==");
    printf("  sizeof(char*)=%zu sizeof(int*)=%zu sizeof(double*)=%zu sizeof(void*)=%zu\n",
           sizeof(char *), sizeof(int *), sizeof(double *), sizeof(void *));
    printf("  但它们「指向的东西」大小不同，这才是类型的意义\n");

    return 0;
}
