/* s04_bitfield_union.c —— 位域、联合体、枚举、tagged union */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* ---- 位域：把多个小字段塞进一个整数 ---- */
struct Flags {
    unsigned int visible  : 1;
    unsigned int enabled  : 1;
    unsigned int level    : 4;   /* 0..15 */
    unsigned int reserved : 26;
};

/* 用位域解析一个 IPv4 头的前 8 位（教学用，真实场景要考虑字节序） */
struct IPv4FirstByte {
    unsigned int ihl     : 4;
    unsigned int version : 4;
};

/* ---- 联合体：所有成员共享同一块内存 ---- */
union Value32 {
    uint32_t u;
    int32_t  i;
    float    f;
    uint8_t  bytes[4];
};

/* ---- tagged union（带标签的联合体）：C 里表达「多选一」的标准做法 ---- */
typedef enum { VT_INT, VT_DOUBLE, VT_STRING } ValueType;

typedef struct {
    ValueType type;              /* 标签：告诉你现在哪个成员是活跃的 */
    union {
        long        i;
        double      d;
        const char *s;
    } as;
} Variant;

static void variant_print(const Variant *v)
{
    switch (v->type) {
    case VT_INT:    printf("    int    = %ld\n", v->as.i); break;
    case VT_DOUBLE: printf("    double = %g\n",  v->as.d); break;
    case VT_STRING: printf("    string = \"%s\"\n", v->as.s); break;
    }
}

int main(void)
{
    puts("== 1. 位域：省空间 ==");
    printf("  struct Flags 用 4 个字段，sizeof = %zu 字节\n", sizeof(struct Flags));
    printf("  如果用 4 个 int 要 %zu 字节\n", 4 * sizeof(int));

    struct Flags fl = {0};
    fl.visible = 1;
    fl.enabled = 0;
    fl.level   = 9;
    printf("  visible=%u enabled=%u level=%u\n", fl.visible, fl.enabled, fl.level);

    /* 4 位放不下 20（=0b10100），会被截断成 0b0100。
     * 这里用变量赋值，否则 clang 会在编译期报 -Wbitfield-constant-conversion。*/
    unsigned wanted = 20u;
    fl.level = wanted;
    printf("  把 level 设成 %u（4 位放不下）-> 实际值 = %u  <-- 悄悄截断！\n",
           wanted, fl.level);
    printf("  （写成字面量 fl.level = 20; 时 clang 会给 -Wbitfield-constant-conversion）\n");

    puts("\n== 2. 位域的可移植性陷阱 ==");
    puts("  - 位的排列顺序（先填低位还是高位）是 implementation-defined");
    puts("  - 能否跨存储单元边界也是 implementation-defined");
    puts("  - 不能对位域取地址（&fl.level 是编译错误）");
    puts("  => 解析网络协议时，用移位和掩码比位域更可靠");

    uint8_t raw = 0x45;     /* 典型的 IPv4 首字节: version=4, ihl=5 */
    printf("  用移位解析 0x%02X: version=%u ihl=%u  <-- 可移植写法\n",
           raw, (unsigned)(raw >> 4), (unsigned)(raw & 0x0Fu));

    struct IPv4FirstByte b;
    memcpy(&b, &raw, 1);
    printf("  用位域解析 0x%02X: version=%u ihl=%u  <-- 依赖实现！\n",
           raw, b.version, b.ihl);

    puts("\n== 3. 联合体：同一块内存的多种解释 ==");
    union Value32 v;
    printf("  sizeof(union Value32) = %zu  (= 最大成员的大小)\n", sizeof(v));
    v.f = 1.0f;
    printf("  写入 v.f = 1.0f\n");
    printf("    v.u     = 0x%08X   (IEEE-754 位模式)\n", v.u);
    printf("    v.i     = %d\n", v.i);
    printf("    v.bytes = %02X %02X %02X %02X\n",
           v.bytes[0], v.bytes[1], v.bytes[2], v.bytes[3]);
    printf("  在 C 里读取非活跃成员是允许的（C++ 不允许），值由对象表示决定\n");

    puts("\n== 4. 用联合体检测字节序 ==");
    union { uint32_t u; uint8_t b[4]; } endian;
    endian.u = 0x01020304u;
    printf("  0x01020304 的第一个字节 = 0x%02X -> %s endian\n",
           endian.b[0], (endian.b[0] == 0x04) ? "little" : "big");

    puts("\n== 5. tagged union：安全地表达「多选一」 ==");
    printf("  sizeof(Variant) = %zu\n", sizeof(Variant));
    Variant vs[3];
    vs[0].type = VT_INT;    vs[0].as.i = 42;
    vs[1].type = VT_DOUBLE; vs[1].as.d = 3.14;
    vs[2].type = VT_STRING; vs[2].as.s = "hello";
    for (size_t i = 0; i < 3; i++) { variant_print(&vs[i]); }
    puts("  关键：type 字段和 union 的活跃成员必须永远保持一致，这要靠你自己维护");

    puts("\n== 6. 枚举的底层类型 ==");
    typedef enum { A = 1, B = 2, C = 100 } Small;
    printf("  sizeof(enum{A,B,C}) = %zu  (C17 里枚举的底层类型由实现决定，通常是 int)\n",
           sizeof(Small));
    printf("  枚举变量可以被赋任意整数值，编译器不一定拦你：\n");
    Small s = (Small)999;
    printf("    Small s = (Small)999; s = %d\n", (int)s);

    return 0;
}
