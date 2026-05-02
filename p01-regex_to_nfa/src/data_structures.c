#include "data_structures.h"

#include <stdlib.h>  // malloc, realloc, free
#include <string.h>  // memcpy, memmove
#include <stdint.h> // SIZE_MAX

/* ---------- Internal helpers ---------- */

/**
 * @brief Returns whether a pointer is NULL.
 *
 * Small utility used across the module to keep argument checks concise.
 *
 * @param p pointer to validate.
 * @return true if `p` is NULL, false otherwise.
 */
static bool ds_bad_ptr(const void *p) { return p == NULL; }

/**
 * @brief Grows a contiguous buffer to hold at least `min_cap` elements.
 *
 * Capacity grows geometrically (x2) starting from 8, with overflow checks both for the growth loop and byte-size multiplication (`cap * elem_size`).
 * The function updates `*data` and `*cap` only on success.
 *
 * @param data address of the backing pointer to reallocate.
 * @param elem_size size of one element in bytes.
 * @param cap address of current capacity in elements.
 * @param min_cap required minimum capacity in elements.
 * @return `DS_OK` on success, `DS_ERR_BADARG` on invalid arguments,
 *         or `DS_ERR_ALLOC` on allocation/overflow failure.
 */
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

/* ---------- Stack core operations (public API) ---------- */


/**
 * @brief Initializes stack metadata without allocating storage.
 *
 * Implementation detail: `data` starts as NULL and capacity as 0; memory is acquired lazily by `ds_stack_reserve`/`ds_stack_push`.
 */
ds_status ds_stack_init(ds_stack *s, size_t elem_size) {
    if (ds_bad_ptr(s) || elem_size == 0) return DS_ERR_BADARG;
    s->data = NULL;
    s->elem_size = elem_size;
    s->size = 0;
    s->cap  = 0;
    return DS_OK;
}

/**
 * @brief Releases stack storage and resets the structure to an empty state.
 *
 * Implementation detail: fields are zeroed after `free` so repeated calls remain safe.
 */
void ds_stack_free(ds_stack *s) {
    if (!s) return;
    free(s->data);
    s->data = NULL;
    s->elem_size = 0;
    s->size = 0;
    s->cap  = 0;
}

/**
 * @brief Clears logical content by setting size to zero.
 *
 * Implementation detail: keeps previously allocated capacity for reuse.
 */
ds_status ds_stack_clear(ds_stack *s) {
    if (ds_bad_ptr(s) || s->elem_size == 0) return DS_ERR_BADARG;
    s->size = 0;
    return DS_OK;
}

/**
 * @brief Checks whether the stack has no elements.
 *
 * Implementation detail: treats NULL as empty.
 */
bool ds_stack_empty(const ds_stack *s) {
    return (!s || s->size == 0);
}

/**
 * @brief Returns the current element count.
 *
 * Implementation detail: returns 0 for NULL input.
 */
size_t ds_stack_size(const ds_stack *s) {
    return s ? s->size : 0;
}

/**
 * @brief Ensures stack capacity can hold at least `min_cap` elements.
 *
 * Implementation detail: delegates growth policy and overflow handling to `ds_grow_buffer`.
 */
ds_status ds_stack_reserve(ds_stack *s, size_t min_cap) {
    if (ds_bad_ptr(s) || s->elem_size == 0) return DS_ERR_BADARG;
    if (s->cap >= min_cap) return DS_OK;
    return ds_grow_buffer(&s->data, s->elem_size, &s->cap, min_cap);
}

/**
 * @brief Appends one element to the top of the stack.
 *
 * Implementation detail: reserves `size + 1`, then copies bytes with `memcpy` into the next slot.
 */
ds_status ds_stack_push(ds_stack *s, const void *elem) {
    if (ds_bad_ptr(s) || s->elem_size == 0 || ds_bad_ptr(elem)) return DS_ERR_BADARG;

    ds_status st = ds_stack_reserve(s, s->size + 1);
    if (st != DS_OK) return st;

    unsigned char *base = (unsigned char *)s->data;
    memcpy(base + (s->size * s->elem_size), elem, s->elem_size);
    s->size += 1;
    return DS_OK;
}

