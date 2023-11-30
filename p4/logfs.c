/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * logfs.c
 */

#include <pthread.h>
#include <inttypes.h>
#include "device.h"
#include "logfs.h"
#include <unistd.h>
#include "system.h"

#define WCACHE_BLOCKS 32
#define RCACHE_BLOCKS 256

/**
 * Needs:
 *   pthread_create()
 *   pthread_join()
 *   pthread_mutex_init()
 *   pthread_mutex_destroy()
 *   pthread_mutex_lock()
 *   pthread_mutex_unlock()
 *   pthread_cond_init()
 *   pthread_cond_destroy()
 *   pthread_cond_wait()
 *   pthread_cond_signal()
 */

/* research the above Needed API and design accordingly */

#define BLOCK_SIZE 4096
int remaining = 0;

struct logfs
{
    pthread_mutex_t mutex;
    struct device *device;
    pthread_cond_t cond;
    pthread_t consumer;
    char *queue;
    uint64_t utilized;
    uint64_t head;
    uint64_t tail;
    int terminate;
    int flushStart;
    int flushComplete;
};

void *threadFunction(void *arg)
{
    struct logfs *logfs = (struct logfs *)arg;
    while (1)
    {
        if (logfs->terminate == 1)
            break;
        
        while (logfs->tail - logfs->head < BLOCK_SIZE && logfs->terminate == 0)
        {
            pthread_cond_wait(&logfs->cond, &logfs->mutex);
            printf("%d\n",logfs->flushStart);
            fflush(stdout);
            if(logfs->flushStart == 1){
                break;
            }
        }
        printf("Consumer signalled\n");
        fflush(stdout);
        if (logfs->flushStart == 1)
        {   
            printf("Flushing the queue\n");
            pthread_mutex_lock(&logfs->mutex);
            char *buf;
            buf = (char *)malloc(sizeof(char) * BLOCK_SIZE);
            memcpy(buf, logfs->queue + logfs->head, BLOCK_SIZE);
            if (0 == device_write(logfs->device, buf, logfs->utilized, BLOCK_SIZE))
            {
                printf("Data Written to disk\n");
                logfs->utilized += logfs->tail - logfs->head;
            }
            logfs->flushComplete = 1;
            printf("Flushing complete\n");
            printf("Singaling for read\n");
            pthread_cond_signal(&logfs->cond);
            pthread_mutex_unlock(&logfs->mutex);
        }
        else if (logfs->tail > 0 && logfs->tail - logfs->head >= BLOCK_SIZE)
        {
            char *buf;
            buf = (char *)malloc(sizeof(char) * BLOCK_SIZE);
            memcpy(buf, logfs->queue + logfs->head, BLOCK_SIZE);
            logfs->head += BLOCK_SIZE;
            if (0 == device_write(logfs->device, buf, logfs->utilized, BLOCK_SIZE))
            {
                printf("Data Written to disk\n");
                logfs->utilized += BLOCK_SIZE;
            }
        }
    }
    return 0;
}

struct logfs *logfs_open(const char *pathname)
{
    struct logfs *logfs = (struct logfs *)malloc(sizeof(struct logfs));
    logfs->queue = (char *)malloc(sizeof(char) * WCACHE_BLOCKS * BLOCK_SIZE);
    logfs->device = device_open(pathname);
    pthread_mutex_init(&logfs->mutex, NULL);
    pthread_cond_init(&logfs->cond, NULL);
    pthread_create(&logfs->consumer, NULL, threadFunction, logfs);
    logfs->terminate = 0;
    logfs->utilized = 0;
    logfs->head = 0;
    logfs->tail = 0;
    logfs->flushStart = 0;
    logfs->flushComplete = 0;

    return logfs;
}

void logfs_close(struct logfs *logfs)
{
    logfs->terminate = 1;
    pthread_cond_signal(&logfs->cond);
    printf("File Data Written = %" PRIu64 "and Queue size %" PRIu64 "\n", logfs->utilized, logfs->tail);
    pthread_join(logfs->consumer, NULL);
    pthread_cond_destroy(&logfs->cond);
    pthread_mutex_destroy(&logfs->mutex);
    free(logfs);
}

int logfs_read(struct logfs *logfs, void *buf, uint64_t off, size_t len)
{   
    printf("Read request arrived\n");
    pthread_mutex_lock(&logfs->mutex);
    // if(off > logfs->tail){
    //     printf("Offset for read is more than data appended\n");
    //     return 0;
    // }
    while (off > logfs->utilized)
    {
        printf("Offset %" PRIu64 " is greater than the file size %" PRIu64 "\n", off, logfs->utilized);
        logfs->flushStart = 1;
        pthread_cond_signal(&logfs->cond);
        while (logfs->flushComplete == 0)
        {
            pthread_cond_wait(&logfs->cond, &logfs->mutex);
        }
        printf("Flush complete from read func\n");
        logfs->flushStart = 0;
        logfs->flushComplete = 0;
        printf("flush variables reset\n");
        device_read(logfs->device, buf, off, len);
        printf("Buffer read from disk:  %s\n", (char *)buf);
        pthread_cond_signal(&logfs->cond);
        pthread_mutex_unlock(&logfs->mutex);
        return 0;
    }
    printf("Device read offset and length %" PRIu64 "%"PRIu64 "\n", off,len);
    device_read(logfs->device, buf, 0, BLOCK_SIZE);
    printf("Buffer read from disk:  %s\n", (char *)buf);
    pthread_cond_signal(&logfs->cond);
    pthread_mutex_unlock(&logfs->mutex);
    return 0;
}

int logfs_append(struct logfs *logfs, const void *buf, uint64_t len)
{
    pthread_mutex_lock(&logfs->mutex);
    memcpy(logfs->queue + logfs->tail, buf, len);
    logfs->tail += len;
    printf("Appending done for length %d, now Signalling---------->\n", (int)len);
    pthread_cond_signal(&logfs->cond);
    pthread_mutex_unlock(&logfs->mutex);
    return 0;
}