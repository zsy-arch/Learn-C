/* container.c —— 动态数组 / 链表 / 哈希表 / BST 的实现 */
#include "algolib.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ================================================================ */
/* 动态数组                                                          */
/* ================================================================ */
/*
 * 不透明类型：调用者只看到 `AlVec *`，看不到内部布局。
 * 好处：
 *   1. 实现可以随时改，不用重编调用者
 *   2. 调用者无法绕过接口直接改 len/cap 造成不一致
 */
struct AlVec {
    int    *data;
    size_t  len;
    size_t  cap;
};

#define ALVEC_INIT_CAP 4

AlVec *al_vec_new(size_t initial_cap)
{
    AlVec *v = malloc(sizeof *v);
    if (v == NULL) { return NULL; }
    v->len = 0;
    v->cap = (initial_cap > 0) ? initial_cap : 0;
    v->data = (v->cap > 0) ? malloc(v->cap * sizeof *v->data) : NULL;
    if (v->cap > 0 && v->data == NULL) { free(v); return NULL; }
    return v;
}

void al_vec_free(AlVec *v)
{
    if (v == NULL) { return; }
    free(v->data);
    free(v);
}

static bool vec_reserve(AlVec *v, size_t need)
{
    if (need <= v->cap) { return true; }

    size_t newcap = (v->cap == 0) ? ALVEC_INIT_CAP : v->cap;
    while (newcap < need) {
        if (newcap > SIZE_MAX / 2) { newcap = need; break; }   /* 防回绕 */
        newcap *= 2;
    }
    if (newcap > SIZE_MAX / sizeof *v->data) { return false; } /* 乘法防溢出 */

    int *tmp = realloc(v->data, newcap * sizeof *v->data);
    if (tmp == NULL) { return false; }        /* 原数据保持有效 */
    v->data = tmp;
    v->cap  = newcap;
    return true;
}

bool al_vec_push(AlVec *v, int value)
{
    if (v == NULL) { return false; }
    if (!vec_reserve(v, v->len + 1)) { return false; }
    v->data[v->len++] = value;
    return true;
}

bool al_vec_get(const AlVec *v, size_t i, int *out)
{
    if (v == NULL || out == NULL) { return false; }
    if (i >= v->len) { return false; }        /* 无符号比较，负数也能拦住 */
    *out = v->data[i];
    return true;
}

bool al_vec_set(AlVec *v, size_t i, int value)
{
    if (v == NULL) { return false; }
    if (i >= v->len) { return false; }
    v->data[i] = value;
    return true;
}

bool al_vec_pop(AlVec *v, int *out)
{
    if (v == NULL || v->len == 0) { return false; }
    v->len--;
    if (out != NULL) { *out = v->data[v->len]; }
    return true;
}

size_t al_vec_len(const AlVec *v) { return (v != NULL) ? v->len : 0; }
size_t al_vec_cap(const AlVec *v) { return (v != NULL) ? v->cap : 0; }

int *al_vec_data(AlVec *v) { return (v != NULL) ? v->data : NULL; }

bool al_vec_shrink(AlVec *v)
{
    if (v == NULL) { return false; }
    if (v->len == v->cap) { return true; }
    if (v->len == 0) {
        free(v->data);
        v->data = NULL;
        v->cap = 0;
        return true;
    }
    int *tmp = realloc(v->data, v->len * sizeof *v->data);
    if (tmp == NULL) { return false; }
    v->data = tmp;
    v->cap = v->len;
    return true;
}

/* ================================================================ */
/* 单链表                                                            */
/* ================================================================ */
typedef struct AlNode {
    int            value;
    struct AlNode *next;
} AlNode;

struct AlList {
    AlNode *head;
    AlNode *tail;      /* 维护 tail 让尾插 O(1) */
    size_t  size;
};

AlList *al_list_new(void)
{
    AlList *l = malloc(sizeof *l);
    if (l == NULL) { return NULL; }
    l->head = NULL;
    l->tail = NULL;
    l->size = 0;
    return l;
}

void al_list_free(AlList *l)
{
    if (l == NULL) { return; }
    AlNode *cur = l->head;
    while (cur != NULL) {
        AlNode *next = cur->next;      /* 必须先存！free 后就读不到 next 了 */
        free(cur);
        cur = next;
    }
    free(l);
}

bool al_list_push_front(AlList *l, int v)
{
    if (l == NULL) { return false; }
    AlNode *n = malloc(sizeof *n);
    if (n == NULL) { return false; }
    n->value = v;
    n->next  = l->head;
    l->head  = n;
    if (l->tail == NULL) { l->tail = n; }
    l->size++;
    return true;
}

