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
static const char *web_port = "80";

/* Request handling functions */
void *thread(void *vargp);
void connect_req(int connfd);
int parse_req(int connection, rio_t *rio, char *host, char *port, char *path);

/* sbuf functions */
void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
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

    /* Wait for and eventually accept a connection */
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
        connect_req(connfd);
        Close(connfd);
    }
}

void connect_req(int connfd){
    int middleman;  // file descriptor
    char host[MAXLINE], port[MAXLINE], path[MAXLINE];
    rio_t rio;

    /* Parse client request into host, port, and path */
    parse_req(connfd, &rio, host, port, path);
   
}

int parse_req(int connection, rio_t *rio, char *host, char *port, char *path){
    /* Parse request into method, uri, and version */
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char rbuf[MAXLINE];
    /* Strings to keep track of URI parsing */
    char *spec, *check;    // port specified?
    char *buf, *p, *save;  // used for explicit URI parse

    /* Initialize rio */
    Rio_readinitb(rio, connection);
    if(!Rio_readlineb(rio, rbuf, MAXLINE)){
        printf("ERROR: Bad request\n");
        return -1;
    }

    /* Splice the request */
    sscanf(rbuf, "%s %s %s", method, uri, version);
    /***************************************/
    printf("Method: %s\n", method);
    printf("URI: %s\n", uri);
    printf("Version: %s\n", version);
    /***************************************/

    if(strcmp(method, "GET")){  // Error: HTTP request isn't GET
        printf("ERROR: HTTP request isn't GET\n");
        return -1;
    } else if(!strstr(uri, "http://")){  // Error: 'http://' not found
        printf("ERROR: 'http://' not found\n");
        return -1;
    } else{  // parse URI
        buf = uri + 7;  // ignore 'http://'
        spec = index(buf, ':');    // pointer to the first occurence of ':'
        check = rindex(buf, ':');  // pointer to the last occurrence of ':'

        /* Cannot handle ":" after port */
        if(spec != check){
            printf("ERROR: Cannot handle ':' after port\n");
            return -1;
        }

        /* Port is specified */
        if(spec){
            // Get host name
            p = strtok_r(buf, ":", &save);
            strcpy(host, p);

            // Get port from buf
            buf = strtok_r(NULL, ":", &save);
            p = strtok_r(buf, "/", &save);
            strcpy(port, p);

            // Get path
            while((p = strtok_r(NULL, "/", &save)) != NULL){
                strcat(path, "/");
                strcat(path, p);
            }
        }
        /* Port not specified */
        else{
            // Get host name
            p = strtok_r(buf, "/", &save);
            strcpy(host, p);

            // Get path
            while((p = strtok_r(NULL, "/", &save)) != NULL){
                strcat(path, "/");
                strcat(path, p);
            }

            // Set port as unspecified
            strcpy(port, web_port);
            
        }

        printf("-----Parsing URI-----\n");
        printf("Host: %s\n", host);
        printf("Port: %s\n", port);
        printf("Path: %s\n", path);

    }

    return -1;
}


/****************
 * sbuf functions
 ****************/

/* Create an empty, bounded, shared FIFO buffer with n slots */
void sbuf_init(sbuf_t *sp, int n){
    sp->buf = Calloc(n, sizeof(int)); 
    sp->n = n;                       /* Buffer holds max of n items */
    sp->front = sp->rear = 0;        /* Empty buffer iff front == rear */
    Sem_init(&sp->mutex, 0, 1);      /* Binary semaphore for locking */
    Sem_init(&sp->slots, 0, n);      /* Initially, buf has n empty slots */
    Sem_init(&sp->items, 0, 0);      /* Initially, buf has zero data items */
}

/* Clean up buffer sp */
void sbuf_deinit(sbuf_t *sp){
    Free(sp->buf);
}

/* Insert item onto the rear of shared buffer sp */
void sbuf_insert(sbuf_t *sp, int item){
    P(&sp->slots);                          /* Wait for available slot */
    P(&sp->mutex);                          /* Lock the buffer */
    sp->buf[(++sp->rear)%(sp->n)] = item;   /* Insert the item */
    V(&sp->mutex);                          /* Unlock the buffer */
    V(&sp->items);                          /* Announce available item */
}

/* Remove and return the first item from buffer sp */
int sbuf_remove(sbuf_t *sp){
    int item;
    P(&sp->items);                          /* Wait for available item */
    P(&sp->mutex);                          /* Lock the buffer */
    item = sp->buf[(++sp->front)%(sp->n)];  /* Remove the item */
    V(&sp->mutex);                          /* Unlock the buffer */
    V(&sp->slots);                          /* Announce available slot */
    return item;
}