/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * scheduler.h
 */

#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

/**
 * scheduler_fnc_t defines the signature of the user thread function to
 * be scheduled by the scheduler. The user thread function will be supplied
 * with a copy of the arg specified when creating the thread.
 */

typedef void (*scheduler_fnc_t)(void *arg);

/**
 * Creates a new user thread.
 *
 * fnc: the start function of the user thread (see scheduler_fnc_t)
 * arg: a pass-through pointer defining the context of the user thread
 *
 * return: 0 on success, otherwise error

        page_size ?
        allocate thread obj use malloc
        initialize object (*fnc,arg; allocate stack-malloc needed)

        link it to state head

        return 0 for success
 */

int scheduler_create(scheduler_fnc_t fnc, void *arg);

/**
 * Called to execute the user threads previously created by calling
 * scheduler_create().
 *
 * Notes:
 *   * This function should be called after a sequence of 0 or more
 *     scheduler_create() calls.
 *   * This function returns after all user threads (previously created)
 *     have terminated.
 *   * This function is not re-enterant.

    create checkpoint and store in state ctx (this is the place where we come to decide what to run next, the state ctx)
    schedule() (implement this)
    destroy() (not able to schedule anything, thread candidate returned null, all done)

 */

void scheduler_execute(void);

/**
 * Called from within a user thread to yield the CPU to another user thread.

    checkpoint to the thread that is currently running (state.thread -> ctx). this is also the point where the next thread from future continues running.
    two paths after checkpoint. 

    in one of the paths:
     do nothing which means there is nobody to run and we are done. 
    
    in other path:
     state.thread -> status = SLEEP
     longjmp(state.ctx)

 */

void scheduler_yield(void);

void schedule(void);

struct thread *thread_candidate(void);

void destroy(void);

#endif /* _SCHEDULER_H_ */