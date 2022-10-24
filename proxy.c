#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define NTHREADS 64
sbuf_t sbuf;  // shared buffer of 

// TODO: remove threads because we don't need to keep track of all thread IDs
// read through the lecture slides

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36\r\n";

void *thread(void *vargp);
void my_connect(int connfd);
int parse_req(int connection, rio_t *rio, char *host, char *port, char *path);

int main(int argc, char **argv){
    int listenfd, *connfdp;
    pthread_t tid;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t *threads = Malloc(sizeof(pthread_t) * NTHREADS);

    // Check commandline arguments
    if(argc != 2){
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    // Listen on port specified by the user
    listenfd = Open_listenfd(argv[1]);

    // Create worker threads
    for(int i = 0; i < NTHREADS; i++){
        Pthread_create(&threads[i], NULL, thread, NULL);
    }

    // Wait for and eventually accept a connection
    while(1){
        clientlen = sizeof(struct sockaddr_storage);
        connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        // TODO: somehow pass the connfdp to a thread
    }
}

void *thread(void *vargp){
    Pthread_detach(pthread_self());  // allow immediate disposal of resources once the thread exits
    while(1){
        // TODO: get the connected file descriptor
    }
    return NULL;
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