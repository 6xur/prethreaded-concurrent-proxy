#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
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
 */
 struct web_cache {
    unsigned int size;
    line *start;
 };
typedef struct web_cache cache;

/* FUnction prototypes for cache operations */
void cache_init(cache *cache, pthread_rwlock_t *lock);
int cache_full(cache *cache);
void cache_free(cache *cache);
/* Function prototypes for cache line operations */
void free_line(cache *cache, line *line);


/*****************
 * CACHE FUNCTIONS
 *****************/

void cache_init(cache *cash, pthread_rwlock_t *lock){ 
  /* Initialize read-write lock */
  pthread_rwlock_init(lock, NULL);

  /* Init cache to empty state */
  cash->size = 0;
  cash->start = NULL;
}

 /* Returns 1 if cache is full, 0 if not */
 int cache_full(cache *cash){
  // The cache is full if there isn't enough room for another object
  return ((MAX_CACHE_SIZE - (cash->size)) < MAX_OBJECT_SIZE);
}

/* Free the cache from memory, including all of the lines in it */
void cache_free(cache *cash){
    /* Need a ptr to keep track of next so current->next can be freed */
    line *lion = cash->start;
    line *nextlion = lion->next;
    /* Free all the lines in the cache */
    while(lion != NULL){
        /* Free a line */
        free_line(cash, lion);
        Free(lion->next);
        lion = nextlion;
        /* Get the next line to be freed */
        if(nextlion != NULL){
            nextlion = nextlion->next;
        } else{
            break;
        }
    }
    /* Free start of cache */
    Free(cash->start);
}


/* Free a specified line from cache */
void free_line(cache *cash, line *lion){
  /* Before freeing, update cache size */
  cash->size -= lion->size;
  /* Free elements of line (except next--needed for freeing cache) */
  Free(lion->loc);
  Free(lion->obj);
}