/**
 * @brief Removes the top element, optionally copying it out.
 *
 * Implementation detail: decrements `size` first, then reads from the previous top position when `out != NULL`.
 */
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

/**
 * @brief Reads the current top element without removing it.
 *
 * Implementation detail: computes top offset as `(size - 1) * elem_size` and copies bytes to `out`.
 */
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

/* ---------- Queue helpers (internal) ---------- */

/**
 * @brief Maps a logical FIFO index to the physical circular-buffer index.
 *
 * Logical index 0 corresponds to the current front (`head`).
 *
 * @param q queue instance.
 * @param logical_i FIFO-relative index in `[0, q->size)`.
 * @return physical index in the circular backing array.
 */
static size_t ds_queue_index(const ds_queue *q, size_t logical_i) {
    /* logical_i: 0 = front */
    return (q->head + logical_i) % q->cap;
}

/**
 * @brief Rebuilds queue storage into a linear layout with `head = 0`.
 *
 * Allocates a new backing array of `new_cap` elements, copies current
 * elements in FIFO order, then swaps buffers. The resulting queue preserves
 * logical order and sets `tail = size`.
 *
 * @param q queue to rebuild.
 * @param new_cap target capacity in elements (must be >= current size).
 * @return `DS_OK` on success, `DS_ERR_BADARG` on invalid input,
 *         or `DS_ERR_ALLOC` on allocation/overflow failure.
 */
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

/* ---------- Queue core operations (public API) ---------- */

/**
 * @brief Initializes queue metadata without allocating storage.
 *
 * Implementation detail: starts as an empty circular buffer with
 * `head = tail = 0` and capacity 0.
 */
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

/**
 * @brief Releases queue storage and resets all fields.
 *
 * Implementation detail: after `free`, indices and sizes are set to zero
 * so multiple frees are safe.
 */
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

/**
 * @brief Clears logical queue content.
 *
 * Implementation detail: keeps allocated capacity and resets both indices
 * (`head`, `tail`) to 0.
 */
ds_status ds_queue_clear(ds_queue *q) {
    if (ds_bad_ptr(q) || q->elem_size == 0) return DS_ERR_BADARG;
    q->size = 0;
    q->head = 0;
    q->tail = 0;
    return DS_OK;
}

/**
 * @brief Checks whether the queue is empty.
 *
 * Implementation detail: treats NULL as empty.
 */
bool ds_queue_empty(const ds_queue *q) {
    return (!q || q->size == 0);
}

/**
 * @brief Returns the number of stored elements.
 *
 * Implementation detail: returns 0 for NULL input.
 */
size_t ds_queue_size(const ds_queue *q) {
    return q ? q->size : 0;
}

/**
 * @brief Ensures queue capacity can hold at least `min_cap` elements.
 *
 * Implementation detail: grows geometrically and either allocates directly
 * (empty queue) or rebuilds to a linearized layout.
 */
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

/**
 * @brief Inserts one element at queue back.
 *
 * Implementation detail: reserves `size + 1`, writes at `tail`, then advances `tail` modulo capacity.
 */
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

/**
 * @brief Removes one element from queue front, optionally copying it out.
 *
 * Implementation detail: reads from `head` when `out != NULL`, then advances `head` modulo capacity and decrements `size`.
 */
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

/**
 * @brief Reads the current front element without removing it.
 *
 * Implementation detail: copies bytes from the slot pointed by `head`.
 */
ds_status ds_queue_peek_front(const ds_queue *q, void *out) {
    if (ds_bad_ptr(q) || q->elem_size == 0 || ds_bad_ptr(out)) return DS_ERR_BADARG;
    if (q->size == 0) return DS_ERR_EMPTY;

    const unsigned char *base = (const unsigned char *)q->data;
    memcpy(out, base + (q->head * q->elem_size), q->elem_size);
    return DS_OK;
}

/**
 * @brief Reads the current back element without removing it.
 *
 * Implementation detail: computes back index as `tail - 1` with wrap-around.
 */
