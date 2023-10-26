/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * main.c
 */

// Memory Allocation Code

// -S flag tells compiler to generate Assembly code for current execution
// -o<level>, for e.g. -o3, modifies Assembly code according to the level as well
// Stack usage can be overridden and does not need to be preallocated. So, concurrent stack sharing cannot easily be done.
// Data cannot move into special registers like Stack Pointer, Program Counter, etc. if it does not exist in a general purpose register

// #define SZ_STACK 4096

// void fnc(void) {
// 	uint64_t rsp;
// 	rsp = 17;
// 	// rsp = (uint64_t)thread->stack.memory + SZ_STACK;
// 	__asm__ volatile ("mov %[rs], %%rsp \n" : [rs] "+r" [rsp] ::);
// }

// Scheduler function

// u_int64 scheduler_create(fnc, arg) {
// 	page_size x;
// 	allocate thread object
// 	initialize thread object
// 	link to global state variable
// 	*fnc.arg;
// 	allocate stack
// 	link it to state.head
//  return 0;
// }

// void scheduler_execute(void) {
// 	setjmp(checkpoint) -> state.ctx;
// 	schedule();
// 	destroy();
// }

// void scheduler_yield() {
// 	setjmp(checkpoint) -> state.thread -> ctx;
// 	// option 1- if thread is stll valid
// 	state.thread -> status = SLEEPING;
// 	longjmp(state.ctx);
// 	// option 2- if thread execution is completed or thread is invalid
// 	return 
// }

// void schedule(void) {
// 	get candidate from thread_candidate
// 	// option 1- no candidate is returned
// 	return
// 	// option 2- valid candidate is returnded
// 	state.thread = thread
// 	// not status_??
// 	thread.status = RUNNING;
// 	longjmp(thread->ctx);
// }

#include "system.h"
#include "scheduler.h"

static void
_thread_(void *arg)
{
	const char *name;
	int i;

	name = (const char *)arg;
	for (i=0; i<100; ++i) {
		printf("%s %d\n", name, i);
		us_sleep(20000);
		scheduler_yield();
	}
}

int
main(int argc, char *argv[])
{
	UNUSED(argc);
	UNUSED(argv);

	if (scheduler_create(_thread_, "hello") ||
	    scheduler_create(_thread_, "world") ||
	    scheduler_create(_thread_, "love") ||
	    scheduler_create(_thread_, "this") ||
	    scheduler_create(_thread_, "course!")) {
		TRACE(0);
		return -1;
	}
	scheduler_execute();
	return 0;
}
