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
 *   fstat() => fstat(fd, pointer to struct s) s is of type struct stat s
 *   S_ISREG()
 *   open() => open(pathname, O_RDWR) returns file descriptor. 
    check if file is a regular file, a file that we can read/write. a flag tells us this. S_ISREG(s.st_mode) s is the same struct (this was in prof's code, erorr check)
    s.st_size ?
 
 *   close() => close(file descriptor)
 *   sbrk() => give it address, it will give break point address in heap, VIRT_ADDR = OX600000000000
 *   mmap() => load disk memory in RAM if what we access is not available in RAM. returns address where file is mapped.

    give address you want, will map file to that address.
    mmap(VIRT_ADDR,length,PROT_READ|PROT_WRITE|MAP_FIXED|MAP_SHARED,file descriptor, 0)
    whatever returned should match virt_addr. 
 *   munmap() => opposite of mmap()
 *   msync() => 
 */

/* research the above Needed API and design accordingly */
