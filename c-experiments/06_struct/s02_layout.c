/* s02_layout.c —— 内存布局、对齐(alignment)、填充(padding)、offsetof */
#include <stdio.h>
#include <stddef.h>
#include <stdalign.h>

/* 排列糟糕的版本：char, int, char, double */
struct Bad {
    char   a;
    int    b;
    char   c;
    double d;
};

/* 排列良好的版本：按对齐要求从大到小 */
struct Good {
    double d;
    int    b;
    char   a;
    char   c;
};

struct OneChar { char a; };

struct Nested {
    char        tag;
    struct Bad  inner;
    char        end;
};

#pragma pack(push, 1)
struct Packed {
    char   a;
    int    b;
    char   c;
    double d;
};
#pragma pack(pop)

static void dump_layout(const char *name, size_t total, const char *fields)
{
    printf("  %-14s sizeof=%2zu   %s\n", name, total, fields);
}

int main(void)
{
    puts("== 1. 成员顺序影响结构体大小 ==");
    printf("  struct Bad  { char a; int b; char c; double d; }\n");
    printf("    sizeof = %zu, alignof = %zu\n", sizeof(struct Bad), alignof(struct Bad));
    printf("    offsetof: a=%zu b=%zu c=%zu d=%zu\n",
           offsetof(struct Bad, a), offsetof(struct Bad, b),
           offsetof(struct Bad, c), offsetof(struct Bad, d));
    printf("    有效数据 = 1+4+1+8 = 14 字节，实际占 %zu 字节，浪费 %zu 字节\n",
           sizeof(struct Bad), sizeof(struct Bad) - 14);

    printf("\n  struct Good { double d; int b; char a; char c; }\n");
    printf("    sizeof = %zu, alignof = %zu\n", sizeof(struct Good), alignof(struct Good));
    printf("    offsetof: d=%zu b=%zu a=%zu c=%zu\n",
           offsetof(struct Good, d), offsetof(struct Good, b),
           offsetof(struct Good, a), offsetof(struct Good, c));
    printf("    同样的数据，只占 %zu 字节，浪费 %zu 字节\n",
           sizeof(struct Good), sizeof(struct Good) - 14);

    puts("\n== 2. 各基本类型的对齐要求 ==");
    printf("  alignof(char)=%zu  alignof(short)=%zu  alignof(int)=%zu\n",
           alignof(char), alignof(short), alignof(int));
    printf("  alignof(long)=%zu  alignof(double)=%zu alignof(void*)=%zu\n",
           alignof(long), alignof(double), alignof(void *));
    printf("  规则: 成员 offset 必须是自己 alignof 的整数倍；\n");
    printf("        结构体总大小必须是「最大成员 alignof」的整数倍（尾部补齐）\n");

    puts("\n== 3. 画出 struct Bad 的字节地图 ==");
    puts("    偏移  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15");
    puts("         [a][ ][ ][ ][    b     ][c][ ][ ][ ][ ][ ][ ][ ]");
    puts("         16 ......................23");
    puts("         [          d(double)          ]");
    puts("    [ ] = padding（编译器插入的空洞，内容不确定）");

    puts("\n== 4. 尾部填充（tail padding）==");
    printf("  struct OneChar { char a; }  sizeof = %zu\n", sizeof(struct OneChar));
    printf("  struct Good 的最后一个成员在 offset %zu，但 sizeof=%zu\n",
           offsetof(struct Good, c), sizeof(struct Good));
    printf("  因为数组里每个元素都必须满足对齐，所以尾部要补齐\n");
    struct Good arr[2];
    printf("  验证: &arr[1] - &arr[0] = %td 字节 = sizeof(struct Good)\n",
           (char *)&arr[1] - (char *)&arr[0]);

    puts("\n== 5. 嵌套结构体的对齐会向外传播 ==");
    printf("  struct Nested { char tag; struct Bad inner; char end; }\n");
    printf("    sizeof = %zu, alignof = %zu\n", sizeof(struct Nested), alignof(struct Nested));
    printf("    offsetof: tag=%zu inner=%zu end=%zu\n",
           offsetof(struct Nested, tag), offsetof(struct Nested, inner),
           offsetof(struct Nested, end));

    puts("\n== 6. #pragma pack 强制紧凑（代价：未对齐访问）==");
    printf("  struct Packed（pack(1)）sizeof = %zu\n", sizeof(struct Packed));
    printf("    offsetof: a=%zu b=%zu c=%zu d=%zu\n",
           offsetof(struct Packed, a), offsetof(struct Packed, b),
           offsetof(struct Packed, c), offsetof(struct Packed, d));
    printf("  只在解析二进制协议/文件格式时用，且要注意取成员地址会产生未对齐指针\n");

    puts("\n== 7. padding 字节的内容是不确定的 ==");
    puts("  => 不要用 memcmp 比较结构体");
    puts("  => 不要把带 padding 的结构体直接 fwrite 到文件再在别的平台读");

    dump_layout("Bad",    sizeof(struct Bad),    "char,int,char,double");
    dump_layout("Good",   sizeof(struct Good),   "double,int,char,char");
    dump_layout("Packed", sizeof(struct Packed), "pack(1)");
    dump_layout("Nested", sizeof(struct Nested), "char,Bad,char");

    puts("\n== 最佳实践 ==");
    puts("  1) 成员按对齐要求从大到小排列，天然省内存");
    puts("  2) 想确认布局就用 offsetof / sizeof / alignof，不要靠猜");
    puts("  3) 跨平台序列化要逐字段读写，不要整块 memcpy 结构体");

    return 0;
}
