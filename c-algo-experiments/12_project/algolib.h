/* algolib.h —— 一个小型算法与数据结构库的公开接口
 *
 * 设计原则（和《C 语言语法与最佳实践》10_project 一致）：
 *   1. 不完整类型（opaque pointer）：调用者看不到结构体内部
 *   2. 所有分配内存的对象都配套 xxx_new / xxx_free
 *   3. 所有可能失败的操作返回状态码，不在库里 exit()
 *   4. 所有接受数组的函数都显式传长度
 *   5. 所有只读指针参数都加 const
 */
#ifndef ALGOLIB_H
#define ALGOLIB_H

#include <stddef.h>
#include <stdbool.h>

/* ================================================================ */
/* 通用排序                                                          */
/* ================================================================ */
/* 全部原地排序 int 数组，n 是元素个数 */

void al_sort_bubble(int *a, size_t n);
void al_sort_insertion(int *a, size_t n);
void al_sort_selection(int *a, size_t n);
void al_sort_shell(int *a, size_t n);
void al_sort_quick(int *a, size_t n);
void al_sort_merge(int *a, size_t n);

/* ================================================================ */
/* 通用搜索                                                          */
/* ================================================================ */
/* 线性搜索：不需要有序，返回下标，失败返回 -1 */
long al_linear_search(const int *a, size_t n, int target);

/* 二分搜索：要求 a 已排序，返回任意一个匹配的下标，失败返回 -1 */
long al_binary_search(const int *a, size_t n, int target);

/* 下界：第一个 >= target 的位置（可能等于 n） */
size_t al_lower_bound(const int *a, size_t n, int target);
/* 上界：第一个 > target 的位置（可能等于 n） */
size_t al_upper_bound(const int *a, size_t n, int target);

/* ================================================================ */
/* 动态数组（不透明类型）                                              */
/* ================================================================ */
typedef struct AlVec AlVec;

AlVec *al_vec_new(size_t initial_cap);
void   al_vec_free(AlVec *v);
bool   al_vec_push(AlVec *v, int value);
bool   al_vec_get(const AlVec *v, size_t i, int *out);
bool   al_vec_set(AlVec *v, size_t i, int value);
bool   al_vec_pop(AlVec *v, int *out);
size_t al_vec_len(const AlVec *v);
size_t al_vec_cap(const AlVec *v);
int   *al_vec_data(AlVec *v);          /* 直接访问底层数组（可能失效） */
bool   al_vec_shrink(AlVec *v);

/* ================================================================ */
/* 单链表                                                            */
/* ================================================================ */
typedef struct AlList AlList;

AlList *al_list_new(void);
void    al_list_free(AlList *l);
bool    al_list_push_front(AlList *l, int v);
bool    al_list_push_back(AlList *l, int v);
bool    al_list_find(const AlList *l, int v);
bool    al_list_remove(AlList *l, int v);
size_t  al_list_remove_all(AlList *l, int v);
void    al_list_reverse(AlList *l);
size_t  al_list_len(const AlList *l);
/* 把链表内容导出到动态数组（调用者负责 free） */
AlVec  *al_list_to_vec(const AlList *l);

/* ================================================================ */
/* 哈希表（字符串 key -> int）                                         */
/* ================================================================ */
typedef struct AlHash AlHash;

AlHash *al_hash_new(void);
void    al_hash_free(AlHash *h);
bool    al_hash_put(AlHash *h, const char *key, int value);
bool    al_hash_get(const AlHash *h, const char *key, int *out);
bool    al_hash_del(AlHash *h, const char *key);
size_t  al_hash_count(const AlHash *h);
size_t  al_hash_buckets(const AlHash *h);
size_t  al_hash_max_chain(const AlHash *h);   /* 最长的冲突链 */

/* ================================================================ */
/* 二叉搜索树                                                         */
/* ================================================================ */
typedef struct AlTree AlTree;

AlTree *al_tree_new(void);
void    al_tree_free(AlTree *t);
bool    al_tree_insert(AlTree *t, int v);
bool    al_tree_contains(const AlTree *t, int v);
bool    al_tree_delete(AlTree *t, int v);
size_t  al_tree_size(const AlTree *t);
int     al_tree_height(const AlTree *t);
bool    al_tree_is_valid(const AlTree *t);

/* 遍历：把结果写入调用者提供的缓冲区，返回写入的元素个数。
 * 缓冲区不够时停止，返回实际写入数。 */
size_t  al_tree_preorder(const AlTree *t, int *out, size_t cap);
size_t  al_tree_inorder(const AlTree *t, int *out, size_t cap);
size_t  al_tree_postorder(const AlTree *t, int *out, size_t cap);
size_t  al_tree_levelorder(const AlTree *t, int *out, size_t cap);

#endif /* ALGOLIB_H */