ds_status ds_queue_peek_back(const ds_queue *q, void *out) {
    if (ds_bad_ptr(q) || q->elem_size == 0 || ds_bad_ptr(out)) return DS_ERR_BADARG;
    if (q->size == 0) return DS_ERR_EMPTY;

    size_t back_idx = (q->tail == 0) ? (q->cap - 1) : (q->tail - 1);
    const unsigned char *base = (const unsigned char *)q->data;
    memcpy(out, base + (back_idx * q->elem_size), q->elem_size);
    return DS_OK;
}

/**
 * @brief Builds a newly allocated C-string from queue contents.
 *
 * Implementation detail: intended for `char` queues; iterates in FIFO order using `ds_queue_index` and appends a trailing null terminator.
 */
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

/* ============================================================
 * Set (unique elements)
 * ============================================================ */

/* ---------- Set helpers (internal) ---------- */

/**
 * @brief Compares two elements using byte-wise equality.
 *
 * This is the set's default strategy when no custom equality function is provided.
 *
 * @param lhs pointer to the first element.
 * @param rhs pointer to the second element.
 * @param elem_size size of each element in bytes.
 * @return true if both memory blocks are identical.
 */
static bool ds_set_default_equals(const void *lhs, const void *rhs, size_t elem_size) {
    return memcmp(lhs, rhs, elem_size) == 0;
}

/**
 * @brief Returns the effective equality function for a set.
 *
 * If the set defines `equals_fn`, it is used; otherwise `ds_set_default_equals` is returned.
 *
 * @param set set from which the comparator is obtained.
 * @return always-valid equality function (never NULL).
 */
static ds_set_equals_fn ds_set_get_equals(const ds_set *set) {
    return (set->equals_fn != NULL) ? set->equals_fn : ds_set_default_equals;
}

/**
 * @brief Duplicates a string into dynamically allocated memory.
 *
 * Allocates an independent copy of `src` and transfers ownership to the caller.
 *
 * @param src source null-terminated string.
 * @return pointer to the allocated copy; `NULL` if `src` is `NULL`
 *         or allocation fails.
 */
static char *ds_strdup_owned(const char *src) {
    if (src == NULL) return NULL;
    size_t len = strlen(src);
    char *dst = (char *)malloc(len + 1);
    if (!dst) return NULL;
    memcpy(dst, src, len + 1);
    return dst;
}

/**
 * @brief Finds an element in the set and returns its index.
 *
 * The search is linear (`O(n)`) and uses the set's effective equality function (custom or default).
 *
 * @param set set to search.
 * @param elem target element.
 * @return index of the matching element; `SIZE_MAX` if not found
 *         or if arguments/set state are invalid.
 */
static size_t ds_set_find_index(const ds_set *set, const void *elem) {
    if (ds_bad_ptr(set) || ds_bad_ptr(elem) || set->elem_size == 0 || set->size == 0) {
        return SIZE_MAX;
    }

    ds_set_equals_fn equals_fn = ds_set_get_equals(set);
    const unsigned char *base = (const unsigned char *)set->data;

    for (size_t i = 0; i < set->size; i++) {
        const void *stored = base + (i * set->elem_size);
        if (equals_fn(elem, stored, set->elem_size)) {
            return i;
        }
    }

    return SIZE_MAX;
}

/* ---------- Set source builders (forward declarations, internal) ---------- */

static ds_status ds_set_init_from_array_impl(ds_set *set, size_t elem_size, const void *array, size_t count);
static ds_status ds_set_init_from_stack_impl(ds_set *set, const ds_stack *stack);
static ds_status ds_set_init_from_queue_impl(ds_set *set, const ds_queue *queue);
static ds_status ds_set_init_from_string_impl(ds_set *set, const char *items);

/* ---------- Set source dispatch (public API) ---------- */

/**
 * @brief Initializes a set from a tagged generic source descriptor.
 *
 * Implementation detail: dispatches by `source->type` to source-specific
 * internal builders.
 */
ds_status ds_set_init_from(ds_set *set, const ds_set_source *source) {
    if (ds_bad_ptr(set) || ds_bad_ptr(source)) return DS_ERR_BADARG;

    switch (source->type) {
        case DS_SET_SOURCE_ARRAY:
            return ds_set_init_from_array_impl(set, source->elem_size, source->data, source->count);
        case DS_SET_SOURCE_STACK:
            return ds_set_init_from_stack_impl(set, (const ds_stack *)source->data);
        case DS_SET_SOURCE_QUEUE:
            return ds_set_init_from_queue_impl(set, (const ds_queue *)source->data);
        case DS_SET_SOURCE_STRING:
            return ds_set_init_from_string_impl(set, (const char *)source->data);
        default:
            return DS_ERR_BADARG;
    }
}

