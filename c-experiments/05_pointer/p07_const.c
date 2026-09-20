/* p07_const.c —— const 与指针的三种组合 */
#include <stdio.h>

/* 只读输入参数：承诺「我不改你的数据」 */
static long sum(const int *arr, size_t n)
{
    long s = 0;
    for (size_t i = 0; i < n; i++) {
        s += arr[i];          /* 只读，OK */
        /* arr[i] = 0;  <-- 编译错误：read-only */
    }
    return s;
}

static void fill(int *arr, size_t n, int v)
{
    for (size_t i = 0; i < n; i++) { arr[i] = v; }
}

int main(void)
{
    int a = 1, b = 2;

    puts("== 从右往左读声明 ==");
    puts("  const int *p        : p 是指针 -> 指向 const int   (数据只读，指针可改)");
    puts("  int const *p        : 和上面完全一样");
    puts("  int * const p       : p 是 const 指针 -> 指向 int   (指针只读，数据可改)");
    puts("  const int * const p : 两个都只读");
    puts("  技巧：看 const 在 * 的左边还是右边。左边管数据，右边管指针。");

    puts("\n== 1. const int *p  —— 指向常量的指针 ==");
    const int *p1 = &a;
    printf("  *p1 = %d\n", *p1);
    /* *p1 = 10;  <-- 编译错误: read-only variable is not assignable */
    p1 = &b;                      /* 允许：改的是指针本身 */
    printf("  p1 改指向 b, *p1 = %d   <-- 指针可以换目标\n", *p1);

    puts("\n== 2. int * const p —— 常量指针 ==");
    int * const p2 = &a;
    *p2 = 100;                    /* 允许：改的是数据 */
    printf("  *p2 = 100 之后 a = %d   <-- 数据可以改\n", a);
    /* p2 = &b;  <-- 编译错误: cannot assign to variable 'p2' with const-qualified type */
    printf("  但 p2 = &b 是编译错误\n");

    puts("\n== 3. const int * const p —— 全都锁死 ==");
    const int * const p3 = &a;
    printf("  *p3 = %d  (只能读)\n", *p3);

    puts("\n== 4. const 在函数接口里的价值 ==");
    int data[4];
    fill(data, 4, 5);
    printf("  fill(data,4,5) 之后 sum = %ld\n", sum(data, 4));
    printf("  sum 的参数写成 const int*，读代码的人一眼就知道它不会改数组\n");

    puts("\n== 5. 一个容易被坑的点 ==");
    puts("  int **       不能隐式转成 const int **（会被编译器拒绝/警告）");
    puts("  而 int *     可以隐式转成 const int * （加 const 是安全的）");

    puts("\n== 6. const 不等于「常量」 ==");
    const int n = 5;
    int arr2[5];                  /* C17 里 const int 不是常量表达式，这里写字面量 5 */
    (void)arr2;
    printf("  const int n = %d; 它是「只读变量」，不是编译期常量\n", n);
    printf("  需要编译期常量请用 enum 或 #define，或 C23 的 constexpr\n");

    return 0;
}
