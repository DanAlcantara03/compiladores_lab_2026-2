#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

/*
 * data_structures.h
 * -----------------
 * A small, dependency-free module providing:
 *  - A generic dynamic array stack (LIFO)
 *  - A generic circular buffer queue (FIFO)
 *  - A generic set (no repeated elements)
 *
 * Design goals:
 *  - Works with any element type via elem_size
 *  - No macros required (but you can wrap it if you want)
 *  - Clear error handling with ds_status
 *  - C11 compatible
*/

#include <stddef.h>  // size_t
#include <stdbool.h> // bool

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Status codes ---------- */

typedef enum ds_status {
    DS_OK = 0,
    DS_ERR_ALLOC,
    DS_ERR_EMPTY,
    DS_ERR_BADARG
} ds_status;

/* ============================================================
 * Stack (LIFO)
 * ============================================================ */

/*
 * Usage example (stack of char):
 *   ds_stack st;
 *   ds_stack_init(&st, sizeof(char));
 *   char c = 'a';
 *   ds_stack_push(&st, &c);
 *   ds_stack_pop(&st, &c);
 *   ds_stack_free(&st);
 */


typedef struct ds_stack {
    void   *data;      /* backing storage (dynamic array) */
    size_t  elem_size; /* size of each element in bytes */
    size_t  size;      /* number of elements currently stored */
    size_t  cap;       /* capacity in elements */
} ds_stack;

/**
 * @brief Initializes a stack for elements of size elem_size bytes.
 * @param s Pointer to stack
 * @param elem_size Size of one element in bytes (must be > 0)
 */
ds_status ds_stack_init(ds_stack *s, size_t elem_size);

/**
 * @brief Frees internal memory; safe to call multiple times.
 */
void ds_stack_free(ds_stack *s);

/**
 * @brief Removes all elements but keeps allocated capacity.
 */
ds_status ds_stack_clear(ds_stack *s);

/**
 * @brief Returns true if stack is empty.
 */
bool ds_stack_empty(const ds_stack *s);

/**
 * @brief Returns number of elements stored.
 */
size_t ds_stack_size(const ds_stack *s);

/**
 * @brief Pushes a new element onto the stack (copies elem_size bytes from elem).
 */
ds_status ds_stack_push(ds_stack *s, const void *elem);

/**
 * @brief Pops the top element. Copies it into out (may be NULL to discard).
 */
ds_status ds_stack_pop(ds_stack *s, void *out);

/**
 * @brief Reads the top element without removing it. Copies it into out.
 */
ds_status ds_stack_peek(const ds_stack *s, void *out);

/**
 * @brief Ensures capacity is at least min_cap elements (may reallocate).
 */
ds_status ds_stack_reserve(ds_stack *s, size_t min_cap);

/* ============================================================
 * Queue (FIFO)
 * ============================================================ */

/*
 * Usage example (queue of int):
 *   ds_queue q;
 *   ds_queue_init(&q, sizeof(int));
 *   int x = 42;
 *   ds_queue_enqueue(&q, &x);
 *   ds_queue_dequeue(&q, &x);
 *   ds_queue_free(&q);
 */

typedef struct ds_queue {
    void   *data;       /* backing storage (dynamic circular buffer) */
    size_t  elem_size;  /* size of each element in bytes */
    size_t  size;       /* number of elements currently stored */
    size_t  cap;        /* capacity in elements */
    size_t  head;       /* index of front element */
    size_t  tail;       /* index of next insertion position */
} ds_queue;


/**
 * @brief Initializes a queue for elements of size elem_size bytes.
 */
ds_status ds_queue_init(ds_queue *q, size_t elem_size);

/**
 * @brief Frees internal memory; safe to call multiple times.
 */
void ds_queue_free(ds_queue *q);

/**
 * @brief Removes all elements but keeps allocated capacity.
 */
ds_status ds_queue_clear(ds_queue *q);

