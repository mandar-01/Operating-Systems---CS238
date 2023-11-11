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

#define SCM_META_SIZE sizeof(size_t)
uint32_t *VIRT_ADDR = (uint32_t *)0x600000000000;

struct scm {
   int fd;
   size_t capacity;
   size_t size;
   char *mem;
};

struct scm *scm_open(const char *pathname, int truncate) {
   struct scm *scm = (struct scm *)malloc(sizeof(struct scm));
   struct stat sb;
   int flags;

   if (!scm) {
      fprintf(stderr, "Failed to allocate memory for SCM handle.\n");
      return NULL;
   }

   flags = O_RDWR;

   if (truncate) {
      flags |= O_TRUNC;
   }

   scm->fd = open(pathname, flags);
   if (scm->fd == -1) {
      perror("Error in opening the file");
      free(scm);
      return NULL;
   }

   if (fstat(scm->fd, &sb) == -1) {
      perror("Error during fstat");
      close(scm->fd);
      free(scm);
      return NULL;
   }

   scm->capacity = sb.st_size;

   if (truncate) {
      scm->size = 0;
   } else {
      if (read(scm->fd, &scm->size, sizeof(size_t)) == -1) {
         perror("read");
         close(scm->fd);
         free(scm);
         return NULL;
      }
   }

   scm->mem = mmap(VIRT_ADDR, scm->capacity, PROT_READ | PROT_WRITE, MAP_SHARED, scm->fd, 0);
   if (scm->mem == MAP_FAILED) {
      perror("Error during mmap");
      close(scm->fd);
      free(scm);
      return NULL;
   }

   return scm;
}

void scm_close(struct scm *scm) {
   if (scm) {
      msync(scm->mem, scm->capacity, MS_SYNC);
      munmap(scm->mem, scm->capacity);
      close(scm->fd);
      free(scm);
   }
}

void *scm_malloc(struct scm *scm, size_t n) {
   size_t *scm_size = (size_t *)scm->mem;
   void *ptr = scm->mem + *scm_size + SCM_META_SIZE;

   if (!scm) {
      return NULL;
   }

   scm->size += n;
   *scm_size = scm->size;
   msync(scm->mem, scm->capacity, MS_SYNC);

   return ptr;
}

char *scm_strdup(struct scm *scm, const char *s) {
   size_t len = strlen(s) + 1;
   char *ptr = scm_malloc(scm, len);
   
   if (!scm) {
      return NULL;
   }

   if (ptr) {
      memcpy(ptr, s, len);
   }

   return ptr;
}

size_t scm_utilized(const struct scm *scm) {
   if (!scm) {
      return 0;
   }
   
   return scm->size;
}

size_t scm_capacity(const struct scm *scm) {
   if (!scm) {
      return 0;
   }
   
   return scm->capacity;
}

void *scm_mbase(struct scm *scm) {
   if (!scm) {
      return NULL;
   }
   
   return scm->mem + SCM_META_SIZE;
}