/* ---------- Set core operations (public API) ---------- */

/**
 * @brief Initializes a set instance with optional callbacks.
 *
 * Implementation detail: only initializes metadata; backing storage remains
 * NULL until reserve/add operations.
 */
ds_status ds_set_init(ds_set *set, size_t elem_size, ds_set_equals_fn equals_fn, ds_set_destroy_fn destroy_fn) {
    if (ds_bad_ptr(set) || elem_size == 0) return DS_ERR_BADARG;

    set->data = NULL;
    set->elem_size = elem_size;
    set->size = 0;
    set->cap = 0;
    set->equals_fn = equals_fn;
    set->destroy_fn = destroy_fn;
    return DS_OK;
}

/**
 * @brief Releases all resources owned by the set and resets fields.
 *
 * Implementation detail: invokes `destroy_fn` for each stored element before
 * freeing the backing array.
 */
void ds_set_free(ds_set *set) {
    if (!set) return;

    if (set->destroy_fn && set->data && set->elem_size > 0) {
        unsigned char *base = (unsigned char *)set->data;
        for (size_t i = 0; i < set->size; i++) {
            set->destroy_fn(base + (i * set->elem_size));
        }
    }

    free(set->data);
    set->data = NULL;
    set->elem_size = 0;
    set->size = 0;
    set->cap = 0;
    set->equals_fn = NULL;
    set->destroy_fn = NULL;
}

/**
 * @brief Removes all elements while preserving allocated capacity.
 *
 * Implementation detail: runs `destroy_fn` over current elements, then sets
 * `size = 0`.
 */
ds_status ds_set_clear(ds_set *set) {
    if (ds_bad_ptr(set) || set->elem_size == 0) return DS_ERR_BADARG;

    if (set->destroy_fn && set->data) {
        unsigned char *base = (unsigned char *)set->data;
        for (size_t i = 0; i < set->size; i++) {
            set->destroy_fn(base + (i * set->elem_size));
        }
    }

    set->size = 0;
    return DS_OK;
}

/**
 * @brief Checks whether the set is empty.
 *
 * Implementation detail: treats NULL as empty.
 */
bool ds_set_empty(const ds_set *set) {
    return (!set || set->size == 0);
}

/**
 * @brief Returns the number of stored unique elements.
 *
 * Implementation detail: returns 0 for NULL input.
 */
size_t ds_set_size(const ds_set *set) {
    return set ? set->size : 0;
}

/**
 * @brief Ensures set capacity can hold at least `min_cap` elements.
 *
 * Implementation detail: delegates growth policy and overflow checks to
 * `ds_grow_buffer`.
 */
ds_status ds_set_reserve(ds_set *set, size_t min_cap) {
    if (ds_bad_ptr(set) || set->elem_size == 0) return DS_ERR_BADARG;
    if (set->cap >= min_cap) return DS_OK;
    return ds_grow_buffer(&set->data, set->elem_size, &set->cap, min_cap);
}

/**
 * @brief Checks membership of one element.
 *
 * Implementation detail: uses linear lookup via `ds_set_find_index`.
 */
bool ds_set_contains(const ds_set *set, const void *elem) {
    return ds_set_find_index(set, elem) != SIZE_MAX;
}

/**
 * @brief Inserts an element if not already present.
 *
 * Implementation detail: membership is checked first; on miss, storage is
 * grown and bytes are copied into the next slot.
 */
ds_status ds_set_add(ds_set *set, const void *elem) {
    if (ds_bad_ptr(set) || set->elem_size == 0 || ds_bad_ptr(elem)) return DS_ERR_BADARG;

    if (ds_set_contains(set, elem)) return DS_OK;

    ds_status st = ds_set_reserve(set, set->size + 1);
    if (st != DS_OK) return st;

    unsigned char *base = (unsigned char *)set->data;
    memcpy(base + (set->size * set->elem_size), elem, set->elem_size);
    set->size += 1;
    return DS_OK;
}