/**
 * @brief Returns true if queue is empty.
 */
bool ds_queue_empty(const ds_queue *q);

/**
 * @brief Returns number of elements stored.
 */
size_t ds_queue_size(const ds_queue *q);

/**
 * @brief Enqueues an element at the back (copies elem_size bytes from elem).
 */
ds_status ds_queue_enqueue(ds_queue *q, const void *elem);

/**
 * @brief Dequeues the front element. Copies it into out (may be NULL to discard).
 */
ds_status ds_queue_dequeue(ds_queue *q, void *out);

/**
 * @brief Peeks at the front element without removing it.
 */
ds_status ds_queue_peek_front(const ds_queue *q, void *out);

/**
 * @brief Peeks at the back element (the most recently enqueued) without removing it.
 */
ds_status ds_queue_peek_back(const ds_queue *q, void *out);

/**
 * @brief Ensures capacity is at least min_cap elements (may reallocate).
 */
ds_status ds_queue_reserve(ds_queue *q, size_t min_cap);

/**
 * @brief Builds a C string with queue contents in FIFO order (front to back).
 *
 * Notes:
 * - This helper is intended for queues of `char` (`elem_size == sizeof(char)`).
 * - Output order is from oldest enqueued to most recently enqueued.
 * - Returns a newly allocated null-terminated string that the caller must free.
 * - Returns NULL on bad arguments, allocation failure, or non-char queues.
 */
char *ds_queue_to_string_newest_first(const ds_queue *q);

/* ============================================================
 * Set (unique elements)
 * ============================================================ */

/*
 * Usage examples:
 *
 * 1) From array (`ds_set_init_from_array`):
 *   int values[] = {1, 2, 2, 3};
 *   ds_set set_from_array;
 *   ds_set_init_from_array(&set_from_array, sizeof(int), values, 4);
 *   ds_set_free(&set_from_array);
 *
 * 2) From stack (`ds_set_init_from_stack`):
 *   ds_stack st;
 *   ds_stack_init(&st, sizeof(int));
 *   int a = 10, b = 20;
 *   ds_stack_push(&st, &a);
 *   ds_stack_push(&st, &b);
 *   ds_set set_from_stack;
 *   ds_set_init_from_stack(&set_from_stack, &st);
 *   ds_set_free(&set_from_stack);
 *   ds_stack_free(&st);
 *
 * 3) From queue (`ds_set_init_from_queue`):
 *   ds_queue q;
 *   ds_queue_init(&q, sizeof(int));
 *   int x = 7, y = 7, z = 9;
 *   ds_queue_enqueue(&q, &x);
 *   ds_queue_enqueue(&q, &y);
 *   ds_queue_enqueue(&q, &z);
 *   ds_set set_from_queue;
 *   ds_set_init_from_queue(&set_from_queue, &q);
 *   ds_set_free(&set_from_queue);
 *   ds_queue_free(&q);
 *
 * 4) From string (`ds_set_init_from_string`):
 *   ds_set set_from_string;
 *   ds_set_init_from_string(&set_from_string, "red blue red green");
 *   ds_set_free(&set_from_string);
 */

/**
 * @brief Callback used to decide whether two set elements are equal.
 *
 * Used to prevent duplicates in operations such as contains/add checks.
 * If `NULL` is passed to `ds_set_init`, the set falls back to byte-wise
 * comparison (`memcmp`) over `elem_size`.
 *
 * @param lhs pointer to the candidate element.
 * @param rhs pointer to the stored element.
 * @param elem_size size of each element in bytes.
 * @return `true` if both values are logically equal; `false` otherwise.
 */
typedef bool (*ds_set_equals_fn)(const void *lhs, const void *rhs, size_t elem_size);

/**
 * @brief Optional callback used to release internal resources of one element.
 *
 * The set invokes it when destroying stored elements (currently in
 * `ds_set_clear` and `ds_set_free`). This is useful when each element owns
 * dynamic memory (e.g., pointers to `char *`).
 *
 * @param elem pointer to the element stored inside the set.
 */
