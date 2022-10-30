#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "csapp.h"
#include "cache.h"

#define MAX_CACHE_SIZE 1049000  // 1 Mb
#define MAX_OBJECT_SIZE 102400  // 1000 kb

/* Structure of a cache line consists of an identifier (loc),
 * an age (for LRU), the cahced web object, its size, and
 * a pointer to the next cache line in the linked list
 */
struct cache_line {
    unsigned int size;
    unsigned int age;
    char *loc;
    char *obj;
    struct line *next;
};
typedef struct cache_line line;

/* Structure of a web cache consists of a pointer to the first
 * line of the cache, the last line of the cache, and the total
 * size of the cache.
 *
 */
 struct web_cache {
    unsigned int size;
    line *start;
 };