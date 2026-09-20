/* p02_decay.c —— 数组退化（array decay）：sizeof(arr) vs sizeof(ptr) */
#include <stdio.h>

/* 注意：这三个函数签名「完全等价」，都是 int* */
static void by_pointer(int *a)
{
    printf("    void f(int *a)      : sizeof(a) = %zu  <-- 指针大小\n", sizeof(a));
}

static void by_array_syntax(int a[])
{
    printf("    void f(int a[])     : sizeof(a) = %zu  <-- 骗人的写法，还是指针\n", sizeof(a));
}

static void by_sized_array(int a[10])
{
    printf("    void f(int a[10])   : sizeof(a) = %zu  <-- 那个 10 被编译器无视\n", sizeof(a));
}

/* 唯一能保住长度的写法：传数组的「指针」 */
static void by_array_pointer(int (*a)[5])
{
    printf("    void f(int (*a)[5]) : sizeof(*a) = %zu  <-- 真的拿到了数组大小\n", sizeof(*a));
    printf("                          元素个数 = %zu\n", sizeof(*a) / sizeof((*a)[0]));
}

int main(void)
{
    int arr[5] = {1, 2, 3, 4, 5};

    puts("== 1. 在「看得见数组」的作用域里 ==");
    printf("  sizeof(arr)            = %zu  (5 个 int = 5*4)\n", sizeof(arr));
    printf("  sizeof(arr[0])         = %zu\n", sizeof(arr[0]));
    printf("  元素个数 = sizeof(arr)/sizeof(arr[0]) = %zu\n",
           sizeof(arr) / sizeof(arr[0]));

    puts("\n== 2. 数组名在大多数表达式里「退化」成指向首元素的指针 ==");
    int *p = arr;                 /* 等价于 &arr[0]，不需要写 & */
    printf("  arr      = %p\n", (void *)arr);
    printf("  &arr[0]  = %p   <-- 和 arr 相同\n", (void *)&arr[0]);
    printf("  &arr     = %p   <-- 数值相同，但类型是 int(*)[5]！\n", (void *)&arr);
    printf("  sizeof(p)= %zu   <-- 退化之后长度信息就没了\n", sizeof(p));

    puts("\n== 3. 类型不同 => +1 走的距离不同 ==");
    printf("  arr  + 1 = %p  (+%zu 字节, 跳 1 个 int)\n",
           (void *)(arr + 1), sizeof(int));
    printf("  &arr + 1 = %p  (+%zu 字节, 跳整个数组!)\n",
           (void *)(&arr + 1), sizeof(arr));

    puts("\n== 4. 传进函数以后，长度信息彻底丢失 ==");
    by_pointer(arr);
    by_array_syntax(arr);
    by_sized_array(arr);
    by_array_pointer(&arr);

    puts("\n== 5. 三种下标写法完全等价 ==");
    printf("  arr[2]    = %d\n", arr[2]);
    printf("  *(arr+2)  = %d\n", *(arr + 2));
    printf("  p[2]      = %d\n", p[2]);
    printf("  2[arr]    = %d   <-- 合法但请永远不要这么写\n", 2[arr]);
    printf("  因为 a[i] 被定义为 *(a + i)，加法可交换\n");

    puts("\n== 6. 不退化的两个例外 ==");
    printf("  sizeof(arr)  不退化 -> %zu\n", sizeof(arr));
    printf("  &arr         不退化 -> 类型 int(*)[5]\n");

    puts("\n== 结论 ==");
    puts("  只要把数组传给函数，就必须「额外把长度也传进去」。");

    return 0;
}
