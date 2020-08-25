#include "cache.h"

void init_cache() {
    for (int i = 0; i < CACHE_TYPES; ++i) {
        caches[i].numOfLine = cacheCnt[i];
        caches[i].cachep = (cache_line *)malloc(cacheCnt[i] * sizeof(cache_line));
        cache_line *cur = caches[i].cachep;
        for (int j = 0; j < cacheCnt[i]; ++j, ++cur) {
            cur->uri = (char *)malloc(MAXLINE * sizeof(char));
            cur->obj = (char *)malloc(cacheSize[i] * sizeof(char));
            cur->objSize = 0;
            cur->time = 0;
            pthread_rwlock_init(&cur->rwlock, NULL);
       }
    }
}

/* If find successfully, return 0; else return -1 */
int find_cache(char *uri, int fd) {
    if (uri == NULL || fd < 0) {
        return -1;
    }

    int type = 0, lineIdx = 0;
    cache_line *cur = NULL;
    for (type = 0; type < CACHE_TYPES; ++type) {
        cur = caches[type].cachep;
        for (lineIdx = 0; lineIdx < cacheCnt[type]; ++lineIdx, ++cur) {
            if (cur->time != 0 && strcmp(uri, cur->uri) == 0) {
                break;
            }
        }
        if (lineIdx < cacheCnt[type]) {
            break;
        }
    }
    if (type == CACHE_TYPES) {
        return -1;
    }

    // may need confirm the uri is true, because write process may have changed uri???

    // If read successfully, update time stamp
    if (pthread_rwlock_trywrlock(&cur->rwlock) != 0) {
        cur->time = getTime();
        pthread_rwlock_unlock(&cur->rwlock);
    }

    pthread_rwlock_rdlock(&cur->rwlock);
    Rio_writen(fd, cur->obj, cur->objSize);
    pthread_rwlock_unlock(&cur->rwlock);
    return 0;
}

void write_cache(char *uri, char *obj, int len) {
    if (uri == NULL || obj == NULL) {
        return;
    }

    int type = 0;
    for (type = 0; type < CACHE_TYPES; ++type) {
        if (cacheSize[type] >= len) {
            break;
        }
    }

    // find empty cache line in this type
    cache_line *cur = caches[type].cachep;
    int lineIdx = 0;
    for (lineIdx = 0; lineIdx < cacheCnt[type]; ++lineIdx, ++cur) {
        if (cur->time == 0) {
            break;
        }
    }

printf("type %d\n", type);
    
    // need to replace one block of this type
    cache_line *lineOut = cur;
    if (lineIdx == cacheCnt[type]) {
        int64_t min = getTime();
        cur = caches[type].cachep;
        for (lineIdx = 0; lineIdx < cacheCnt[type]; ++lineIdx, ++cur) {
            if (cur->time <= min) {
                min = cur->time;
                lineOut = cur;
            }
        }
    }

printf("uri = %s\n", uri);
printf("len = %d, objsize = %ld\n", len, strlen(obj));

    pthread_rwlock_wrlock(&lineOut->rwlock);
    lineOut->time = getTime();
    lineOut->objSize = len;
    memcpy(lineOut->uri, uri, MAXLINE);
    memcpy(lineOut->obj, obj, len);
    pthread_rwlock_unlock(&lineOut->rwlock);
}

void free_cache() {
    for (int i = 0; i < CACHE_TYPES; ++i) {
        cache_line *cur = caches[i].cachep;
        for (int j = 0; j < cacheCnt[i]; ++j, ++cur) {
            free(cur->uri);
            free(cur->obj);
            pthread_rwlock_destroy(&cur->rwlock);
        }
        free(caches[i].cachep);
    }
}

int64_t getTime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