bool al_list_push_back(AlList *l, int v)
{
    if (l == NULL) { return false; }
    AlNode *n = malloc(sizeof *n);
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

bool al_list_find(const AlList *l, int v)
{
    if (l == NULL) { return false; }
    for (const AlNode *c = l->head; c != NULL; c = c->next) {
        if (c->value == v) { return true; }
    }
    return false;
}

bool al_list_remove(AlList *l, int v)
{
    if (l == NULL) { return false; }
    AlNode **cur  = &l->head;              /* 二级指针：零特判 */
    AlNode  *prev = NULL;
    while (*cur != NULL) {
        AlNode *entry = *cur;
        if (entry->value == v) {
            *cur = entry->next;
            if (l->tail == entry) { l->tail = prev; }
            free(entry);
            l->size--;
            return true;
        }
        prev = entry;
        cur  = &entry->next;
    }
    return false;
}

size_t al_list_remove_all(AlList *l, int v)
{
    if (l == NULL) { return 0; }
    size_t removed = 0;
    AlNode **cur  = &l->head;
    AlNode  *prev = NULL;
    while (*cur != NULL) {
        AlNode *entry = *cur;
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

void al_list_reverse(AlList *l)
{
    if (l == NULL || l->head == NULL) { return; }
    AlNode *prev = NULL;
    AlNode *cur  = l->head;
    l->tail = cur;                      /* 原来的头会变成尾 */
    while (cur != NULL) {
        AlNode *next = cur->next;       /* ① 先保存后继 */
        cur->next = prev;               /* ② 掉头 */
        prev = cur;                     /* ③ prev 前进 */
        cur  = next;                    /* ④ cur 前进 */
    }
    l->head = prev;
}

size_t al_list_len(const AlList *l) { return (l != NULL) ? l->size : 0; }

AlVec *al_list_to_vec(const AlList *l)
{
    if (l == NULL) { return NULL; }
    AlVec *v = al_vec_new(l->size);
    if (v == NULL) { return NULL; }
    for (const AlNode *c = l->head; c != NULL; c = c->next) {
        if (!al_vec_push(v, c->value)) { al_vec_free(v); return NULL; }
    }
    return v;
}

/* ================================================================ */
/* 哈希表（链地址法）                                                  */
/* ================================================================ */
typedef struct AlHEntry {
    char            *key;
    int              value;
    uint64_t         hash;
    struct AlHEntry *next;
} AlHEntry;

struct AlHash {
    AlHEntry **buckets;
    size_t     nbuckets;
    size_t     count;
};

#define ALHASH_INIT_BUCKETS 16
#define ALHASH_MAX_LOAD     0.75

/* FNV-1a —— 分布好，实现简单 */
static uint64_t hash_str(const char *s)
{
    uint64_t h = 1469598103934665603ULL;
    for (; *s; s++) {
        h ^= (unsigned char)*s;
        h *= 1099511628211ULL;
    }
    return h;
}

AlHash *al_hash_new(void)
{
    AlHash *h = malloc(sizeof *h);
    if (h == NULL) { return NULL; }
    h->buckets = calloc(ALHASH_INIT_BUCKETS, sizeof *h->buckets);
    if (h->buckets == NULL) { free(h); return NULL; }
    h->nbuckets = ALHASH_INIT_BUCKETS;
    h->count = 0;
    return h;
}

void al_hash_free(AlHash *h)
{
    if (h == NULL) { return; }
    for (size_t b = 0; b < h->nbuckets; b++) {
        AlHEntry *c = h->buckets[b];
        while (c != NULL) {
            AlHEntry *next = c->next;
            free(c->key);
            free(c);
            c = next;
        }
    }
    free(h->buckets);
    free(h);
}

static bool hash_grow(AlHash *h)
{
    size_t newn = h->nbuckets * 2;
    if (newn > SIZE_MAX / sizeof *h->buckets) { return false; }
    AlHEntry **nb = calloc(newn, sizeof *nb);
    if (nb == NULL) { return false; }

    for (size_t b = 0; b < h->nbuckets; b++) {
        AlHEntry *c = h->buckets[b];
        while (c != NULL) {
            AlHEntry *next = c->next;
            size_t idx = (size_t)(c->hash % newn);    /* 复用缓存的 hash */
            c->next = nb[idx];
            nb[idx] = c;
            c = next;
        }
    }
    free(h->buckets);
    h->buckets = nb;
    h->nbuckets = newn;
    return true;
}

bool al_hash_put(AlHash *h, const char *key, int value)
{
    if (h == NULL || key == NULL) { return false; }

    if ((double)(h->count + 1) / (double)h->nbuckets > ALHASH_MAX_LOAD) {
        if (!hash_grow(h)) { return false; }
    }

    uint64_t hv = hash_str(key);
    size_t   b  = (size_t)(hv % h->nbuckets);

    for (AlHEntry *c = h->buckets[b]; c != NULL; c = c->next) {
        if (c->hash == hv && strcmp(c->key, key) == 0) {
            c->value = value;          /* 已存在：覆盖 */
            return true;
        }
    }

    AlHEntry *n = malloc(sizeof *n);
    if (n == NULL) { return false; }
    size_t klen = strlen(key);
    n->key = malloc(klen + 1);
    if (n->key == NULL) { free(n); return false; }
    memcpy(n->key, key, klen + 1);
    n->value = value;
    n->hash  = hv;
    n->next  = h->buckets[b];
    h->buckets[b] = n;
    h->count++;
    return true;
}

bool al_hash_get(const AlHash *h, const char *key, int *out)
{
    if (h == NULL || key == NULL) { return false; }
    uint64_t hv = hash_str(key);
    size_t   b  = (size_t)(hv % h->nbuckets);
    for (const AlHEntry *c = h->buckets[b]; c != NULL; c = c->next) {
        if (c->hash == hv && strcmp(c->key, key) == 0) {
            if (out != NULL) { *out = c->value; }
            return true;
        }
    }
    return false;
}

bool al_hash_del(AlHash *h, const char *key)
{
    if (h == NULL || key == NULL) { return false; }
    uint64_t hv = hash_str(key);
    size_t   b  = (size_t)(hv % h->nbuckets);

    AlHEntry **cur = &h->buckets[b];
    while (*cur != NULL) {
        AlHEntry *entry = *cur;
        if (entry->hash == hv && strcmp(entry->key, key) == 0) {
            *cur = entry->next;
            free(entry->key);
            free(entry);
            h->count--;
            return true;
        }
        cur = &entry->next;
    }
    return false;
}

size_t al_hash_count(const AlHash *h) { return (h != NULL) ? h->count : 0; }
size_t al_hash_buckets(const AlHash *h) { return (h != NULL) ? h->nbuckets : 0; }

size_t al_hash_max_chain(const AlHash *h)
{
    if (h == NULL) { return 0; }
    size_t maxchain = 0;
    for (size_t b = 0; b < h->nbuckets; b++) {
        size_t len = 0;
        for (const AlHEntry *c = h->buckets[b]; c != NULL; c = c->next) { len++; }
        if (len > maxchain) { maxchain = len; }
    }
    return maxchain;
}

/* ================================================================ */
/* 二叉搜索树                                                         */
/* ================================================================ */
typedef struct AlTreeNode {
    int                value;
    struct AlTreeNode *left;
    struct AlTreeNode *right;
} AlTreeNode;

struct AlTree {
    AlTreeNode *root;
    size_t      count;
};

AlTree *al_tree_new(void)
{
    AlTree *t = malloc(sizeof *t);
    if (t == NULL) { return NULL; }
    t->root = NULL;
    t->count = 0;
    return t;
}

static void tree_free_rec(AlTreeNode *n)
{
    if (n == NULL) { return; }
    tree_free_rec(n->left);
    tree_free_rec(n->right);
    free(n);
}

void al_tree_free(AlTree *t)
{
    if (t == NULL) { return; }
    tree_free_rec(t->root);
    free(t);
}

bool al_tree_insert(AlTree *t, int v)
{
    if (t == NULL) { return false; }
    AlTreeNode **cur = &t->root;        /* 二级指针：零特判 */
    while (*cur != NULL) {
        if (v == (*cur)->value) { return false; }   /* 不插入重复值 */
        cur = (v < (*cur)->value) ? &(*cur)->left : &(*cur)->right;
    }
    AlTreeNode *n = malloc(sizeof *n);
    if (n == NULL) { return false; }
    n->value = v;
    n->left  = NULL;
    n->right = NULL;
    *cur = n;
    t->count++;
    return true;
}

bool al_tree_contains(const AlTree *t, int v)
{
    if (t == NULL) { return false; }
    for (const AlTreeNode *c = t->root; c != NULL; ) {
        if (v == c->value) { return true; }
        c = (v < c->value) ? c->left : c->right;
    }
    return false;
}

static AlTreeNode *tree_delete_rec(AlTreeNode *n, int v, bool *deleted)
{
    if (n == NULL) { return NULL; }

    if (v < n->value) {
        n->left = tree_delete_rec(n->left, v, deleted);
    } else if (v > n->value) {
        n->right = tree_delete_rec(n->right, v, deleted);
    } else {
        *deleted = true;
        /* 情况 1：叶子 */
        if (n->left == NULL && n->right == NULL) { free(n); return NULL; }
        /* 情况 2：只有一个孩子 */
        if (n->left == NULL)  { AlTreeNode *r = n->right; free(n); return r; }
        if (n->right == NULL) { AlTreeNode *l = n->left;  free(n); return l; }
        /* 情况 3：两个孩子 —— 用中序后继（右子树最小值）替换 */
        AlTreeNode *succ = n->right;
        while (succ->left != NULL) { succ = succ->left; }
        n->value = succ->value;
        n->right = tree_delete_rec(n->right, succ->value, deleted);
    }
    return n;
}

bool al_tree_delete(AlTree *t, int v)
{
    if (t == NULL) { return false; }
    bool deleted = false;
    t->root = tree_delete_rec(t->root, v, &deleted);
    if (deleted) { t->count--; }
    return deleted;
}

size_t al_tree_size(const AlTree *t) { return (t != NULL) ? t->count : 0; }

static int height_rec(const AlTreeNode *n)
{
    if (n == NULL) { return 0; }
    int lh = height_rec(n->left);
    int rh = height_rec(n->right);
    return 1 + (lh > rh ? lh : rh);
}

int al_tree_height(const AlTree *t)
{
    return (t != NULL) ? height_rec(t->root) : 0;
}

static bool validate_rec(const AlTreeNode *n, int *prev, bool *first)
{
    if (n == NULL) { return true; }
    if (!validate_rec(n->left, prev, first)) { return false; }
    if (*first) { *first = false; }
    else if (n->value <= *prev) { return false; }    /* 必须严格递增 */
    *prev = n->value;
    return validate_rec(n->right, prev, first);
}

bool al_tree_is_valid(const AlTree *t)
{
    if (t == NULL) { return true; }
    int prev = 0;
    bool first = true;
    return validate_rec(t->root, &prev, &first);
}

/* ---- 遍历：函数指针回调，避免为每种遍历写一套导出代码 ---- */
typedef struct {
    int   *out;
    size_t cap;
    size_t len;
} OutBuf;

static void pre_rec(const AlTreeNode *n, OutBuf *b)
{
    if (n == NULL || b->len >= b->cap) { return; }
    b->out[b->len++] = n->value;
    pre_rec(n->left, b);
    pre_rec(n->right, b);
}

static void in_rec(const AlTreeNode *n, OutBuf *b)
{
    if (n == NULL || b->len >= b->cap) { return; }
    in_rec(n->left, b);
    if (b->len < b->cap) { b->out[b->len++] = n->value; }
    in_rec(n->right, b);
}

static void post_rec(const AlTreeNode *n, OutBuf *b)
{
    if (n == NULL || b->len >= b->cap) { return; }
    post_rec(n->left, b);
    post_rec(n->right, b);
    if (b->len < b->cap) { b->out[b->len++] = n->value; }
}

size_t al_tree_preorder(const AlTree *t, int *out, size_t cap)
{
    if (t == NULL || out == NULL || cap == 0) { return 0; }
    OutBuf b = { out, cap, 0 };
    pre_rec(t->root, &b);
    return b.len;
}

size_t al_tree_inorder(const AlTree *t, int *out, size_t cap)
{
    if (t == NULL || out == NULL || cap == 0) { return 0; }
    OutBuf b = { out, cap, 0 };
    in_rec(t->root, &b);
    return b.len;
}

size_t al_tree_postorder(const AlTree *t, int *out, size_t cap)
{
    if (t == NULL || out == NULL || cap == 0) { return 0; }
    OutBuf b = { out, cap, 0 };
    post_rec(t->root, &b);
    return b.len;
}

size_t al_tree_levelorder(const AlTree *t, int *out, size_t cap)
{
    if (t == NULL || out == NULL || cap == 0 || t->root == NULL) { return 0; }

    /* 队列容量最多是节点数 */
    const AlTreeNode **q = malloc((t->count + 1) * sizeof *q);
    if (q == NULL) { return 0; }
    size_t head = 0, tail = 0, written = 0;

    q[tail++] = t->root;
    while (head < tail && written < cap) {
        const AlTreeNode *n = q[head++];
        out[written++] = n->value;
        if (n->left  != NULL) { q[tail++] = n->left; }
        if (n->right != NULL) { q[tail++] = n->right; }
    }
    free(q);
    return written;
}
