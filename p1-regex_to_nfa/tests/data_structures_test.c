#include "data_structures.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef void (*test_fn)(void);

/* Simple test helper */
static void test_status(ds_status got, ds_status expected, const char *msg) {
    if (got != expected) {
        fprintf(stderr, "FAIL: %s (got=%d expected=%d)\n", msg, (int)got, (int)expected);
        assert(got == expected);
    }
}

static void run_test(test_fn fn, const char *name, int *passed, int *total) {
    (*total)++;
    fn();
    (*passed)++;
    printf("[PASS] %s\n", name);
}

static void test_stack_int_basic(void) {
    ds_stack s;
    test_status(ds_stack_init(&s, sizeof(int)), DS_OK, "stack init");

    assert(ds_stack_empty(&s));
    assert(ds_stack_size(&s) == 0);

    for (int i = 1; i <= 5; i++) {
        test_status(ds_stack_push(&s, &i), DS_OK, "stack push int");
    }
    assert(!ds_stack_empty(&s));
    assert(ds_stack_size(&s) == 5);

    int top = 0;
    test_status(ds_stack_peek(&s, &top), DS_OK, "stack peek");
    assert(top == 5);

    for (int expected = 5; expected >= 1; expected--) {
        int out = 0;
        test_status(ds_stack_pop(&s, &out), DS_OK, "stack pop");
        assert(out == expected);
    }
    assert(ds_stack_empty(&s));
    assert(ds_stack_size(&s) == 0);

    int out = 123;
    test_status(ds_stack_pop(&s, &out), DS_ERR_EMPTY, "stack pop empty");

    ds_stack_free(&s);
}

static void test_stack_char_growth(void) {
    ds_stack s;
    test_status(ds_stack_init(&s, sizeof(char)), DS_OK, "stack init char");

    /* push enough items to trigger growth */
    for (int i = 0; i < 1000; i++) {
        char c = (char)('a' + (i % 26));
        test_status(ds_stack_push(&s, &c), DS_OK, "stack push char");
    }
    assert(ds_stack_size(&s) == 1000);

    char last = '\0';
    test_status(ds_stack_peek(&s, &last), DS_OK, "stack peek char");
    assert(last == (char)('a' + (999 % 26)));

    ds_stack_free(&s);
}

static void test_queue_int_basic(void) {
    ds_queue q;
    test_status(ds_queue_init(&q, sizeof(int)), DS_OK, "queue init");

    assert(ds_queue_empty(&q));
    assert(ds_queue_size(&q) == 0);

    /* enqueue 1..5 */
    for (int i = 1; i <= 5; i++) {
        test_status(ds_queue_enqueue(&q, &i), DS_OK, "queue enqueue");
    }
    assert(ds_queue_size(&q) == 5);

    int front = 0, back = 0;
    test_status(ds_queue_peek_front(&q, &front), DS_OK, "queue peek front");
    test_status(ds_queue_peek_back(&q, &back), DS_OK, "queue peek back");
    assert(front == 1);
    assert(back == 5);

    /* dequeue in FIFO order */
    for (int expected = 1; expected <= 5; expected++) {
        int out = 0;
        test_status(ds_queue_dequeue(&q, &out), DS_OK, "queue dequeue");
        assert(out == expected);
    }

    assert(ds_queue_empty(&q));
    test_status(ds_queue_dequeue(&q, NULL), DS_ERR_EMPTY, "queue dequeue empty");

    ds_queue_free(&q);
}

static void test_queue_wraparound_and_growth(void) {
    ds_queue q;
    test_status(ds_queue_init(&q, sizeof(int)), DS_OK, "queue init");

    /* Fill and drain repeatedly to force head/tail movement */
    for (int round = 0; round < 50; round++) {
        for (int i = 0; i < 100; i++) {
            int x = round * 100 + i;
            test_status(ds_queue_enqueue(&q, &x), DS_OK, "queue enqueue lots");
        }
        assert(ds_queue_size(&q) == 100);

        for (int i = 0; i < 100; i++) {
            int out = -1;
            test_status(ds_queue_dequeue(&q, &out), DS_OK, "queue dequeue lots");
            assert(out == round * 100 + i);
        }
        assert(ds_queue_empty(&q));
    }

    ds_queue_free(&q);
}

