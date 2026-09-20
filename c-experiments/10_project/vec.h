/* vec.h —— 动态数组（dynamic array / vector）
 *
 * 设计要点：
 *   - 所有可能失败的操作都返回状态码，不在库里 exit()
 *   - 容量按 1.5~2 倍增长，摊还 O(1)
 *   - realloc 结果先接临时变量，失败时不破坏原状态
 */
#ifndef VEC_H
#define VEC_H

#include <stddef.h>

typedef struct {
    int   *data;
    size_t len;      /* 当前元素个数 */
    size_t cap;      /* 已分配的元素容量 */
} Vec;

typedef enum {
    VEC_OK = 0,
    VEC_ENOMEM,      /* 内存不足 */
    VEC_ERANGE,      /* 下标越界 */
    VEC_EINVAL       /* 参数非法 */
} VecStatus;

const char *vec_strerror(VecStatus s);

void       vec_init(Vec *v);
void       vec_free(Vec *v);
VecStatus  vec_reserve(Vec *v, size_t want);
VecStatus  vec_push(Vec *v, int value);
VecStatus  vec_pop(Vec *v, int *out);
VecStatus  vec_get(const Vec *v, size_t i, int *out);
VecStatus  vec_set(Vec *v, size_t i, int value);
VecStatus  vec_insert(Vec *v, size_t i, int value);
VecStatus  vec_remove(Vec *v, size_t i, int *out);
VecStatus  vec_shrink_to_fit(Vec *v);
size_t     vec_len(const Vec *v);
size_t     vec_cap(const Vec *v);

#endif /* VEC_H */
