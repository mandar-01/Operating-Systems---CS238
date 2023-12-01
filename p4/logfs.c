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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>

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

// #define WCACHE_BLOCKS 32
#define NUM_BLOCKS 256

struct device
{
    int fd;
    uint64_t size;  /* immutable */
    uint64_t block; /* immutable */
};

typedef struct
{
    uint64_t offset;
    size_t length;
    char *data;
} CacheBlock;

typedef struct AppendData
{
    char *data;
    uint64_t len;
    struct AppendData *next;
} AppendData;

struct logfs
{
    int device;
    struct device *block_device;
    CacheBlock read_cache[NUM_BLOCKS];
    pthread_mutex_t read_mutex;
    pthread_mutex_t write_mutex;
    pthread_cond_t write_cond;
    AppendData *append_queue;
    AppendData *append_queue_tail;
    pthread_t worker_thread;
    uint64_t highest_written_offset;
    bool stop_worker;
};

static void initialize_read_cache(struct logfs *log)
{
    int i;

    for (i = 0; i < NUM_BLOCKS; ++i)
    {
        log->read_cache[i].offset = UINT64_MAX;
        log->read_cache[i].length = 0;
        log->read_cache[i].data = NULL;
    }
}

void initiliaze_logfs_attributes(struct logfs *logfs)
{
    logfs->append_queue = NULL;
    logfs->append_queue_tail = NULL;
    logfs->highest_written_offset = 0;
    logfs->stop_worker = false;
}

static void *worker_function(void *arg)
{
    struct logfs *logfs = (struct logfs *)arg;
    AppendData *data;
    ssize_t bytes_written;
    // int write_status;

    while (true)
    {
        pthread_mutex_lock(&logfs->write_mutex);

        while (logfs->append_queue == NULL && !logfs->stop_worker)
        {
            pthread_cond_wait(&logfs->write_cond, &logfs->write_mutex);
        }

        if (logfs->stop_worker && logfs->append_queue == NULL)
        {
            pthread_mutex_unlock(&logfs->write_mutex);
            break;
        }

        data = logfs->append_queue;
        logfs->append_queue = data->next;
        if (logfs->append_queue == NULL)
        {
            logfs->append_queue_tail = NULL;
        }

        bytes_written = pwrite(logfs->device, data->data, data->len, logfs->highest_written_offset);
        // bytes_written = pwrite(logfs->block_device->fd, data->data, data->len, logfs->highest_written_offset);

        if (bytes_written > 0)
        {
            logfs->highest_written_offset += bytes_written;
        }
        else
        {
            TRACE(0);
        }

        // if(0 == device_write(logfs->device, data->data, logfs->highest_written_offset, data->len)) {
        //     logfs->highest_written_offset += data->len;
        // } else {
        //     TRACE(0);
        // }

        pthread_mutex_unlock(&logfs->write_mutex);
        free(data->data);
        free(data);

        pthread_cond_broadcast(&logfs->write_cond);
    }

    return NULL;
}

struct logfs *logfs_open(const char *pathname)
{
    struct logfs *logfs = malloc(sizeof(struct logfs));
    // if (logfs == NULL)
    // {
    //     return NULL;
    // }

    // logfs->block_device = open(pathname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    // if (logfs->block_device < 0)
    // {
    //     free(logfs);
    //     return NULL;
    // }

    logfs->block_device = device_open(pathname);

    if (!logfs->block_device)
    {
        free(logfs);
        TRACE(0);
        return NULL;
    }

    pthread_mutex_init(&logfs->read_mutex, NULL);
    pthread_mutex_init(&logfs->write_mutex, NULL);
    pthread_cond_init(&logfs->write_cond, NULL);

    initialize_read_cache(logfs);
    // logfs->append_queue = NULL;
    // logfs->append_queue_tail = NULL;
    // logfs->highest_written_offset = 0;
    // logfs->stop_worker = false;
    initiliaze_logfs_attributes(logfs);

    if (pthread_create(&logfs->worker_thread, NULL, worker_function, logfs) != 0)
    {
        device_close(logfs->block_device);
        free(logfs);
        return NULL;
    }

