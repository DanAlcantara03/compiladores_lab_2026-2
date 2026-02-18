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

int main(void) {
    int total = 0;
    int passed = 0;

    printf("Running data_structures tests...\n");

    run_test(test_stack_int_basic, "test_stack_int_basic", &passed, &total);
    run_test(test_stack_char_growth, "test_stack_char_growth", &passed, &total);
    run_test(test_queue_int_basic, "test_queue_int_basic", &passed, &total);
    run_test(test_queue_wraparound_and_growth, "test_queue_wraparound_and_growth", &passed, &total);
    run_test(test_queue_to_string_fifo_order, "test_queue_to_string_fifo_order", &passed, &total);
    run_test(test_bad_args, "test_bad_args", &passed, &total);

    printf("Summary: %d/%d tests passed\n", passed, total);
    return (passed == total) ? 0 : 1;
}
