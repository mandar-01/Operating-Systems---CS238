/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * main.c
 */

#include "system.h"
#include "scheduler.h"
#include <signal.h>
#include <unistd.h>

void signal_handler(int i) {
	assert(SIGALRM == i);
	scheduler_yield();
	alarm(1);
}

static void
_thread_(void *arg)
{
	const char *name;
	int i;

	name = (const char *)arg;
	for (i=0; i<100; ++i) {
		printf("%s %d\n", name, i);
		us_sleep(20000);
	}
}

int
main(int argc, char *argv[])
{
	if(SIG_ERR == signal(SIGALRM, *signal_handler)) {
		fprintf(stderr, "Error in signal handling.\n");
	}

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
	alarm(1);
	
	return 0;
}