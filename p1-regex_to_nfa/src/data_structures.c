#include "data_structures.h"

#include <stdlib.h>  // malloc, realloc, free
#include <string.h>  // memcpy, memmove
#include <stdint.h> // SIZE_MAX

/* ---------- Internal helpers ---------- */

static bool ds_bad_ptr(const void *p) { return p == NULL; }

static size_t ds_max(size_t a, size_t b) { return (a > b) ? a : b; }

static ds_status ds_grow_buffer(void **data, size_t elem_size, size_t *cap, size_t min_cap) {
    if (ds_bad_ptr(data) || ds_bad_ptr(cap) || elem_size == 0) return DS_ERR_BADARG;

    size_t new_cap = (*cap == 0) ? 8 : *cap;
    while (new_cap < min_cap) {
        /* prevent overflow in extreme cases */
        if (new_cap > (SIZE_MAX / 2)) {
            new_cap = min_cap;
            break;
        }
        new_cap *= 2;
    }

    /* allocate in bytes: new_cap * elem_size */
    if (new_cap != 0 && elem_size > SIZE_MAX / new_cap) return DS_ERR_ALLOC;

    void *new_data = realloc(*data, new_cap * elem_size);
    if (!new_data) return DS_ERR_ALLOC;

    *data = new_data;
    *cap  = new_cap;
    return DS_OK;
}

/* ============================================================
 * Stack (LIFO)
 * ============================================================ */

ds_status ds_stack_init(ds_stack *s, size_t elem_size) {
    if (ds_bad_ptr(s) || elem_size == 0) return DS_ERR_BADARG;
    s->data = NULL;
    s->elem_size = elem_size;
    s->size = 0;
    s->cap  = 0;
    return DS_OK;
}

void ds_stack_free(ds_stack *s) {
    if (!s) return;
    free(s->data);
    s->data = NULL;
    s->elem_size = 0;
    s->size = 0;
    s->cap  = 0;
}

ds_status ds_stack_clear(ds_stack *s) {
    if (ds_bad_ptr(s) || s->elem_size == 0) return DS_ERR_BADARG;
    s->size = 0;
    return DS_OK;
}

bool ds_stack_empty(const ds_stack *s) {
    return (!s || s->size == 0);
}

size_t ds_stack_size(const ds_stack *s) {
    return s ? s->size : 0;
}

ds_status ds_stack_reserve(ds_stack *s, size_t min_cap) {
    if (ds_bad_ptr(s) || s->elem_size == 0) return DS_ERR_BADARG;
    if (s->cap >= min_cap) return DS_OK;
    return ds_grow_buffer(&s->data, s->elem_size, &s->cap, min_cap);
}

ds_status ds_stack_push(ds_stack *s, const void *elem) {
    if (ds_bad_ptr(s) || s->elem_size == 0 || ds_bad_ptr(elem)) return DS_ERR_BADARG;

    ds_status st = ds_stack_reserve(s, s->size + 1);
    if (st != DS_OK) return st;

    unsigned char *base = (unsigned char *)s->data;
    memcpy(base + (s->size * s->elem_size), elem, s->elem_size);
    s->size += 1;
    return DS_OK;
}

ds_status ds_stack_pop(ds_stack *s, void *out) {
    if (ds_bad_ptr(s) || s->elem_size == 0) return DS_ERR_BADARG;
    if (s->size == 0) return DS_ERR_EMPTY;

    s->size -= 1;
    if (out) {
        unsigned char *base = (unsigned char *)s->data;
        memcpy(out, base + (s->size * s->elem_size), s->elem_size);
    }
    return DS_OK;
}

ds_status ds_stack_peek(const ds_stack *s, void *out) {
    if (ds_bad_ptr(s) || s->elem_size == 0 || ds_bad_ptr(out)) return DS_ERR_BADARG;
    if (s->size == 0) return DS_ERR_EMPTY;

    const unsigned char *base = (const unsigned char *)s->data;
    memcpy(out, base + ((s->size - 1) * s->elem_size), s->elem_size);
    return DS_OK;
}

/* ============================================================
 * Queue (FIFO)
 * ============================================================ */

ds_status ds_queue_init(ds_queue *q, size_t elem_size) {
    if (ds_bad_ptr(q) || elem_size == 0) return DS_ERR_BADARG;
    q->data = NULL;
    q->elem_size = elem_size;
    q->size = 0;
    q->cap  = 0;
    q->head = 0;
    q->tail = 0;
    return DS_OK;
}

void ds_queue_free(ds_queue *q) {
    if (!q) return;
    free(q->data);
    q->data = NULL;
    q->elem_size = 0;
    q->size = 0;
    q->cap  = 0;
    q->head = 0;
    q->tail = 0;
}

ds_status ds_queue_clear(ds_queue *q) {
    if (ds_bad_ptr(q) || q->elem_size == 0) return DS_ERR_BADARG;
    q->size = 0;
    q->head = 0;
    q->tail = 0;
    return DS_OK;
}

bool ds_queue_empty(const ds_queue *q) {
    return (!q || q->size == 0);
}

