#ifndef __CACHE_H__
#define __CACHE_H__


#include "csapp.h"

#define CACHE_TYPES 5

static const int cacheSize[] = {1024, 10240, 20480, 51200, 102400};
static const int cacheCnt[] = {24, 10, 5, 6, 5};

typedef struct {
    char *uri;
    char *obj;
    int objSize;
    int64_t time;
    pthread_rwlock_t rwlock; 
} cache_line;

typedef struct {
    int numOfLine;
    cache_line *cachep;
} cache_type;

cache_type caches[CACHE_TYPES];

int find_cache(char *uri, int fd);
void free_cache();
int64_t getTime();
void init_cache();
void write_cache(char *uri, char *obj, int len);

#endif

