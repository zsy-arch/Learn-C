/* sstr.h —— 字符串工具函数（安全版本） */
#ifndef SSTR_H
#define SSTR_H

#include <stddef.h>
#include <stdbool.h>

/* 安全拷贝：总是以 '\0' 结尾；返回「源串长度」，>= dstsize 说明被截断。
 * 语义与 BSD strlcpy 一致。 */
size_t sstr_copy(char *dst, size_t dstsize, const char *src);

/* 安全追加：同上 */
size_t sstr_cat(char *dst, size_t dstsize, const char *src);

/* 堆上复制一份（自己实现，避免依赖 POSIX strdup） */
char  *sstr_dup(const char *src);

/* 原地去掉首尾空白，返回新的起始位置（可能不等于 s） */
char  *sstr_trim(char *s);

/* 原地转大写/小写 */
void   sstr_upper(char *s);
void   sstr_lower(char *s);

bool   sstr_starts_with(const char *s, const char *prefix);
bool   sstr_ends_with(const char *s, const char *suffix);

/* 按分隔符切分，结果写进 out 数组，返回实际片段数（最多 maxparts 个）。
 * 会修改 s（把分隔符替换成 '\0'），out[i] 指向 s 内部。 */
size_t sstr_split(char *s, char sep, char **out, size_t maxparts);

#endif /* SSTR_H */