size_t ds_queue_size(const ds_queue *q) {
    return q ? q->size : 0;
}

static size_t ds_queue_index(const ds_queue *q, size_t logical_i) {
    /* logical_i: 0 = front */
    return (q->head + logical_i) % q->cap;
}

static ds_status ds_queue_rebuild_linear(ds_queue *q, size_t new_cap) {
    /* Allocate new buffer and copy existing elements in FIFO order. */
    if (ds_bad_ptr(q) || q->elem_size == 0) return DS_ERR_BADARG;
    if (new_cap < q->size) return DS_ERR_BADARG;

    if (new_cap != 0 && q->elem_size > SIZE_MAX / new_cap) return DS_ERR_ALLOC;

    void *new_data = malloc(new_cap * q->elem_size);
    if (!new_data) return DS_ERR_ALLOC;

    unsigned char *dst = (unsigned char *)new_data;
    unsigned char *src = (unsigned char *)q->data;

    for (size_t i = 0; i < q->size; i++) {
        size_t idx = ds_queue_index(q, i);
        memcpy(dst + (i * q->elem_size), src + (idx * q->elem_size), q->elem_size);
    }

    free(q->data);
    q->data = new_data;
    q->cap  = new_cap;
    q->head = 0;
    q->tail = q->size;
    return DS_OK;
}

ds_status ds_queue_reserve(ds_queue *q, size_t min_cap) {
    if (ds_bad_ptr(q) || q->elem_size == 0) return DS_ERR_BADARG;
    if (q->cap >= min_cap) return DS_OK;

    size_t new_cap = (q->cap == 0) ? 8 : q->cap;
    while (new_cap < min_cap) {
        if (new_cap > (SIZE_MAX / 2)) {
            new_cap = min_cap;
            break;
        }
        new_cap *= 2;
    }

    /* If cap is 0, we can just malloc. Otherwise, rebuild to linearize. */
    if (q->cap == 0) {
        if (new_cap != 0 && q->elem_size > SIZE_MAX / new_cap) return DS_ERR_ALLOC;
        q->data = malloc(new_cap * q->elem_size);
        if (!q->data) return DS_ERR_ALLOC;
        q->cap = new_cap;
        q->head = 0;
        q->tail = 0;
        return DS_OK;
    }

    return ds_queue_rebuild_linear(q, new_cap);
}

ds_status ds_queue_enqueue(ds_queue *q, const void *elem) {
    if (ds_bad_ptr(q) || q->elem_size == 0 || ds_bad_ptr(elem)) return DS_ERR_BADARG;

    ds_status st = ds_queue_reserve(q, q->size + 1);
    if (st != DS_OK) return st;

    unsigned char *base = (unsigned char *)q->data;
    memcpy(base + (q->tail * q->elem_size), elem, q->elem_size);

    q->tail = (q->tail + 1) % q->cap;
    q->size += 1;
    return DS_OK;
}

ds_status ds_queue_dequeue(ds_queue *q, void *out) {
    if (ds_bad_ptr(q) || q->elem_size == 0) return DS_ERR_BADARG;
    if (q->size == 0) return DS_ERR_EMPTY;

    if (out) {
        unsigned char *base = (unsigned char *)q->data;
        memcpy(out, base + (q->head * q->elem_size), q->elem_size);
    }

    q->head = (q->head + 1) % q->cap;
    q->size -= 1;
    return DS_OK;
}

ds_status ds_queue_peek_front(const ds_queue *q, void *out) {
    if (ds_bad_ptr(q) || q->elem_size == 0 || ds_bad_ptr(out)) return DS_ERR_BADARG;
    if (q->size == 0) return DS_ERR_EMPTY;

    const unsigned char *base = (const unsigned char *)q->data;
    memcpy(out, base + (q->head * q->elem_size), q->elem_size);
    return DS_OK;
}

ds_status ds_queue_peek_back(const ds_queue *q, void *out) {
    if (ds_bad_ptr(q) || q->elem_size == 0 || ds_bad_ptr(out)) return DS_ERR_BADARG;
    if (q->size == 0) return DS_ERR_EMPTY;

    size_t back_idx = (q->tail == 0) ? (q->cap - 1) : (q->tail - 1);
    const unsigned char *base = (const unsigned char *)q->data;
    memcpy(out, base + (back_idx * q->elem_size), q->elem_size);
    return DS_OK;
}

char *ds_queue_to_string_newest_first(const ds_queue *q) {
    if (ds_bad_ptr(q)) return NULL;
    if (q->elem_size != sizeof(char)) return NULL;
    if (q->size > 0 && ds_bad_ptr(q->data)) return NULL;

    char *out = (char *)malloc(q->size + 1);
    if (!out) return NULL;

    const unsigned char *base = (const unsigned char *)q->data;
    for (size_t i = 0; i < q->size; i++) {
        size_t idx = ds_queue_index(q, i); /* oldest(front) -> newest(back) */
        out[i] = (char)base[idx * q->elem_size];
    }
    out[q->size] = '\0';

    return out;
}
