/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * logfs.c
 */

#include <pthread.h>
#include "device.h"
#include "logfs.h"

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

/*
log fs abstraction to disk tape includes append command where offset need not be there. Just the buffer and length is enough as it linearly attaches blocks to memory.
*/

struct logfs {
    struct device *dev;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    uint64_t next_offset;
};

/**
 * Opens the block device specified in pathname for buffered I/O using an
 * append only log structure.
 *
 * pathname: the pathname of the block device
 *
 * return: an opaque handle or NULL on error
 */

struct logfs *logfs_open(const char *pathname) {
    struct logfs *log_fs;
    log_fs = malloc(sizeof(struct logfs));
    
    if (!log_fs) {
        TRACE("out of memory");
        return NULL;
    }

    log_fs->dev = device_open(pathname);
    if (!log_fs->dev) {
        TRACE("failed to open device");
        free(log_fs);
        return NULL;
    }

    if (pthread_mutex_init(&log_fs->mutex, NULL) != 0) {
        TRACE("mutex initialization failed");
        device_close(log_fs->dev);
        free(log_fs);
        return NULL;
    }

    if (pthread_cond_init(&log_fs->cond, NULL) != 0) {
        TRACE("condition variable initialization failed");
        pthread_mutex_destroy(&log_fs->mutex);
        device_close(log_fs->dev);
        free(log_fs);
        return NULL;
    }

    log_fs->next_offset = 0;

    return log_fs;
}

/**
 * Closes a previously opened logfs handle.
 *
 * logfs: an opaque handle previously obtained by calling logfs_open()
 *
 * Note: logfs may be NULL.
 */

void logfs_close(struct logfs *logfs) {
    if (!logfs) {
        return;
    }

    pthread_mutex_destroy(&logfs->mutex);
    pthread_cond_destroy(&logfs->cond);
    device_close(logfs->dev);

    free(logfs);
}

/**
 * Random read of len bytes at location specified in off from the logfs.
 *
 * logfs: an opaque handle previously obtained by calling logfs_open()
 * buf  : a region of memory large enough to receive len bytes
 * off  : the starting byte offset
 * len  : the number of bytes to read
 *
 * return: 0 on success, otherwise error
 */

/*
Offset and length alignment need not exist
*/

int logfs_read(struct logfs *logfs, void *buf, uint64_t off, size_t len) {
    if (!logfs || !buf) {
        return -1;
    }

    pthread_mutex_lock(&logfs->mutex);
    if (off + len > logfs->next_offset) {
        TRACE("attempt to read beyond end of log");
        pthread_mutex_unlock(&logfs->mutex);
        return -1;
    }
    pthread_mutex_unlock(&logfs->mutex);

    return device_read(logfs->dev, buf, off, len);
}

/**
 * Append len bytes to the logfs.
 *
 * logfs: an opaque handle previously obtained by calling logfs_open()
 * buf  : a region of memory holding the len bytes to be written
 * len  : the number of bytes to write
 *
 * return: 0 on success, otherwise error
 */

/*
NO offset as we are linearly appending blocks to disk
*/

int logfs_append(struct logfs *logfs, const void *buf, uint64_t len) {
    uint64_t offset;
    int result;
    
    if (!logfs || !buf) {
        return -1;
    }

    pthread_mutex_lock(&logfs->mutex);

    offset = logfs->next_offset;
    logfs->next_offset += len;

    pthread_mutex_unlock(&logfs->mutex);

    result = device_write(logfs->dev, buf, offset, len);

    pthread_cond_signal(&logfs->cond);

    return result;
}