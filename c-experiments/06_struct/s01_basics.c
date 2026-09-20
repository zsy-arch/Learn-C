/* s01_basics.c —— 结构体定义、初始化、赋值是浅拷贝 */
#include <stdio.h>
#include <string.h>

struct Point {
    int x;
    int y;
};

/* typedef 让使用处不用每次写 struct */
typedef struct {
    char   name[16];
    int    age;
    double score;
} Student;

/* 嵌套结构体 */
typedef struct {
    struct Point topleft;
    struct Point bottomright;
    const char  *label;
} Rect;

/* 结构体可以按值传递，也可以按值返回（会整体拷贝） */
static struct Point point_add(struct Point a, struct Point b)
{
    struct Point r = { a.x + b.x, a.y + b.y };
    return r;
}

/* 大结构体优先传 const 指针，避免整块拷贝 */
static void print_student(const Student *s)
{
    printf("    {name=\"%s\", age=%d, score=%.1f}\n", s->name, s->age, s->score);
}

int main(void)
{
    puts("== 1. 三种初始化写法 ==");
    struct Point p1 = {1, 2};                       /* 顺序初始化 */
    struct Point p2 = {.y = 20, .x = 10};           /* 指定初始化器（C99），顺序随意 */
    struct Point p3 = {0};                          /* 全部清零的惯用法 */
    printf("  p1 = (%d, %d)\n", p1.x, p1.y);
    printf("  p2 = (%d, %d)  <-- 用 .name= 写，顺序无所谓且不怕以后加字段\n", p2.x, p2.y);
    printf("  p3 = (%d, %d)  <-- {0} 把所有成员清零\n", p3.x, p3.y);

    puts("\n== 2. . 和 -> ==");
    struct Point *pp = &p1;
    printf("  p1.x       = %d   (对象用 .)\n", p1.x);
    printf("  pp->x      = %d   (指针用 ->)\n", pp->x);
    printf("  (*pp).x    = %d   (等价写法，-> 只是语法糖)\n", (*pp).x);

    puts("\n== 3. 结构体赋值 = 逐字节整体拷贝 ==");
    struct Point a = {1, 2};
    struct Point b;
    b = a;                      /* 合法！结构体可以整体赋值（数组不行） */
    b.x = 99;
    printf("  b = a; b.x = 99;  =>  a=(%d,%d)  b=(%d,%d)\n", a.x, a.y, b.x, b.y);
    printf("  a 没被影响，说明是拷贝而不是引用\n");
    printf("  &a=%p  &b=%p  两个独立对象\n", (void *)&a, (void *)&b);

    puts("\n== 4. 值传递 / 值返回 ==");
    struct Point s = point_add(p1, p2);
    printf("  point_add((1,2),(10,20)) = (%d,%d)\n", s.x, s.y);

    puts("\n== 5. 数组成员会被一起拷贝（和裸数组不同！）==");
    Student s1 = {.name = "Alice", .age = 20, .score = 91.5};
    Student s2 = s1;                  /* name[16] 整块被复制 */
    strcpy(s2.name, "Bob");
    s2.age = 21;
    printf("  s1: "); print_student(&s1);
    printf("  s2: "); print_student(&s2);
    printf("  改 s2 不影响 s1 —— 因为 name 是「数组成员」，随结构体一起被复制\n");
    printf("  sizeof(Student) = %zu\n", sizeof(Student));

    puts("\n== 6. 嵌套结构体 ==");
    Rect r = {
        .topleft     = {.x = 0,  .y = 0},
        .bottomright = {.x = 10, .y = 5},
        .label       = "box",
    };
    printf("  %s: (%d,%d) -> (%d,%d), 面积 = %d\n",
           r.label, r.topleft.x, r.topleft.y,
           r.bottomright.x, r.bottomright.y,
           (r.bottomright.x - r.topleft.x) * (r.bottomright.y - r.topleft.y));

    puts("\n== 7. 结构体比较不能用 == ==");
    printf("  a == b 是编译错误；因为 padding 字节的内容不确定，\n");
    printf("  memcmp 也不可靠。要逐成员比较：\n");
    printf("  a 和 p1 相等吗? %s\n",
           (a.x == p1.x && a.y == p1.y) ? "是" : "否");

    return 0;
}
