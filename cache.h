#ifndef COMP2310_CACHE_H
#define COMP2310_CACHE_H

#include <stdlib.h>
#include "csapp.h"
#include "cache.h"

#define MAX_CACHE_SIZE 1049000  // 1 Mb
#define MAX_OBJECT_SIZE 102400  // 1000 kb

/* Structure of a cache line consists of an identifier (loc),
 * an age (for LRU), the cahced web object, its size, and
 * a pointer to the next cache line in the linked list
 */
typedef struct cache_line {
    unsigned int size;
    unsigned int age;
    char *loc;
    char *obj;
    struct line *next;
}line;

/* Structure of a web cache consists of a pointer to the first
 * line of the cache, the last line of the cache, and the total
 * size of the cache.
 */
 typedef struct web_cache {
    unsigned int size;
    line *start;
 }cache;

/* FUnction prototypes for cache operations */
void cache_init(cache *cash, pthread_rwlock_t *lock);
int cache_full(cache *cash);
void cache_free(cache *cash);
/* Function prototypes for cache_line operations */
line *in_cache(cache *cash, char *host, char *path);
line *make_line(char *host, char *path, char *object, size_t obj_size);
void add_line(cache *cash, line *lion);
void remove_line(cache *cash, line *lion);
line *choose_evict(cache *cash);
void free_line(cache *cash, line *lion);;
void age_lines(cache *cash);

#endif