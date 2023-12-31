/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * scheduler.c
 */

#undef _FORTIFY_SOURCE

#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include "system.h"
#include "scheduler.h"

/**
 * Needs:
 *   setjmp()
 *   longjmp()
 */

/* research the above Needed API and design accordingly */

#define SZ_STACK (2*page_size())

typedef struct thread {
    jmp_buf ctx;
    enum {
        STATUS_,
        STATUS_RUNNING,
        STATUS_SLEEPING,
        STATUS_TERMINATED
    } status;
    struct {
        void *memory_;
        void *memory;
    } stack;
    struct thread *link;
} Thread;

static struct {
    Thread *head;
    Thread *cur_thread;
    jmp_buf ctx;
} state;

Thread *thread_candidate(void) {
    Thread *running = state.cur_thread;
    Thread *start = running->link;
    do {
        if(start == NULL)
            start = state.head;
        if(start->status == STATUS_ || start->status == STATUS_SLEEPING)
            return start;
        else
            start = start->link;
    } while(start != running->link);
    return NULL;
}

void destroy(void) {
    Thread *start = state.head;
    Thread *temp;
    
    while(start != NULL) {
        temp = start->link;
        free(start->stack.memory_);
        free(start);
        start = temp;
    }
    
    free(temp);
    state.head = NULL;
    state.cur_thread = NULL;
}

int scheduler_create(scheduler_fnc_t fnc, void *arg) {
    Thread *thread = (Thread *)malloc(sizeof(Thread));
    /* Error handling for memory allocation failure */
    if (!thread) {
        fprintf(stderr, "Memory allocation failed for thread.\n");
        free(thread);
        return -1;
    }

    thread->link = state.head;
    state.head = thread;
    thread->status = STATUS_;

    thread->stack.memory_ = malloc(SZ_STACK);

    /* Error handling for stack allocation failure */
    if (!thread->stack.memory_) {
        fprintf(stderr, "Stack allocation failed for stack.\n");
        free(thread);
        return -1;
    }

    thread->stack.memory = memory_align(thread->stack.memory_, page_size());
    
    if (setjmp(thread->ctx) != 0) {
        (*fnc)(arg);
        thread->status = STATUS_TERMINATED;
    }

    return 0;
}

void schedule(void) {
    Thread *next = thread_candidate();

    if(next == NULL) {
        fprintf(stderr, "No candidate thread found.\n");
        return;
    }

    state.cur_thread = next;
    state.cur_thread->status = STATUS_RUNNING;
    longjmp(next->ctx, 1);
}

void scheduler_execute(void) {
    if(setjmp(state.ctx) == 0) {
        schedule();
        destroy();
    }
    schedule();
}

void scheduler_yield(void) {
    if(setjmp(state.cur_thread->ctx) == 0) {
        state.cur_thread->status = STATUS_SLEEPING;
        longjmp(state.ctx, 1);
    }
}