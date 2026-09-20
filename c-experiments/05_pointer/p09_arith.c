/* p09_arith.c —— 指针运算、边界与 one-past-the-end */
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

int main(void)
{
    int arr[5] = {10, 20, 30, 40, 50};
    int *begin = arr;
    int *end   = arr + 5;      /* one-past-the-end：允许「指向」，不允许解引用 */

    puts("== 1. 指针 + 整数 ==");
    for (int *p = begin; p != end; p++) {
        printf("  p=%p  *p=%2d  (p - begin = %td)\n",
               (void *)p, *p, p - begin);
    }

    puts("\n== 2. 指针 - 指针 = 元素个数（类型是 ptrdiff_t）==");
    ptrdiff_t count = end - begin;
    printf("  end - begin = %td  个元素\n", count);
    printf("  字节差 = %td  (= %td * sizeof(int))\n",
           (const char *)end - (const char *)begin, count);
    printf("  打印 ptrdiff_t 用 %%td\n");

    puts("\n== 3. one-past-the-end 的规则 ==");
    printf("  arr+5 = %p  <-- 合法：可以计算、可以比较\n", (void *)(arr + 5));
    printf("  *(arr+5)     <-- 非法：解引用是 undefined behavior\n");
    printf("  arr+6        <-- 非法：连「算出这个地址」本身都是 UB\n");
    printf("  这就是所有 STL/迭代器风格循环写 p != end 的依据\n");

    puts("\n== 4. 指针比较 ==");
    printf("  begin <  end  ? %s\n", (begin < end) ? "true" : "false");
    printf("  只有指向同一个数组（或同一对象）的指针之间比较才有定义\n");

    puts("\n== 5. 逆序遍历的正确写法 ==");
    for (int *p = end; p-- != begin; ) {
        printf("  %d ", *p);
    }
    putchar('\n');

    puts("\n== 6. 用 char* 做字节级步进 ==");
    struct S { int a; double b; char c; };
    struct S s = { .a = 1, .b = 2.0, .c = 'x' };
    const unsigned char *raw = (const unsigned char *)&s;
    printf("  sizeof(struct S) = %zu\n", sizeof(s));
    printf("  原始字节: ");
    for (size_t i = 0; i < sizeof(s); i++) { printf("%02X ", raw[i]); }
    putchar('\n');
    printf("  char* 加减 1 就是「一个字节」，这是唯一能安全遍历对象表示的指针类型\n");

    puts("\n== 7. 整数与指针互转（可移植性警告）==");
    int v = 7;
    int *pv = &v;
    uintptr_t as_int = (uintptr_t)pv;
    int *back = (int *)as_int;
    printf("  pv        = %p\n", (void *)pv);
    printf("  uintptr_t = 0x%llX\n", (unsigned long long)as_int);
    printf("  转回来 *back = %d  (uintptr_t 往返是标准保证的)\n", *back);
    printf("  但 (int)pv 在 64 位平台会截断，绝对不要这么写\n");

    puts("\n== 8. 常见越界写法对照 ==");
    puts("  for (i = 0; i <= n; i++)  arr[i]   <-- 错，多访问一个");
    puts("  for (i = 0; i <  n; i++)  arr[i]   <-- 对");
    puts("  p = arr + n; *p                    <-- 错，解引用尾后指针");
    puts("  越界的现场请看 08_ub/u02_oob.c（ASan 会直接抓住）");

    return 0;
}
