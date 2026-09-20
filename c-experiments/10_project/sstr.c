/* sstr.c —— 字符串工具实现 */
#include "sstr.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

size_t sstr_copy(char *dst, size_t dstsize, const char *src)
{
    size_t srclen = strlen(src);
    if (dstsize == 0) { return srclen; }        /* 没空间，只报告需要多少 */

    size_t n = (srclen < dstsize - 1) ? srclen : dstsize - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';                               /* 永远补终止符 */
    return srclen;                               /* >= dstsize 表示被截断 */
}

/* strnlen 是 POSIX 不是 ISO C17，自己写一个保证可移植 */
static size_t sstr_nlen(const char *s, size_t maxlen)
{
    size_t i = 0;
    while (i < maxlen && s[i] != '\0') { i++; }
    return i;
}

size_t sstr_cat(char *dst, size_t dstsize, const char *src)
{
    size_t dstlen = sstr_nlen(dst, dstsize);
    size_t srclen = strlen(src);

    if (dstlen == dstsize) { return dstsize + srclen; }  /* dst 未终止 */

    size_t avail = dstsize - dstlen;
    size_t n = (srclen < avail - 1) ? srclen : avail - 1;
    memcpy(dst + dstlen, src, n);
    dst[dstlen + n] = '\0';
    return dstlen + srclen;
}

char *sstr_dup(const char *src)
{
    if (src == NULL) { return NULL; }
    size_t n = strlen(src);
    char *p = malloc(n + 1);                     /* +1 给 '\0' */
    if (p == NULL) { return NULL; }
    memcpy(p, src, n + 1);
    return p;
}

char *sstr_trim(char *s)
{
    if (s == NULL) { return NULL; }
    /* 掐头 */
    while (*s != '\0' && isspace((unsigned char)*s)) { s++; }
    if (*s == '\0') { return s; }
    /* 去尾 */
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) { end--; }
    end[1] = '\0';
    return s;
}

void sstr_upper(char *s)
{
    if (s == NULL) { return; }
    for (; *s != '\0'; s++) {
        /* 必须先转 unsigned char：char 可能是有符号的，负值传给 toupper 是 UB */
        *s = (char)toupper((unsigned char)*s);
    }
}

void sstr_lower(char *s)
{
    if (s == NULL) { return; }
    for (; *s != '\0'; s++) {
        *s = (char)tolower((unsigned char)*s);
    }
}

bool sstr_starts_with(const char *s, const char *prefix)
{
    if (s == NULL || prefix == NULL) { return false; }
    size_t n = strlen(prefix);
    return strncmp(s, prefix, n) == 0;
}

bool sstr_ends_with(const char *s, const char *suffix)
{
    if (s == NULL || suffix == NULL) { return false; }
    size_t sl = strlen(s);
    size_t fl = strlen(suffix);
    if (fl > sl) { return false; }
    return memcmp(s + sl - fl, suffix, fl) == 0;
}

size_t sstr_split(char *s, char sep, char **out, size_t maxparts)
{
    if (s == NULL || out == NULL || maxparts == 0) { return 0; }
    size_t count = 0;
    char  *cur = s;

    out[count++] = cur;
    while (*cur != '\0' && count < maxparts) {
        if (*cur == sep) {
            *cur = '\0';
            out[count++] = cur + 1;
        }
        cur++;
    }
    return count;
}
