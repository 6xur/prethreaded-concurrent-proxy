#include <stdio.h>
#include "sbuf.h"
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define NTHREADS 4
#define SBUFSIZE 16

sbuf_t sbuf;  // shared buffer of connected descriptors

// TODO: remove threads because we don't need to keep track of all thread IDs
// read through the lecture slides

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36\r\n";

/* Request handling functions */
void *thread(void *vargp);
void my_connect(int connfd);
int parse_req(int connection, rio_t *rio, char *host, char *port, char *path);

/* sbuf functions */
void sbuf_init(sbuf_t *sp, int n);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);

int main(int argc, char **argv){
    int listenfd, connfd;
    pthread_t tid;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check commandline arguments */
    if(argc != 2){
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    /* Listen on port specified by the user */
    listenfd = Open_listenfd(argv[1]);
    
    sbuf_init(&sbuf, SBUFSIZE);
    for(int i = 0; i < NTHREADS; i++){
        Pthread_create(&tid, NULL, thread, NULL);
    }

    /* Wiat for and eventually accept a connection */
    while(1){
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        sbuf_insert(&sbuf, connfd);  // insert connfd in buffer
    }
}

void *thread(void *vargp){
    Pthread_detach(pthread_self());
    while(1){
        int connfd = sbuf_remove(&sbuf);  // remove connfd from buf
        printf("connfd: %d\n", connfd);
        my_connect(connfd);
        Close(connfd);
    }
}

void my_connect(int connfd){
    int middleman;  // file descriptor
    char host[MAXLINE], port[MAXLINE], path[MAXLINE];
    // Rio to parse client request
    rio_t rio;

    parse_req(connfd, &rio, host, port, path);
   
}

int parse_req(int connection, rio_t *rio, char *host, char *port, char *path){
    size_t n;
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char rbuf[MAXLINE];
    // Strings to keep track of for URI parsing
    char *spec, *check;    // port specified?
    char *buf, *p, *save;  // used for explicit URI parse

    // Initialize rio
    Rio_readinitb(rio, connection);
    if(!Rio_readlineb(rio, rbuf, MAXLINE)){
        printf("bad request\n");
        return -1;
    }

    // Splice the request
    sscanf(rbuf, "%s %s %s", method, uri, version);
    /*********************/
    printf("the method is: %s\n", method);
    printf("the uri is: %s\n", uri);
    printf("the version is: %s\n", version);
    printf("\n");
    /*********************/

    if(strcmp(method, "GET")){  // Error: HTTP request isn't GET
        printf("Error: HTTP request isn't GET\n");
        return -1;
    } else if(!strstr(uri, "http://")){  // Error: HTTP prefix not found
        printf("Error: HTTP prefix not found\n");
        return -1;
    } else{  // parse URI
        buf = uri + 7;  // ignore 'http://'
        printf("the new buffer (host) is: %s\n", buf);
        spec = index(buf, ':');    // pointer to the first occurence of ':'
        check = rindex(buf, ':');  // pointer to the last occurrence of ':'
        
    }

    return -1;
}



/* Create an empty, bounded, shared FIFO buffer with n slots */
/* $begin sbuf_init */
void sbuf_init(sbuf_t *sp, int n)
{
    sp->buf = Calloc(n, sizeof(int)); 
    sp->n = n;                       /* Buffer holds max of n items */
    sp->front = sp->rear = 0;        /* Empty buffer iff front == rear */
    Sem_init(&sp->mutex, 0, 1);      /* Binary semaphore for locking */
    Sem_init(&sp->slots, 0, n);      /* Initially, buf has n empty slots */
    Sem_init(&sp->items, 0, 0);      /* Initially, buf has zero data items */
}
/* $end sbuf_init */

/* Clean up buffer sp */
/* $begin sbuf_deinit */
void sbuf_deinit(sbuf_t *sp)
{
    Free(sp->buf);
}
/* $end sbuf_deinit */

/* Insert item onto the rear of shared buffer sp */
/* $begin sbuf_insert */
void sbuf_insert(sbuf_t *sp, int item)
{
    P(&sp->slots);                          /* Wait for available slot */
    P(&sp->mutex);                          /* Lock the buffer */
    sp->buf[(++sp->rear)%(sp->n)] = item;   /* Insert the item */
    V(&sp->mutex);                          /* Unlock the buffer */
    V(&sp->items);                          /* Announce available item */
}
/* $end sbuf_insert */

/* Remove and return the first item from buffer sp */
/* $begin sbuf_remove */
int sbuf_remove(sbuf_t *sp)
{
    int item;
    P(&sp->items);                          /* Wait for available item */
    P(&sp->mutex);                          /* Lock the buffer */
    item = sp->buf[(++sp->front)%(sp->n)];  /* Remove the item */
    V(&sp->mutex);                          /* Unlock the buffer */
    V(&sp->slots);                          /* Announce available slot */
    return item;
}
/* $end sbuf_remove */
/* $end sbufc */