typedef void (*ds_set_destroy_fn)(void *elem);

typedef struct ds_set {
    void            *data;       /* backing storage (dynamic array) */
    size_t           elem_size;  /* size of each element in bytes */
    size_t           size;       /* number of unique elements stored */
    size_t           cap;        /* capacity in elements */
    ds_set_equals_fn equals_fn;  /* element equality callback (NULL => byte-wise compare) */
    ds_set_destroy_fn destroy_fn;/* optional destructor for each stored element */
} ds_set;

typedef enum ds_set_source_type {
    DS_SET_SOURCE_ARRAY = 0,
    DS_SET_SOURCE_STACK,
    DS_SET_SOURCE_QUEUE,
    DS_SET_SOURCE_STRING
} ds_set_source_type;

typedef struct ds_set_source {
    ds_set_source_type type;
    const void        *data;
    size_t             elem_size; /* required for ARRAY; ignored otherwise */
    size_t             count;     /* required for ARRAY; ignored otherwise */
} ds_set_source;

/**
 * @brief Initializes a set for elements of size elem_size bytes.
 *
 * @param set Pointer to set.
 * @param elem_size Size of one element in bytes (must be > 0).
 * @param equals_fn Optional equality callback. If NULL, byte-wise compare is used.
 * @param destroy_fn Optional destructor called for each stored element on clear/free.
 */
ds_status ds_set_init(ds_set *set, size_t elem_size, ds_set_equals_fn equals_fn, ds_set_destroy_fn destroy_fn);

/**
 * @brief Frees internal memory; safe to call multiple times.
 */
void ds_set_free(ds_set *set);

/**
 * @brief Removes all elements but keeps allocated capacity.
 */
ds_status ds_set_clear(ds_set *set);

/**
 * @brief Returns true if set is empty.
 */
bool ds_set_empty(const ds_set *set);

/**
 * @brief Returns number of unique elements stored.
 */
size_t ds_set_size(const ds_set *set);

/**
 * @brief Ensures capacity is at least min_cap elements (may reallocate).
 */
ds_status ds_set_reserve(ds_set *set, size_t min_cap);

/**
 * @brief Returns true if elem already exists in set.
 */
bool ds_set_contains(const ds_set *set, const void *elem);

/**
 * @brief Adds elem if it does not exist yet. Duplicate inserts are ignored.
 */
ds_status ds_set_add(ds_set *set, const void *elem);

/**
 * @brief Initializes a set from one source: array, stack, queue or string.
 *
 * Source contract:
 * - ARRAY:  source.data points to a contiguous array of `count` elements
 *           where each element has `elem_size` bytes.
 * - STACK:  source.data points to `const ds_stack *`.
 * - QUEUE:  source.data points to `const ds_queue *`.
 * - STRING: source.data points to `const char *` with space-separated tokens.
 *           Set stores owned `char *` tokens without duplicates.
 */
ds_status ds_set_init_from(ds_set *set, const ds_set_source *source);

/**
 * @brief Convenience wrapper for DS_SET_SOURCE_ARRAY.
 */
ds_status ds_set_init_from_array(ds_set *set, size_t elem_size, const void *array, size_t count);

/**
 * @brief Convenience wrapper for DS_SET_SOURCE_STACK.
 */
ds_status ds_set_init_from_stack(ds_set *set, const ds_stack *stack);

/**
 * @brief Convenience wrapper for DS_SET_SOURCE_QUEUE.
 */
ds_status ds_set_init_from_queue(ds_set *set, const ds_queue *queue);

/**
 * @brief Convenience wrapper for DS_SET_SOURCE_STRING.
 */
ds_status ds_set_init_from_string(ds_set *set, const char *items);

#ifdef __cplusplus
}
#endif

#endif /* DATA_STRUCTURES_H */
