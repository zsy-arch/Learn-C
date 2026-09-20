/* demo.c —— 分支：if/else、switch、条件表达式、短路求值、枚举状态机 */
#include <stdio.h>
#include <stdbool.h>

/* 带副作用的「探针」函数：用来肉眼观察哪些表达式真的被求值了 */
static int probe(const char *tag, int value)
{
    printf("    [求值] %s -> %d\n", tag, value);
    return value;
}

/* ---- 枚举 + switch 写状态机 ---- */
typedef enum {
    LIGHT_RED,
    LIGHT_GREEN,
    LIGHT_YELLOW,
    LIGHT_COUNT          /* 惯用法：放一个 COUNT 在最后，自动等于枚举个数 */
} Light;

static const char *light_name(Light l)
{
    switch (l) {
    case LIGHT_RED:    return "RED";
    case LIGHT_GREEN:  return "GREEN";
    case LIGHT_YELLOW: return "YELLOW";
    case LIGHT_COUNT:  break;
    }
    return "?";
}

static Light light_next(Light l)
{
    switch (l) {
    case LIGHT_RED:    return LIGHT_GREEN;
    case LIGHT_GREEN:  return LIGHT_YELLOW;
    case LIGHT_YELLOW: return LIGHT_RED;
    case LIGHT_COUNT:  break;
    }
    return LIGHT_RED;
}

int main(void)
{
    puts("== 1. if / else if / else ==");
    for (int score = 95; score >= 45; score -= 25) {
        const char *grade;
        if (score >= 90)      grade = "A";
        else if (score >= 80) grade = "B";
        else if (score >= 60) grade = "C";
        else                  grade = "F";
        printf("  score=%3d -> %s\n", score, grade);
    }

    puts("\n== 2. 短路求值：&& 左边为假，右边根本不执行 ==");
    printf("  表达式: probe(\"A\",0) && probe(\"B\",1)\n");
    int r1 = probe("A", 0) && probe("B", 1);
    printf("  结果 = %d（注意 B 没有被求值）\n", r1);

    printf("  表达式: probe(\"C\",1) || probe(\"D\",1)\n");
    int r2 = probe("C", 1) || probe("D", 1);
    printf("  结果 = %d（注意 D 没有被求值）\n", r2);

    puts("\n== 3. 短路求值最重要的用途：先判空指针，再解引用 ==");
    int  value = 99;
    int *p = NULL;
    if (p != NULL && *p == 1) {           /* 顺序反过来就会崩 */
        puts("  不可能走到这里");
    } else {
        puts("  p 是 NULL，&& 短路保护了 *p，没有崩溃");
    }
    p = &value;
    if (p != NULL && *p == 99) {
        printf("  p 非空且 *p == %d\n", *p);
    }

    puts("\n== 4. 条件（三目）表达式 ==");
    int a = 3, b = 8;
    printf("  max(%d,%d) = %d\n", a, b, (a > b) ? a : b);
    printf("  三目表达式是「表达式」，可以直接放进 printf；if 是「语句」，不行\n");

    puts("\n== 5. switch：有意的 fallthrough（合并 case）==");
    for (char c = 'a'; c <= 'e'; c++) {
        switch (c) {
        case 'a':
        case 'e':
        case 'i':
        case 'o':
        case 'u':
            printf("  '%c' 是元音\n", c);
            break;
        default:
            printf("  '%c' 是辅音\n", c);
            break;
        }
    }

    puts("\n== 6. switch 只能用整型/枚举，且 case 必须是常量表达式 ==");
    int cmd = 2;
    switch (cmd) {
    case 1: puts("  cmd=1 -> start"); break;
    case 2: puts("  cmd=2 -> stop");  break;
    default: puts("  未知命令");      break;
    }

    puts("\n== 7. 枚举状态机 ==");
    printf("  LIGHT_COUNT = %d（枚举成员默认从 0 递增）\n", LIGHT_COUNT);
    Light l = LIGHT_RED;
    for (int step = 0; step < 6; step++) {
        printf("  step %d: %-6s -> %s\n", step, light_name(l), light_name(light_next(l)));
        l = light_next(l);
    }

    puts("\n== 8. 大括号永远写上 ==");
    bool flag = false;
    if (flag) {
        puts("  A");
        puts("  B");
    }
    puts("  即使只有一行也写 {}，否则后期加一行就会悄悄改变语义");

    return 0;
}
