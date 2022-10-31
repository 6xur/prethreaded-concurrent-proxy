#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "csapp.h"
#include "cache.h"

/*****************
 * CACHE FUNCTIONS
 *****************/

void cache_init(cache *cash){ 
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


/**********************
 * CACHE LINE FUNCTIONS
 **********************/

 /* In cache - Determines if a web object is already in the cache
  * Returns pointer to line if it is, NULL if it isn't 
  */
line *in_cache(cache* cash, char *host, char *path){
    size_t hp = strlen(host) + strlen(path) + 2;
    char loc[hp];

    memset(loc, 0, sizeof(loc));

    /* CRITICAL SECTION: READING */
    /* Nothing is in the cache if it's empty */
    if(cash->size == 0) return NULL;
    /* Incr age of lines */
    age_lines(cash);
    /* Create the location given host and path */
    strcat(loc, host);
    strcat(loc, path);

    /* Determine if this object is cached */
    line *object = NULL;
    line *lion = cash->start;
    while(lion != NULL){
        if(!strcmp(loc, lion->loc)){
            object = lion;
            break;  // object found!
        }
        lion = lion->next;
    }
    /* END CRITICAL SECTION*/

    return object;
}

/* Create a line that can be inserted into the cache using a given hostname,
 * path to an object, size of the object, and the object as it would be returned
 * to the client.
 * Returns a pointer to this line
 */
line *make_line(char *host, char *path, char *object, size_t obj_size){
    /* Variables to build the elements of the line */
    line *lion;

    size_t loc_size = strlen(host) + strlen(path) + 2;
    char location[loc_size];

    memset(location, 0, sizeof(location));

    /* Allocate space for this line */
    lion = Malloc(sizeof(struct cache_line));

    /* Set size and age of line */
    lion->size = (unsigned int)obj_size;
    lion->age = 0;

    /* Set the location of the line (identifier) */
    /* Combine host & path */
    strcat(location, host);
    strcat(location, path);

    /* Allocate space for loc*/
    loc_size = strlen(location) + 1;
    lion->loc = Malloc(loc_size);
    /* Finish */
    memcpy(lion->loc, location, loc_size);

    /* Set the object of the line (core purpose of line) */
    lion->obj = Malloc(obj_size + 1);     // allocate space for obj
    memcpy(lion->obj, object, obj_size);  // finish

    /* A brand new line is alone in the world until added to the cache */
    lion->next = NULL;

    return lion;
}

/* Add a line to the cache and evict if necessary
 * Must call make_line before adding a line
*/
void add_line(cache *cash, line *lion) {
  /* CRITICAL SECTION: WRITE */
  /* If the cache is full, choose a line to evict & remove it */
  if (cache_full(cash))
    remove_line(cash, choose_evict(cash));
  /* Insert the line at the beginning of the list */
  lion->next = cash->start;
  cash->start = lion;
  /* Update the cache size accordingly */
  cash->size += lion->size;
  /* END CRITICAL SECTION */
}

/* Age the cache (for LRU policy) */
void age_lines(cache *cash){
    line *lion = cash->start;
    /* Increment age of all lines */
    while(lion != NULL){
        (lion->age)++;
        lion = lion->next;
    }
}

/* Remove a line from the cache */
void remove_line(cache *cash, line *lion){
    line *tmp = cash->start;
    /* Case: first line of cache */
    if(tmp == lion){
        // Adjust start of cache
        cash->start = lion->next;
        // Fully free line
        free_line(cash, lion);
        free(lion);
        return;
    }
    while(tmp != NULL){
        /* Case: middle line of cache */
        if(tmp->next == lion){
            // Adjust previous line's next ptr
            tmp->next = lion->next;
            // Fully free line
            free_line(cash, lion);
            free(lion);
            return;
        }
        /* Continue */
        else tmp = tmp->next;
    }
    /* Case: line not found.. can't remove */
    printf("remove_line error: line not found");
}

/* Choose a line to evict using an LRU policy
 * Return a pointer to the chosen line
 */
line *choose_evict(cache *cash){
  line *evict, *lion;
  int eldest = -1;

  lion = cash->start;
  evict = lion;
  /* Search the cache for the oldest line */
  while (lion != NULL) {
    if (lion->age > eldest) {
      eldest = lion->age;
      evict = lion;
    }
    lion = lion->next;
  }
  return evict;
}

 /* Free a specified line from cache */
void free_line(cache *cash, line *lion){
  /* Before freeing, update cache size */
  cash->size -= lion->size;
  /* Free elements of line (except next--needed for freeing cache) */
  Free(lion->loc);
  Free(lion->obj);
}