static void test_queue_to_string_fifo_order(void) {
    ds_queue q;
    test_status(ds_queue_init(&q, sizeof(char)), DS_OK, "queue init char");

    const char initial[] = { 'a', 'b', 'c', 'd' };
    for (size_t i = 0; i < sizeof(initial); i++) {
        test_status(ds_queue_enqueue(&q, &initial[i]), DS_OK, "queue enqueue char");
    }

    char *text = ds_queue_to_string_newest_first(&q);
    assert(text != NULL);
    assert(strcmp(text, "abcd") == 0);
    free(text);

    char dropped = '\0';
    test_status(ds_queue_dequeue(&q, &dropped), DS_OK, "queue dequeue one");
    assert(dropped == 'a');

    char e = 'e';
    test_status(ds_queue_enqueue(&q, &e), DS_OK, "queue enqueue wraparound");

    text = ds_queue_to_string_newest_first(&q);
    assert(text != NULL);
    assert(strcmp(text, "bcde") == 0);
    free(text);

    test_status(ds_queue_clear(&q), DS_OK, "queue clear");
    text = ds_queue_to_string_newest_first(&q);
    assert(text != NULL);
    assert(strcmp(text, "") == 0);
    free(text);

    ds_queue_free(&q);
}

static void test_bad_args(void) {
    ds_stack s;
    test_status(ds_stack_init(&s, 0), DS_ERR_BADARG, "stack init bad elem_size");

    ds_queue q;
    test_status(ds_queue_init(&q, 0), DS_ERR_BADARG, "queue init bad elem_size");
}

static bool set_contains_int(const ds_set *set, int value) {
    return ds_set_contains(set, &value);
}

static bool set_contains_cstr(const ds_set *set, const char *value) {
    const char *needle = value;
    return ds_set_contains(set, &needle);
}

static void test_set_int_basic_and_uniqueness(void) {
    ds_set set;
    test_status(ds_set_init(&set, sizeof(int), NULL, NULL), DS_OK, "set init int");

    const int values[] = { 1, 2, 2, 3, 1, 4, 4 };
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
        test_status(ds_set_add(&set, &values[i]), DS_OK, "set add int");
    }

    assert(ds_set_size(&set) == 4);
    assert(set_contains_int(&set, 1));
    assert(set_contains_int(&set, 2));
    assert(set_contains_int(&set, 3));
    assert(set_contains_int(&set, 4));
    assert(!set_contains_int(&set, 9));

    test_status(ds_set_clear(&set), DS_OK, "set clear");
    assert(ds_set_empty(&set));

    ds_set_free(&set);
}

static void test_set_init_from_array(void) {
    const int array[] = { 9, 1, 9, 2, 3, 1, 2, 5 };

    ds_set set;
    test_status(
        ds_set_init_from_array(&set, sizeof(int), array, sizeof(array) / sizeof(array[0])),
        DS_OK,
        "set init from array"
    );

    assert(ds_set_size(&set) == 5);
    assert(set_contains_int(&set, 1));
    assert(set_contains_int(&set, 2));
    assert(set_contains_int(&set, 3));
    assert(set_contains_int(&set, 5));
    assert(set_contains_int(&set, 9));

    ds_set_free(&set);
}

static void test_set_init_from_generic_source(void) {
    const int array[] = { 10, 10, 20, 30 };
    ds_set_source source = {
        .type = DS_SET_SOURCE_ARRAY,
        .data = array,
        .elem_size = sizeof(int),
        .count = sizeof(array) / sizeof(array[0])
    };

    ds_set set;
    test_status(ds_set_init_from(&set, &source), DS_OK, "set init_from generic source");

    assert(ds_set_size(&set) == 3);
    assert(set_contains_int(&set, 10));
    assert(set_contains_int(&set, 20));
    assert(set_contains_int(&set, 30));

    ds_set_free(&set);
}

static void test_set_init_from_stack(void) {
    ds_stack stack;
    test_status(ds_stack_init(&stack, sizeof(int)), DS_OK, "stack init for set");

    const int values[] = { 7, 8, 7, 6, 8 };
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
        test_status(ds_stack_push(&stack, &values[i]), DS_OK, "stack push for set");
    }

    ds_set set;
    test_status(ds_set_init_from_stack(&set, &stack), DS_OK, "set init from stack");

    assert(ds_set_size(&set) == 3);
    assert(set_contains_int(&set, 6));
    assert(set_contains_int(&set, 7));
    assert(set_contains_int(&set, 8));

    ds_set_free(&set);
    ds_stack_free(&stack);
}

