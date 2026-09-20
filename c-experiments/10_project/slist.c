/* slist.c —— 单链表实现 */
#include "slist.h"

#include <stdlib.h>

void slist_init(SList *l)
{
    if (l == NULL) { return; }
    l->head = NULL;
    l->tail = NULL;
    l->size = 0;
}

void slist_free(SList *l)
{
    if (l == NULL) { return; }
    SNode *cur = l->head;
    while (cur != NULL) {
        SNode *next = cur->next;   /* 先存住 next，free 之后就读不到了 */
        free(cur);
        cur = next;
    }
    l->head = NULL;
    l->tail = NULL;
    l->size = 0;
}

bool slist_push_front(SList *l, int v)
{
    if (l == NULL) { return false; }
    SNode *n = malloc(sizeof *n);
    if (n == NULL) { return false; }
    n->value = v;
    n->next  = l->head;
    l->head  = n;
    if (l->tail == NULL) { l->tail = n; }
    l->size++;
    return true;
}

bool slist_push_back(SList *l, int v)
{
    if (l == NULL) { return false; }
    SNode *n = malloc(sizeof *n);
    if (n == NULL) { return false; }
    n->value = v;
    n->next  = NULL;
    if (l->tail != NULL) {
        l->tail->next = n;
    } else {
        l->head = n;
    }
    l->tail = n;
    l->size++;
    return true;
}

bool slist_pop_front(SList *l, int *out)
{
    if (l == NULL || l->head == NULL) { return false; }
    SNode *n = l->head;
    if (out != NULL) { *out = n->value; }
    l->head = n->next;
    if (l->head == NULL) { l->tail = NULL; }
    free(n);
    l->size--;
    return true;
}

size_t slist_remove_all(SList *l, int v)
{
    if (l == NULL) { return 0; }
    size_t removed = 0;
    SNode **cur  = &l->head;
    SNode  *prev = NULL;

    while (*cur != NULL) {
        SNode *entry = *cur;
        if (entry->value == v) {
            *cur = entry->next;
            if (l->tail == entry) { l->tail = prev; }
            free(entry);
            l->size--;
            removed++;
        } else {
            prev = entry;
            cur  = &entry->next;
        }
    }
    return removed;
}

bool slist_contains(const SList *l, int v)
{
    if (l == NULL) { return false; }
    for (const SNode *c = l->head; c != NULL; c = c->next) {
        if (c->value == v) { return true; }
    }
    return false;
}

void slist_reverse(SList *l)
{
    if (l == NULL || l->head == NULL) { return; }
    SNode *prev = NULL;
    SNode *cur  = l->head;
    l->tail = cur;
    while (cur != NULL) {
        SNode *next = cur->next;
        cur->next = prev;
        prev = cur;
        cur  = next;
    }
    l->head = prev;
}

size_t slist_size(const SList *l) { return (l != NULL) ? l->size : 0; }

void slist_foreach(const SList *l, bool (*fn)(int value, void *ctx), void *ctx)
{
    if (l == NULL || fn == NULL) { return; }
    for (const SNode *c = l->head; c != NULL; c = c->next) {
        if (!fn(c->value, ctx)) { break; }
    }
}
