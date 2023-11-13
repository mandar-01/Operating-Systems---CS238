/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * scm.c
 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include "scm.h"

/**
 * Needs:
 *   fstat()
 *   S_ISREG()
 *   open()
 *   close()
 *   sbrk()
 *   mmap()
 *   munmap()
 *   msync()
 */

/* research the above Needed API and design accordingly */

#define VIRT_ADDR 0x600000000000
struct scm
{
    int fd;
    char *memory;
    size_t size;
    char *memory_start;
    size_t capacity;
    size_t used_memory;
};

struct scm_meta
{
    size_t used_memory;
};

/**
 * Initializes an SCM region using the file specified in pathname as the
 * backing device, opening the regsion for memory allocation activities.
 *
 * pathname: the file pathname of the backing device
 * truncate: if non-zero, truncates the SCM region, clearning all data
 *
 * return: an opaque handle or NULL on error
 */

void *mapFile(int fd, size_t size)
{
    void *mapped_addr;
    mapped_addr = mmap((void *)VIRT_ADDR,
                       size,
                       PROT_READ | PROT_WRITE,
                       MAP_FIXED | MAP_SHARED,
                       fd,
                       0);

    if (MAP_FAILED == mapped_addr)
    {
        EXIT("Error in mapping the given file");
    }

    return mapped_addr;
}

struct scm *scm_open(const char *pathname, int truncate)
{
    int fd;
    struct stat stat;
    void *mapped_addr;
    struct scm *scm;
    struct scm_meta *meta;
    unsigned long meta_size;

    fd = open(pathname, O_RDWR);
    if (fd == -1)
    {
        EXIT("The specified filepath failed to open");
    }

    if (fstat(fd, &stat) == -1)
    {
        EXIT("Error in getting the fstat for the file");
    }

    mapped_addr = mapFile(fd, stat.st_size);

    meta_size = sizeof(struct scm_meta);

    scm = (struct scm *)malloc(sizeof(struct scm));
    scm->fd = fd;
    scm->memory = mapped_addr;
    scm->memory_start = scm->memory + meta_size;
    scm->size = stat.st_size;
    scm->capacity = stat.st_size - meta_size;

    if (0 == truncate)
    {
        meta = (struct scm_meta *)scm->memory;
        scm->used_memory = meta->used_memory;
    }
    return scm;
}

/**
 * Closes a previously opened SCM handle.
 *
 * scm: an opaque handle previously obtained by calling scm_open()
 *
 * Note: scm may be NULL
 */

void scm_close(struct scm *scm)
{
    struct scm_meta *meta = (struct scm_meta *)scm->memory;
    meta->used_memory = scm->used_memory;

    if (munmap(scm->memory, scm->capacity) == -1)
    {
        EXIT("Error while unmmapping the scm handle");
    }
    close(scm->fd);

    free(scm);
}

/**
 * Analogous to the standard C malloc function, but using SCM region.
 *
 * scm: an opaque handle previously obtained by calling scm_open()
 * n  : the size of the requested memory in bytes
 *
 * return: a pointer to the start of the allocated memory or NULL on error
 */

void *scm_malloc(struct scm *scm, size_t n)
{

    char *next_allocation = NULL;
    next_allocation = scm->memory_start + scm->used_memory;

    scm->used_memory += n;

    return next_allocation;
}

/**
 * Analogous to the standard C strdup function, but using SCM region.
 *
 * scm: an opaque handle previously obtained by calling scm_open()
 * s  : a C string to be duplicated.
 *
 * return: the base memory address of the duplicated C string or NULL on error
 */

char *scm_strdup(struct scm *scm, const char *s)
{
    size_t n = strlen(s);
    char *mem = (char *)scm_malloc(scm, n);
    memset(mem, 0, n);
    strcpy(mem, s);
    return mem;
}

/**
 * Analogous to the standard C free function, but using SCM region.
 *
 * scm: an opaque handle previously obtained by calling scm_open()
 * p  : a pointer to the start of a previously allocated memory
 */

void scm_free(struct scm *scm, void *p);

/**
 * Returns the number of SCM bytes utilized thus far.
 *
 * scm: an opaque handle previously obtained by calling scm_open()
 *
 * return: the number of bytes utilized thus far
 */

size_t scm_utilized(const struct scm *scm)
{
    return scm->used_memory;
}

/**
 * Returns the number of SCM bytes available in total.
 *
 * scm: an opaque handle previously obtained by calling scm_open()
 *
 * return: the number of bytes available in total
 */

size_t scm_capacity(const struct scm *scm)
{
    return scm->capacity;
}

/**
 * Returns the base memory address withn the SCM region, i.e., the memory
 * pointer that would have been returned by the first call to scm_malloc()
 * after a truncated initialization.
 *
 * scm: an opaque handle previously obtained by calling scm_open()
 *
 * return: the base memory address within the SCM region
 */

void *scm_mbase(struct scm *scm)
{
    return scm->memory_start;
}