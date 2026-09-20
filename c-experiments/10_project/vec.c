/* vec.c —— 动态数组实现 */
#include "vec.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define VEC_INIT_CAP 4

const char *vec_strerror(VecStatus s)
{
    switch (s) {
    case VEC_OK:     return "OK";
    case VEC_ENOMEM: return "out of memory";
    case VEC_ERANGE: return "index out of range";
    case VEC_EINVAL: return "invalid argument";
    }
    return "unknown";
}

void vec_init(Vec *v)
{
    if (v == NULL) { return; }
    v->data = NULL;
    v->len  = 0;
    v->cap  = 0;
}

void vec_free(Vec *v)
{
    if (v == NULL) { return; }
    free(v->data);
    v->data = NULL;      /* 置空：再次 vec_free 也安全 */
    v->len  = 0;
    v->cap  = 0;
}

VecStatus vec_reserve(Vec *v, size_t want)
{
    if (v == NULL) { return VEC_EINVAL; }
    if (want <= v->cap) { return VEC_OK; }

    size_t newcap = (v->cap == 0) ? VEC_INIT_CAP : v->cap;
    while (newcap < want) {
        /* 溢出检查：翻倍之前先确认不会绕回 */
        if (newcap > SIZE_MAX / 2) { newcap = want; break; }
        newcap *= 2;
    }
    /* 乘法溢出检查 */
    if (newcap > SIZE_MAX / sizeof *v->data) { return VEC_ENOMEM; }

    int *tmp = realloc(v->data, newcap * sizeof *v->data);
    if (tmp == NULL) { return VEC_ENOMEM; }   /* 原 v->data 保持有效 */

    v->data = tmp;
    v->cap  = newcap;
    return VEC_OK;
}

VecStatus vec_push(Vec *v, int value)
{
    if (v == NULL) { return VEC_EINVAL; }
    if (v->len == v->cap) {
        VecStatus st = vec_reserve(v, v->len + 1);
        if (st != VEC_OK) { return st; }
    }
    v->data[v->len++] = value;
    return VEC_OK;
}

VecStatus vec_pop(Vec *v, int *out)
{
    if (v == NULL) { return VEC_EINVAL; }
    if (v->len == 0) { return VEC_ERANGE; }
    v->len--;
    if (out != NULL) { *out = v->data[v->len]; }
    return VEC_OK;
}

VecStatus vec_get(const Vec *v, size_t i, int *out)
{
    if (v == NULL || out == NULL) { return VEC_EINVAL; }
    if (i >= v->len) { return VEC_ERANGE; }    /* 无符号比较，负数会变成巨大值也能拦住 */
    *out = v->data[i];
    return VEC_OK;
}

VecStatus vec_set(Vec *v, size_t i, int value)
{
    if (v == NULL) { return VEC_EINVAL; }
    if (i >= v->len) { return VEC_ERANGE; }
    v->data[i] = value;
    return VEC_OK;
}

VecStatus vec_insert(Vec *v, size_t i, int value)
{
    if (v == NULL) { return VEC_EINVAL; }
    if (i > v->len) { return VEC_ERANGE; }     /* i == len 相当于 push */
    VecStatus st = vec_reserve(v, v->len + 1);
    if (st != VEC_OK) { return st; }
    /* 区间重叠，必须用 memmove */
    memmove(v->data + i + 1, v->data + i, (v->len - i) * sizeof *v->data);
    v->data[i] = value;
    v->len++;
    return VEC_OK;
}

VecStatus vec_remove(Vec *v, size_t i, int *out)
{
    if (v == NULL) { return VEC_EINVAL; }
    if (i >= v->len) { return VEC_ERANGE; }
    if (out != NULL) { *out = v->data[i]; }
    memmove(v->data + i, v->data + i + 1, (v->len - i - 1) * sizeof *v->data);
    v->len--;
    return VEC_OK;
}

VecStatus vec_shrink_to_fit(Vec *v)
{
    if (v == NULL) { return VEC_EINVAL; }
    if (v->len == v->cap) { return VEC_OK; }
    if (v->len == 0) {
        free(v->data);
        v->data = NULL;
        v->cap  = 0;
        return VEC_OK;
    }
    int *tmp = realloc(v->data, v->len * sizeof *v->data);
    if (tmp == NULL) { return VEC_ENOMEM; }
    v->data = tmp;
    v->cap  = v->len;
    return VEC_OK;
}

size_t vec_len(const Vec *v) { return (v != NULL) ? v->len : 0; }
size_t vec_cap(const Vec *v) { return (v != NULL) ? v->cap : 0; }
