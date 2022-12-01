#ifndef COMP2310_CACHE_H
#define COMP2310_CACHE_H

#include <stdlib.h>
#include "csapp.h"
#include "cache.h"

#define MAX_CACHE_SIZE 1049000  // 1 Mb
#define MAX_OBJECT_SIZE 102400  // 1000 kb

/* Structure of a cache line consists of an identifier (loc),
 * the usage frequency (for LFU), the cahced web object, its size
 * and a pointer to the next cache line in the linked list
 */
typedef struct cache_line {
    unsigned int size;
    unsigned int frequency;
    char *loc;
    char *obj;
    struct line *next;
}line;

/* Structure of a web cache consists of a pointer to the first
 * line of the cache, the total size and the eviction policy (pol)
 */
 typedef struct web_cache {
    unsigned int size;
    unsigned int pol;
    line *start;
 }cache;

/* Function prototypes for cache operations */
void init_cache(cache *cash, int pol);
int full_cache(cache *cash);
void free_cache(cache *cash);
/* Function prototypes for cache_line operations */
line *in_cache(cache *cash, char *host, char *path);
line *make_line(char *host, char *path, char *object, size_t obj_size);
void add_line(cache *cash, line *lion);
void remove_line(cache *cash, line *lion);
line *choose_evict_lru(cache *cash);
line *choose_evict_lfu(cache *cash);
void free_line(cache *cash, line *lion);;

#endif