    return logfs;
}

void logfs_close(struct logfs *logfs)
{
    int i;

    if (logfs != NULL)
    {
        pthread_mutex_lock(&logfs->write_mutex);
        logfs->stop_worker = true;
        pthread_cond_signal(&logfs->write_cond);
        pthread_mutex_unlock(&logfs->write_mutex);

        pthread_join(logfs->worker_thread, NULL);

        device_close(logfs->block_device);
        pthread_mutex_destroy(&logfs->read_mutex);
        pthread_mutex_destroy(&logfs->write_mutex);
        pthread_cond_destroy(&logfs->write_cond);

        for (i = 0; i < NUM_BLOCKS; ++i)
        {
            free(logfs->read_cache[i].data);
        }

        free(logfs);
    }
}

int logfs_read(struct logfs *logfs, void *buf, uint64_t off, size_t len)
{
    int i;
    ssize_t read_bytes;

    // if (logfs == NULL || buf == NULL || len == 0)
    // {
    //     return 0;
    // }

    if (!logfs || !buf || !len)
    {
        return 0;
    }

    pthread_mutex_lock(&logfs->read_mutex);
    pthread_mutex_lock(&logfs->write_mutex);

    while (off + len > logfs->highest_written_offset && len > 0)
    {
        pthread_cond_wait(&logfs->write_cond, &logfs->write_mutex);
    }
    pthread_mutex_unlock(&logfs->write_mutex);

    for (i = 0; i < NUM_BLOCKS; ++i)
    {
        CacheBlock *cache = &logfs->read_cache[i];
        if (cache->offset <= off && off + len <= cache->offset + cache->length && cache->data)
        {
            size_t cache_offset = off - cache->offset;
            memcpy(buf, cache->data + cache_offset, len);
            pthread_mutex_unlock(&logfs->read_mutex);
            return 0;
        }
    }

    if (0 >= device_read(logfs->block_device, buf, off, len))
    {
        pthread_mutex_unlock(&logfs->read_mutex);
        return -1;
    }

    // read_bytes = pread(logfs->block_device->fd, buf, len, off);
    // if (read_bytes < 0 || (size_t)read_bytes != len)
    // {
    //     pthread_mutex_unlock(&logfs->read_mutex);
    //     return -1;
    // }

    for (i = 0; i < NUM_BLOCKS; ++i)
    {
        if (logfs->read_cache[i].offset == UINT64_MAX || logfs->read_cache[i].length == 0)
        {
            logfs->read_cache[i].offset = off;
            logfs->read_cache[i].length = len;
            if (logfs->read_cache[i].data != NULL)
            {
                free(logfs->read_cache[i].data);
            }
            logfs->read_cache[i].data = malloc(len);
            // if (logfs->read_cache[i].data == NULL)
            // {
            //     pthread_mutex_unlock(&logfs->read_mutex);
            //     return -1;
            // }
            memcpy(logfs->read_cache[i].data, buf, len);
            pthread_mutex_unlock(&logfs->read_mutex);
            return 0;

            break;
        }
    }

    // pthread_mutex_unlock(&logfs->read_mutex);
    return 0;
}

int logfs_append(struct logfs *logfs, const void *buf, uint64_t len)
{

    AppendData *append_data;

    if (logfs == NULL)
    {

        return -1;
    }
    if ((buf == NULL && len > 0) || (len == 0))
    {

        return 0;
    }

    append_data = malloc(sizeof(AppendData));

    append_data->data = malloc(len);
    append_data->len = len;
    append_data->next = NULL;
    // if (append_data->data == NULL)
    // {

    //     free(append_data);
    //     return -1;
    // }

    memcpy(append_data->data, buf, len);

    pthread_mutex_lock(&logfs->write_mutex);

    if (logfs->append_queue_tail == NULL)
    {
        logfs->append_queue = append_data;
    }
    else
    {
        logfs->append_queue_tail->next = append_data;
    }
    logfs->append_queue_tail = append_data;

    pthread_cond_signal(&logfs->write_cond);
    pthread_mutex_unlock(&logfs->write_mutex);

    return 0;
}