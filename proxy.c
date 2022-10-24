#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36\r\n";

void *thread(void *vargp);
void my_connect(int connfd);
int parse_req(int connection, rio_t *rio, char *host, char *port, char *path);
void flush_str(char *str);

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

    my_connect(connfd);

    Close(connfd);
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
    char buf[MAXLINE];

    // Initialize rio
    Rio_readinitb(rio, connection);
    if(!Rio_readlineb(rio, buf, MAXLINE)){
        printf("bad request\n");
        return -1;
    }

    // Splice the request
    sscanf(buf, "%s %s %s", method, uri, version);
    /*********************/
    printf("the method is: %s\n", method);
    printf("the uri is: %s\n", uri);
    printf("the version is: %s\n", version);
    printf("\n");
    /*********************/

    if(strcmp(method, "GET")){  // Error: HTTP request isn't GET
        printf("HTTP request isn't get\n");
        return -1;
    } else if(!strstr(uri, "http://")){  // Error: HTTP prefix not found
        printf("HTTP prefix not found\n");
        return -1;
    } else{  // Parse URI

    }

    return -1;
}

void flush_str(char *str){
    if(str){
        memset(str, 0, sizeof(str));
    }
}