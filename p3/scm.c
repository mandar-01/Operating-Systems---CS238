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
#define SCM_META_SIZE sizeof(size_t)

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

void *scm_malloc(struct scm *scm, size_t n)
{
    const size_t hidden_size = sizeof(size_t);
    const size_t alloc_mem = n + hidden_size;
    char *mem = NULL;
    char *hidden_data = NULL;

    if (scm->used_memory + alloc_mem > scm->capacity) {
        TRACE("Exceed memory limit!");
        return NULL;
    }

    hidden_data = scm->memory_start + scm->used_memory;
    *((size_t *)hidden_data) = n;
    mem = hidden_data + hidden_size;

    printf("Address %p Allocated memory %lu\n", hidden_data, alloc_mem);

    scm->used_memory += alloc_mem;

    return mem;
}

char *scm_strdup(struct scm *scm, const char *s)
{
    size_t n = strlen(s);
    char *mem = (char *)scm_malloc(scm, n+1);
    memset(mem, 0, n+1);
    strcpy(mem, s);
    return mem;
}

void scm_free(struct scm *scm, void *p) {
    char *hidden_data = (char *)p - SCM_META_SIZE;
    size_t n = *((size_t *)hidden_data);
    memset(hidden_data, 0, SCM_META_SIZE + n);
    printf("Address %p Size %lu\n", hidden_data, SCM_META_SIZE + n);
    UNUSED(scm);
}

size_t scm_utilized(const struct scm *scm)
{
    return scm->used_memory;
}

size_t scm_capacity(const struct scm *scm)
{
    return scm->capacity;
}

void *scm_mbase(struct scm *scm)
{
    return scm->memory_start + SCM_META_SIZE;
}