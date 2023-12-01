#define _POSIX_C_SOURCE 200809L
#include "logfs.h"
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define RCACHE_BLOCKS 256

struct Node
{
    char *data;
    uint64_t len;
    struct Node *link;
};

typedef struct
{
    uint64_t offset;
    size_t length;
    char *data;
} CacheBlock;

struct logfs
{
    int device;
    CacheBlock read_cache[RCACHE_BLOCKS];
    pthread_mutex_t read_mutex;
    pthread_mutex_t write_mutex;
    pthread_cond_t condition;
    struct Node *head;
    struct Node *tail;
    pthread_t worker;
    uint64_t utilized;
    int stop;
};

static void start_cache(struct logfs *log)
{
    int i;

    for (i = 0; i < RCACHE_BLOCKS; ++i)
    {
        log->read_cache[i].offset = UINT64_MAX;
        log->read_cache[i].length = 0;
        log->read_cache[i].data = NULL;
    }
}

static void *worker_function(void *arg)
{
    struct logfs *logfs = (struct logfs *)arg;
    struct Node *data;
    ssize_t bytes_written;

    while (1)
    {
        pthread_mutex_lock(&logfs->write_mutex);

        while (logfs->head == NULL && !logfs->stop)
        {
            pthread_cond_wait(&logfs->condition, &logfs->write_mutex);
        }

        if (logfs->stop && logfs->head == NULL)
        {
            pthread_mutex_unlock(&logfs->write_mutex);
            break;
        }

        data = logfs->head;
        logfs->head = data->link;
        if (logfs->head == NULL)
        {
            logfs->tail = NULL;
        }

        bytes_written = pwrite(logfs->device, data->data, data->len, logfs->utilized);
        if (bytes_written > 0)
        {
            logfs->utilized += bytes_written;
        }
        else
        {
            TRACE(0);
        }

        pthread_mutex_unlock(&logfs->write_mutex);
        free(data->data);
        free(data);

        pthread_cond_broadcast(&logfs->condition);
    }

    return NULL;
}

struct logfs *logfs_open(const char *pathname)
{
    struct logfs *logfs = malloc(sizeof(struct logfs));
    if (logfs == NULL)
    {
        return NULL;
    }

    logfs->device = open(pathname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (logfs->device < 0)
    {
        free(logfs);
        return NULL;
    }

    pthread_mutex_init(&logfs->read_mutex, NULL);
    pthread_mutex_init(&logfs->write_mutex, NULL);
    pthread_cond_init(&logfs->condition, NULL);

    start_cache(logfs);
    logfs->head = NULL;
    logfs->tail = NULL;
    logfs->utilized = 0;
    logfs->stop = 0;

    if (pthread_create(&logfs->worker, NULL, worker_function, logfs) != 0)
    {
        close(logfs->device);
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
        logfs->stop = 1;
        pthread_cond_signal(&logfs->condition);
        pthread_mutex_unlock(&logfs->write_mutex);

        pthread_join(logfs->worker, NULL);

        close(logfs->device);
        pthread_mutex_destroy(&logfs->read_mutex);
        pthread_mutex_destroy(&logfs->write_mutex);
        pthread_cond_destroy(&logfs->condition);

        for (i = 0; i < RCACHE_BLOCKS; ++i)
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

    if (logfs == NULL || buf == NULL || len == 0)
    {
        return 0;
    }

    pthread_mutex_lock(&logfs->read_mutex);
    pthread_mutex_lock(&logfs->write_mutex);

    while (off + len > logfs->utilized && len > 0)
    {
        pthread_cond_wait(&logfs->condition, &logfs->write_mutex);
    }
    pthread_mutex_unlock(&logfs->write_mutex);

    for (i = 0; i < RCACHE_BLOCKS; ++i)
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

    read_bytes = pread(logfs->device, buf, len, off);
    if (read_bytes < 0 || (size_t)read_bytes != len)
    {
        pthread_mutex_unlock(&logfs->read_mutex);
        return -1;
    }

    for (i = 0; i < RCACHE_BLOCKS; ++i)
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
            if (logfs->read_cache[i].data == NULL)
            {
                pthread_mutex_unlock(&logfs->read_mutex);
                return -1;
            }
            memcpy(logfs->read_cache[i].data, buf, len);

            break;
        }
    }

    pthread_mutex_unlock(&logfs->read_mutex);
    return 0;
}

int logfs_append(struct logfs *logfs, const void *buf, uint64_t len)
{

    struct Node *new_data;

    if (logfs == NULL)
    {

        return -1;
    }
    if (buf == NULL && len > 0)
    {

        return 0;
    }
    if (len == 0)
    {

        return 0;
    }

    new_data = malloc(sizeof(struct Node));
    if (new_data == NULL)
    {

        return -1;
    }

    new_data->data = malloc(len);
    if (new_data->data == NULL)
    {

        free(new_data);
        return -1;
    }

    memcpy(new_data->data, buf, len);
    new_data->len = len;
    new_data->link = NULL;

    pthread_mutex_lock(&logfs->write_mutex);

    if (logfs->tail == NULL)
    {
        logfs->head = new_data;
    }
    else
    {
        logfs->tail->link = new_data;
    }
    logfs->tail = new_data;

    pthread_cond_signal(&logfs->condition);
    pthread_mutex_unlock(&logfs->write_mutex);

    return 0;
}