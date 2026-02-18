#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

/*
 * data_structures.h
 * -----------------
 * A small, dependency-free module providing:
 *  - A generic dynamic array stack (LIFO)
 *  - A generic circular buffer queue (FIFO)
 *
 * Design goals:
 *  - Works with any element type via elem_size
 *  - No macros required (but you can wrap it if you want)
 *  - Clear error handling with ds_status
 *  - C11 compatible
 *
 * Typical usage (stack of char):
 *   ds_stack st;
 *   ds_stack_init(&st, sizeof(char));
 *   char c = 'a';
 *   ds_stack_push(&st, &c);
 *   ds_stack_pop(&st, &c);
 *   ds_stack_free(&st);
 *
 * Typical usage (queue of int):
 *   ds_queue q;
 *   ds_queue_init(&q, sizeof(int));
 *   int x = 42;
 *   ds_queue_enqueue(&q, &x);
 *   ds_queue_dequeue(&q, &x);
 *   ds_queue_free(&q);
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

/* ---------- Stack (LIFO) ---------- */

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

/* ---------- Queue (FIFO) ---------- */

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

#ifdef __cplusplus
}
#endif

#endif /* DATA_STRUCTURES_H */