static void test_set_init_from_queue(void) {
    ds_queue queue;
    test_status(ds_queue_init(&queue, sizeof(int)), DS_OK, "queue init for set");

    const int a = 1, b = 2, c = 3, d = 4;
    test_status(ds_queue_enqueue(&queue, &a), DS_OK, "queue enqueue a");
    test_status(ds_queue_enqueue(&queue, &b), DS_OK, "queue enqueue b");
    test_status(ds_queue_enqueue(&queue, &c), DS_OK, "queue enqueue c");

    int dropped = 0;
    test_status(ds_queue_dequeue(&queue, &dropped), DS_OK, "queue dequeue to move head");
    assert(dropped == 1);

    test_status(ds_queue_enqueue(&queue, &d), DS_OK, "queue enqueue d");
    test_status(ds_queue_enqueue(&queue, &b), DS_OK, "queue enqueue duplicate b");

    ds_set set;
    test_status(ds_set_init_from_queue(&set, &queue), DS_OK, "set init from queue");

    assert(ds_set_size(&set) == 3);
    assert(set_contains_int(&set, 2));
    assert(set_contains_int(&set, 3));
    assert(set_contains_int(&set, 4));

    ds_set_free(&set);
    ds_queue_free(&queue);
}

static void test_set_init_from_string(void) {
    ds_set set;
    test_status(
        ds_set_init_from_string(&set, "a b c hola  a   b hola"),
        DS_OK,
        "set init from string"
    );

    assert(ds_set_size(&set) == 4);
    assert(set_contains_cstr(&set, "a"));
    assert(set_contains_cstr(&set, "b"));
    assert(set_contains_cstr(&set, "c"));
    assert(set_contains_cstr(&set, "hola"));
    assert(!set_contains_cstr(&set, "mundo"));

    ds_set_free(&set);

    test_status(ds_set_init_from_string(&set, ""), DS_OK, "set init from empty string");
    assert(ds_set_size(&set) == 0);
    ds_set_free(&set);
}

static void test_set_bad_args(void) {
    ds_set set;
    test_status(ds_set_init(NULL, sizeof(int), NULL, NULL), DS_ERR_BADARG, "set init null ptr");
    test_status(ds_set_init(&set, 0, NULL, NULL), DS_ERR_BADARG, "set init bad elem_size");
    test_status(ds_set_init_from(NULL, NULL), DS_ERR_BADARG, "set init_from null args");
    test_status(ds_set_init_from_string(&set, NULL), DS_ERR_BADARG, "set init_from_string null");
    test_status(ds_set_init_from_array(&set, sizeof(int), NULL, 3), DS_ERR_BADARG, "set init_from_array null data");
}

int main(void) {
    int total = 0;
    int passed = 0;

    printf("Running data_structures tests...\n");

    printf("Stack tests...\n");
    run_test(test_stack_int_basic, "test_stack_int_basic", &passed, &total);
    run_test(test_stack_char_growth, "test_stack_char_growth", &passed, &total);

    printf("Queue tests...\n");
    run_test(test_queue_int_basic, "test_queue_int_basic", &passed, &total);
    run_test(test_queue_wraparound_and_growth, "test_queue_wraparound_and_growth", &passed, &total);
    run_test(test_queue_to_string_fifo_order, "test_queue_to_string_fifo_order", &passed, &total);
    run_test(test_bad_args, "test_bad_args", &passed, &total);

    printf("Set tests...\n");
    run_test(test_set_int_basic_and_uniqueness, "test_set_int_basic_and_uniqueness", &passed, &total);
    run_test(test_set_init_from_array, "test_set_init_from_array", &passed, &total);
    run_test(test_set_init_from_generic_source, "test_set_init_from_generic_source", &passed, &total);
    run_test(test_set_init_from_stack, "test_set_init_from_stack", &passed, &total);
    run_test(test_set_init_from_queue, "test_set_init_from_queue", &passed, &total);
    run_test(test_set_init_from_string, "test_set_init_from_string", &passed, &total);
    run_test(test_set_bad_args, "test_set_bad_args", &passed, &total);

    printf("Summary: %d/%d tests passed\n", passed, total);
    return (passed == total) ? 0 : 1;
}
