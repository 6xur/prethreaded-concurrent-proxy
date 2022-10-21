#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36\r\n";

void my_connect(int connfd);
void *thread(void *vargp);

// Listen for client requests and create a new thread for each client
int main(int argc, char **argv)
{
    int listenfd, *connfdp;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    // Check commandline arguments
    if(argc != 2){
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    // Listen on port specified by the user
    listenfd = Open_listenfd(argv[1]);

    while(1){
        // Wait for and eventually accept an incoming connection
        clientlen = sizeof(struct sockaddr_storage);
        connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        // Create a new thread for each client
        Pthread_create(&tid, NULL, thread, connfdp);
    }
}

// Thread connects to the client on [vargp] and forward request
void *thread(void *vargp){
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());  // allow immediate disposal of resources once the thread exits
    Free(vargp);

    printf("Thread %d created\n", connfd);

    my_connect(connfd);

    Close(connfd);
    return NULL;
}

void my_connect(int connfd){
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    // TODO: simply echo every request at the moment
    Rio_readinitb(&rio, connfd);
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
        printf("Proxy server received %d bytes\n", (int)n);
        Rio_writen(connfd, buf, n);
    }
}