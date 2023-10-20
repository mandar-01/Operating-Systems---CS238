/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * jitc.c
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dlfcn.h>
#include "system.h"
#include "jitc.h"

/**
 * Needs:
 *   fork()
 *   execv()
 *   waitpid()
 *   WIFEXITED()
 *   WEXITSTATUS()
 *   dlopen()
 *   dlclose()
 *   dlsym()
 */

/* research the above Needed API and design accordingly */

struct jitc
{
	void *handle;
};

int jitc_compile(const char *input, const char *output)
{
	int status;

	pid_t pid = fork();

	if (pid == -1)
	{
		fprintf(stderr, "Fork failed\n");
		return -1;
	}

	if (pid == 0)
	{
		/* Child process */
		char* cmd[] = {"gcc", "lexer.o","jitc.o","parser.o","system.o","main.o","", "-o3", "-fPIC", "-shared", "-o", "","-lm", NULL};
		cmd[6] = (char*) input;
		cmd[11] = (char*) output;

		execv("/usr/bin/gcc", cmd);

		/* Handle the error for execv */
		fprintf(stderr, "Error in execv\n");
		exit(1);
	}
	else
	{
		/*  Parent process */
		if (waitpid(pid, &status, 0) == -1)
		{
			fprintf(stderr, "Error in waitpid\n");
			return -1;
		}

		if (WIFEXITED(status))
		{
			return WEXITSTATUS(status);
		}

		return -1;
	}
}

struct jitc *jitc_open(const char *pathname)
{
	struct jitc *jitc = malloc(sizeof(struct jitc));
	
	if (jitc)
	{
		printf("pathname %s\n",pathname);
		jitc->handle = dlopen(pathname, RTLD_LAZY);

		if (!jitc->handle)
		{	
			printf("jitc handle not found\n");		
			free(jitc);
		}
	}
	printf("open complete\n");
	fflush(stdout);
	return jitc;
}

void jitc_close(struct jitc *jitc)
{
	if (jitc)
	{
		dlclose(jitc->handle);
		free(jitc);
	}
}

long jitc_lookup(struct jitc *jitc, const char *symbol)
{
	if (jitc)
	{	
		printf("in lookup\n");
		printf("jitc->handle %p\n",jitc->handle);
		fflush(stdout);
		return (long)dlsym(jitc->handle, symbol);
	}
	return 0;
}
