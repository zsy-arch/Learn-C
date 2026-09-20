/* slist.h —— 单链表（singly linked list），存 int */
#ifndef SLIST_H
#define SLIST_H

#include <stddef.h>
#include <stdbool.h>

typedef struct SNode {
    int           value;
    struct SNode *next;
} SNode;

typedef struct {
    SNode *head;
    SNode *tail;     /* 维护 tail 让尾插变成 O(1) */
    size_t size;
} SList;

void   slist_init(SList *l);
void   slist_free(SList *l);
bool   slist_push_front(SList *l, int v);
bool   slist_push_back(SList *l, int v);
bool   slist_pop_front(SList *l, int *out);
size_t slist_remove_all(SList *l, int v);
bool   slist_contains(const SList *l, int v);
void   slist_reverse(SList *l);
size_t slist_size(const SList *l);

/* 遍历回调：返回 false 表示提前停止 */
void   slist_foreach(const SList *l, bool (*fn)(int value, void *ctx), void *ctx);

#endif /* SLIST_H */