/* ---------- Set source wrappers (public API) ---------- */

/* --- From arrays --- */
/**
 * @brief Convenience wrapper to initialize from a raw array.
 *
 * Implementation detail: builds a `ds_set_source` descriptor and forwards to
 * `ds_set_init_from`.
 */
ds_status ds_set_init_from_array(ds_set *set, size_t elem_size, const void *array, size_t count) {
    ds_set_source src = {
        .type = DS_SET_SOURCE_ARRAY,
        .data = array,
        .elem_size = elem_size,
        .count = count
    };
    return ds_set_init_from(set, &src);
}

/* --- From stack --- */

/**
 * @brief Convenience wrapper to initialize from a stack.
 *
 * Implementation detail: builds a `ds_set_source` descriptor and forwards to
 * `ds_set_init_from`.
 */
ds_status ds_set_init_from_stack(ds_set *set, const ds_stack *stack) {
    ds_set_source src = {
        .type = DS_SET_SOURCE_STACK,
        .data = stack,
        .elem_size = 0,
        .count = 0
    };
    return ds_set_init_from(set, &src);
}

/* --- From queue --- */

/**
 * @brief Convenience wrapper to initialize from a queue.
 *
 * Implementation detail: builds a `ds_set_source` descriptor and forwards to
 * `ds_set_init_from`.
 */
ds_status ds_set_init_from_queue(ds_set *set, const ds_queue *queue) {
    ds_set_source src = {
        .type = DS_SET_SOURCE_QUEUE,
        .data = queue,
        .elem_size = 0,
        .count = 0
    };
    return ds_set_init_from(set, &src);
}

/* --- From strings --- */

/**
 * @brief Convenience wrapper to initialize a set from tokenized text.
 *
 * Implementation detail: builds a `ds_set_source` descriptor and forwards to
 * `ds_set_init_from`.
 */
ds_status ds_set_init_from_string(ds_set *set, const char *items) {
    ds_set_source src = {
        .type = DS_SET_SOURCE_STRING,
        .data = items,
        .elem_size = 0,
        .count = 0
    };
    return ds_set_init_from(set, &src);
}

/* ---------- Set source builders (internal helpers) ---------- */

/* --- From arrays --- */
/**
 * @brief Builds a set from a contiguous raw array.
 *
 * Creates a plain set (`memcmp` equality, no destructor) and inserts each
 * element from `array` in order, naturally skipping duplicates through
 * `ds_set_add`.
 *
 * @param set destination set.
 * @param elem_size size of each array element in bytes.
 * @param array pointer to contiguous source elements.
 * @param count number of elements in `array`.
 * @return `DS_OK` on success, error status otherwise.
 */
static ds_status ds_set_init_from_array_impl(ds_set *set, size_t elem_size, const void *array, size_t count) {
    if (elem_size == 0) return DS_ERR_BADARG;
    if (count > 0 && ds_bad_ptr(array)) return DS_ERR_BADARG;

    ds_status st = ds_set_init(set, elem_size, NULL, NULL);
    if (st != DS_OK) return st;

    const unsigned char *base = (const unsigned char *)array;
    for (size_t i = 0; i < count; i++) {
        st = ds_set_add(set, base + (i * elem_size));
        if (st != DS_OK) {
            ds_set_free(set);
            return st;
        }
    }

    return DS_OK;
}

/* --- From stack --- */

/**
 * @brief Builds a set from stack storage.
 *
 * Creates a plain set and iterates the stack backing array from index 0 to
 * `size - 1`, inserting each element through `ds_set_add`.
 *
 * @param set destination set.
 * @param stack source stack.
 * @return `DS_OK` on success, error status otherwise.
 */
static ds_status ds_set_init_from_stack_impl(ds_set *set, const ds_stack *stack) {
    if (ds_bad_ptr(stack) || stack->elem_size == 0) return DS_ERR_BADARG;
    if (stack->size > 0 && ds_bad_ptr(stack->data)) return DS_ERR_BADARG;

    ds_status st = ds_set_init(set, stack->elem_size, NULL, NULL);
    if (st != DS_OK) return st;

    const unsigned char *base = (const unsigned char *)stack->data;
    for (size_t i = 0; i < stack->size; i++) {
        st = ds_set_add(set, base + (i * stack->elem_size));
        if (st != DS_OK) {
            ds_set_free(set);
            return st;
        }
    }

    return DS_OK;
}

