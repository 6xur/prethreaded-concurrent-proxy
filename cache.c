/* Parts of the following code use functions from https://github.com/jcksber/CMU_15-213_caching-web-proxy */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "csapp.h"
#include "cache.h"


/*****************
 * CACHE FUNCTIONS
 *****************/

void init_cache(cache *cash, int pol){
  /* Eviction policy 
   * 0 - Least Recently Used   (LRU)
   * 1 - Least Frequently Used (LFU)
  */
  cash->pol = pol;
  cash->size = 0;
  cash->start = NULL;
}

 /* Return 1 if cache is full, 0 if not */
 int full_cache(cache *cash){
  /* The cache is full if there isn't enough room for another object */
  return ((MAX_CACHE_SIZE - (cash->size)) < MAX_OBJECT_SIZE);
}

/* Free the cache from memory, including all of the lines in it */
void free_cache(cache *cash){
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

 /* Determines if a web object is already in the cache
  * return pointer to line if it is, NULL if it isn't 
  */
line *in_cache(cache* cash, char *host, char *path){
    size_t hp = strlen(host) + strlen(path) + 2;
    char loc[hp];

    memset(loc, 0, sizeof(loc));

    /* CRITICAL SECTION: READING */
    /* Nothing is in the cache if it's empty */
    if(cash->size == 0) return NULL;
    /* Create the location given host and path */
    strcat(loc, host);
    strcat(loc, path);

    /* Determine if this object is cached */
    line *object = NULL;
    line *lion = NULL;
    line *nextlion = cash->start;

    /* If it is found in the cache, we move it
     * to the head of the cache and return its value 
     */
    while(nextlion->next != NULL){
        if(!strcmp(loc, nextlion->loc)){  // object found
            lion->next = nextlion->next;
            nextlion->next = cash->start;
            cash->start = nextlion;

            (nextlion->frequency)++;     // object is used, increment frequency counter
            object = nextlion;
            break;
        }
        lion = nextlion;
        nextlion = nextlion->next;
    }
    /* END CRITICAL SECTION*/

    return object;
}

/* Create a line that can be inserted into the cache using a given
 * host, path to an object, size of the object, and the object as
 * it would be returned to the client
 * Return a pointer to this line
 */
line *make_line(char *host, char *path, char *object, size_t obj_size){
    /* Variables to build the elements of the line */
    line *lion;

    size_t loc_size = strlen(host) + strlen(path) + 2;
    char location[loc_size];

    memset(location, 0, sizeof(location));

    /* Allocate space for this line */
    lion = Malloc(sizeof(struct cache_line));

    /* Set size and frequency of line */
    lion->size = (unsigned int)obj_size;
    lion->frequency = 0;

    /* Set the location of the line (identifier) */
    /* Combine host & path */
    strcat(location, host);
    strcat(location, path);

    /* Allocate space for loc*/
    loc_size = strlen(location) + 1;
    lion->loc = Malloc(loc_size);
    /* Finish */
    memcpy(lion->loc, location, loc_size);

    /* Set the object of the line */
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
  if (full_cache(cash) && cash->pol == 0){
    remove_line(cash, choose_evict_lru(cash));
  }

  if (full_cache(cash) && cash->pol == 1){
    remove_line(cash, choose_evict_lfu(cash));
  }

  /* Insert the line at the beginning of the list */
  lion->next = cash->start;
  cash->start = lion;
  /* Update the cache size accordingly */
  cash->size += lion->size;
  /* END CRITICAL SECTION */
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
    fprintf(stderr,"remove_line error: line not found\n");
}

/* Choose the least recently used line, which is
 * present at the tail of the cache */
line *choose_evict_lru(cache *cash){
  line *evict;
  evict = cash->start;
  while (evict->next != NULL) {
    evict = evict->next;
  }
  return evict;
}

/* Return a poiner to the least frequently used line*/
line *choose_evict_lfu(cache *cash){
  line *evict, *lion;
  int min = 99999;  // placeholder

  lion = cash->start;
  evict = lion;
  /* Search the cache for the oldest line */
  while (lion != NULL) {
    if (lion->frequency < min) {
      min = lion->frequency;
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
  /* Free elements of line (except next, needed for freeing cache) */
  Free(lion->loc);
  Free(lion->obj);
}