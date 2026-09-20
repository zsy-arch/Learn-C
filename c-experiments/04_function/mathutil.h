/* mathutil.h —— 一个规范的模块头文件长什么样 */
#ifndef MATHUTIL_H
#define MATHUTIL_H

#include <stdbool.h>
#include <stddef.h>

/* 只放声明、类型、宏；不放定义（否则多个 .c include 后会重复定义） */

int  mu_add(int a, int b);
int  mu_clamp(int v, int lo, int hi);
bool mu_is_prime(unsigned n);

/* 数组求和：把长度一起传进来，因为数组在参数里会退化成指针 */
long mu_sum(const int *arr, size_t n);

#endif /* MATHUTIL_H */