/* --- From queue --- */

/**
 * @brief Builds a set from queue elements in FIFO order.
 *
 * Creates a plain set and walks queue elements via `ds_queue_index`, adding
 * each value through `ds_set_add`.
 *
 * @param set destination set.
 * @param queue source queue.
 * @return `DS_OK` on success, error status otherwise.
 */
static ds_status ds_set_init_from_queue_impl(ds_set *set, const ds_queue *queue) {
    if (ds_bad_ptr(queue) || queue->elem_size == 0) return DS_ERR_BADARG;
    if (queue->size > 0 && (ds_bad_ptr(queue->data) || queue->cap == 0)) return DS_ERR_BADARG;

    ds_status st = ds_set_init(set, queue->elem_size, NULL, NULL);
    if (st != DS_OK) return st;

    const unsigned char *base = (const unsigned char *)queue->data;
    for (size_t i = 0; i < queue->size; i++) {
        size_t idx = ds_queue_index(queue, i);
        st = ds_set_add(set, base + (idx * queue->elem_size));
        if (st != DS_OK) {
            ds_set_free(set);
            return st;
        }
    }

    return DS_OK;
}

/* --- From strings --- */

/**
 * @brief Compares two stored `char *` slots by string content.
 *
 * The function expects pointers to slots (`char **`) because set elements are
 * stored by value as pointer-sized entries. NULL strings compare equal only
 * when both sides are NULL.
 *
 * @param lhs pointer to left slot (`const char *const *`).
 * @param rhs pointer to right slot (`const char *const *`).
 * @param elem_size unused for this comparator.
 * @return true when both slots represent the same string value.
 */
static bool ds_set_cstr_equals(const void *lhs, const void *rhs, size_t elem_size) {
    (void)elem_size;

    if (ds_bad_ptr(lhs) || ds_bad_ptr(rhs)) return false;

    const char *const *a = (const char *const *)lhs;
    const char *const *b = (const char *const *)rhs;

    if (*a == NULL || *b == NULL) return (*a == *b);
    return strcmp(*a, *b) == 0;
}

/**
 * @brief Destroys one stored `char *` slot.
 *
 * Frees the pointed string and then nulls the slot to avoid dangling pointers
 * during cleanup passes.
 *
 * @param elem pointer to slot (`char **`) stored inside the set.
 */
static void ds_set_cstr_destroy(void *elem) {
    if (ds_bad_ptr(elem)) return;
    char **slot = (char **)elem;
    free(*slot);
    *slot = NULL;
}

/**
 * @brief Builds a set of owned `char *` tokens from a text input.
 *
 * Initializes a set configured for string-pointer elements using the string
 * comparator/destructor helpers. Input is tokenized by whitespace; each token
 * is duplicated and inserted only if not already present.
 *
 * @param set destination set.
 * @param items source null-terminated string with whitespace-separated tokens.
 * @return `DS_OK` on success, error status otherwise.
 */
static ds_status ds_set_init_from_string_impl(ds_set *set, const char *items) {
    if (ds_bad_ptr(items)) return DS_ERR_BADARG;

    ds_status st = ds_set_init(set, sizeof(char *), ds_set_cstr_equals, ds_set_cstr_destroy);
    if (st != DS_OK) return st;

    char *buffer = ds_strdup_owned(items);
    if (!buffer) {
        ds_set_free(set);
        return DS_ERR_ALLOC;
    }

    char *token = strtok(buffer, " \t\r\n");
    while (token != NULL) {
        char *owned = ds_strdup_owned(token);
        if (!owned) {
            free(buffer);
            ds_set_free(set);
            return DS_ERR_ALLOC;
        }

        if (ds_set_contains(set, &owned)) {
            free(owned);
        } else {
            st = ds_set_add(set, &owned);
            if (st != DS_OK) {
                free(owned);
                free(buffer);
                ds_set_free(set);
                return st;
            }
        }

        token = strtok(NULL, " \t\r\n");
    }

    free(buffer);
    return DS_OK;
}
