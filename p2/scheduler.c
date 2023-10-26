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

#define SZ_STACK 4096

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
    scheduler_fnc_t fnc;
    const char *name;
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

static void destroy(void) {
    Thread *start = state.head;
    Thread *temp;
    
    while(start != NULL) {
        temp = start->link;
        
        memset(start->stack.memory_, 0, sizeof(start->stack.memory_));
        memset(start->stack.memory, 0, sizeof(start->stack.memory));
        free(start->stack.memory_);
        
        memset(start, 0, sizeof(start));
        free(start);
        start = temp;
    }

    state.head = NULL;
    state.cur_thread = NULL;
}

int scheduler_create(scheduler_fnc_t fnc, void *arg) {
    size_t page_size_ = page_size();
    int num_pages;

    Thread *thread = (Thread *)malloc(sizeof(Thread));
    /* Error handling for memory allocation failure */
    if (!thread) {
        fprintf(stderr, "Memory allocation failed for thread.\n");
        return -1;
    }

    thread->name = (const char *)arg;
    thread->status = STATUS_;
    
    thread->fnc = fnc;
    thread->stack.memory_ = malloc(SZ_STACK);
    /* Error handling for stack allocation failure */
    if (!thread->stack.memory_) {
        fprintf(stderr, "Stack allocation failed for stack.\n");
        return -1;
    }

    num_pages = (int)SZ_STACK/(int)page_size_ + ((int)SZ_STACK % (int)page_size_ != 0) + 1;

    thread->stack.memory = memory_align(thread->stack.memory_, (size_t)num_pages);
    thread->link = state.head;
    state.head = thread;
    
    return 0;
}

void schedule(void) {
    Thread *next = thread_candidate();

    if(next == NULL) {
        destroy();
    }

    else {
        state.cur_thread = next;
        state.cur_thread->status = STATUS_RUNNING;

    }
}