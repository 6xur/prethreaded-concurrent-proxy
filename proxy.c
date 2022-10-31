#include <stdio.h>
#include "sbuf.h"
#include "sbuf.c"
#include "csapp.h"
#include "cache.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define NTHREADS 32
#define SBUFSIZE 32

sbuf_t sbuf;  // shared buffer of connection file descriptors

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_connection_hdr = "Proxy-Connection: close\r\n";
static const char *end_hdr = "\r\n";
static const char *default_port = "80";

/* Request handling functions */
void *thread(void *vargp);
void doit(int client);
int parse_req(int client, rio_t *rio, char *host, char *port, char *path);
void parse_uri(char *uri, char *host, char *port, char *path);
void forward_req(int server, int client, char *host, char *path);

/* Error handling functions */
void flush_str(char *str);
void flush_strs(char *str, char *str2, char *str3);

/* sbuf functions */
void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);

/* GLobal web cache */
cache *C;
pthread_rwlock_t lock;

/* 
 * main routine: accept connections and place them 
 * in the shared buffer for a worker thread to process
 */
int main(int argc, char **argv){
    int listenfd, client;
    pthread_t tid;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check commandline arguments */
    if(argc != 2){
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    /* Initialize read-write lock */
    pthread_rwlock_init(&lock, NULL);

    /* Initialize cache */
    C = Malloc(sizeof(struct web_cache));
    cache_init(C);

    /* Listen on port specified by the user */
    listenfd = Open_listenfd(argv[1]);
    
    /* Create a thread pool */
    sbuf_init(&sbuf, SBUFSIZE);
    for(int i = 0; i < NTHREADS; i++){
        Pthread_create(&tid, NULL, thread, NULL);
    }

    /* Wait for and eventually accept a connection */
    while(1){
        clientlen = sizeof(struct sockaddr_storage);
        client = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        sbuf_insert(&sbuf, client);
    }

    cache_free(C);

    /* Destory read-write lock */
    pthread_rwlock_destroy(&lock);
}

void *thread(void *vargp){
    Pthread_detach(pthread_self());
    while(1){
        int client = sbuf_remove(&sbuf);  // remove a client fd from the shared buffer
        printf("connfd: %d\n", client);
        doit(client);
        Close(client);
    }
}

/* Check for errors in the client request, parse the request,
 * forward the request to server, and finally forward server
 * response to client */
void doit(int client){
    int server;
    char host[MAXLINE], port[MAXLINE], path[MAXLINE];
    rio_t rio;

    /* Parse client request into host, port, and path */
    if(parse_req(client, &rio, host, port, path) < 0){
        fprintf(stderr, "ERROR: Cannot read this request path\n");
        flush_strs(host, port, path);
    }
    /* Parsing succeeded, continue */
    else{
        /* READING */
        pthread_rwlock_rdlock(&lock);
        line *lion = in_cache(C, host, path);
        pthread_rwlock_unlock(&lock);

        /* If in cache, don't connect to server */
        if(lion != NULL){
            if(rio_writen(client, lion->obj, lion->size) < 0){
                printf("ERROR: rio_writen error: bad connection\n");
            } else{
                printf("Using cache !!!!!!!!!!!!!!!!!!!!!!!!!!!!");
            }
            flush_strs(host, port, path);
        }
        
        /* Otherwise, connect to server & forward request */
        else{
            printf("NOt using cache???????????\n");
            if((server = Open_clientfd(host, port)) < 0){  // open connection to server
                fprintf(stderr, "ERROR: Could not establish connection to the server\n");
                flush_strs(host, port, path);
            } else{
                forward_req(server, client, host, path);
                /* Clean up & close connection to server */
                flush_strs(host, port, path);
                Close(server);
            }
        }
    }
}

void forward_req(int server, int client, char *host, char *path){
    /* Client-side reading */
    char buf[MAXLINE];

    /* Server-side reading */
    char svbuf[MAXLINE];
    rio_t respio;
    ssize_t m = 0;
    
    /* Web object cache */
    char object[MAX_OBJECT_SIZE];
    size_t obj_size = 0;

    /* -- BUILD & FORWARD REQUEST TO SERVER -- */
    sprintf(buf, "GET %s HTTP/1.0\r\n", path);
    sprintf(buf, "%sHost: %s\r\n", buf, host);
    sprintf(buf, "%s%s", buf, user_agent_hdr);
    sprintf(buf, "%s%s", buf, connection_hdr);
    sprintf(buf, "%s%s", buf, proxy_connection_hdr);
    sprintf(buf, "%s%s", buf, end_hdr);

    /* Forward request to server */
    if(rio_writen(server, buf, strlen(buf)) < 0){
        flush_str(buf);
        fprintf(stderr, "ERROR: writing to server failed");
        return;
    }

    printf("-------- Request forwarded to the server --------\n");
    printf("%s\n", buf);

    /* -- FORWARD SERVER RESPONSE TO CLIENT -- */
    Rio_readinitb(&respio, server);

    // TODO: using rio_readnb cause segmentation fault
    while((m = Rio_readlineb(&respio, svbuf, MAXLINE)) != 0){
        /* For caching */
        if((obj_size + m) <= MAX_OBJECT_SIZE){
            obj_size += m;
            sprintf(object, "%s%s", object, svbuf);
        }

        /* Write to client */
        Rio_writen(client, svbuf, m);
        flush_str(svbuf);
    }

    /* Object is not cached.
     * If it's small enough and not a error, cache it 
     */
     if(obj_size <= MAX_OBJECT_SIZE){
        pthread_rwlock_wrlock(&lock);
        add_line(C, make_line(host, path, object, obj_size));
        pthread_rwlock_unlock(&lock);
     }

    /* Clean up */
    flush_strs(buf, buf, svbuf);
    flush_strs(host, path, object);
}

int parse_req(int client, rio_t *rio, char *host, char *port, char *path){
    /* Parse request into method, uri, and version */
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char rbuf[MAXLINE];

    /* Initialize rio */
    Rio_readinitb(rio, client);

    /* Read the first line of the request */
    if(!Rio_readlineb(rio, rbuf, MAXLINE)){
        printf("ERROR: Bad request\n");
        return -1;
    }

    /* Splice the request */
    sscanf(rbuf, "%s %s %s", method, uri, version);
    /***************************************/
    printf("---------Parsing request-----------\n");
    printf("Method: %s\n", method);
    printf("URI: %s\n", uri);
    printf("Version: %s\n", version);
    /***************************************/

    if(strcasecmp(method, "GET")){       // Error: HTTP request isn't GET
        printf("ERROR: HTTP request isn't GET\n");
        return -1;
    } else if(!strstr(uri, "http://")){  // Error: 'http://' not found
        printf("ERROR: 'http://' not found\n");
        return -1;
    } else{  // parse URI
        /* Parse the URI, get hostname, path (if specified) and port*/
        parse_uri(uri, host, port, path);

        if(path[0] == '\0'){
                strcat(path, "/");
        }

        printf("-----Parsing URI-----\n");
        printf("Host: %s\n", host);
        printf("Port: %s\n", port);
        printf("Path: %s\n", path);
        printf("\n");

        return 0;
    }
}

void parse_uri(char *uri, char *host, char *port, char *path){
    /* Strings to keep track of URI parsing */
    char *spec, *check;    // port specified?
    char *buf, *p, *save;  // used for explicit URI parse

    buf = uri + 7;             // ignore 'http://'
    spec = index(buf, ':');    // pointer to the first occurence of ':'
    check = rindex(buf, ':');  // pointer to the last occurrence of ':'

    /* Cannot handle ":" after port */
    if(spec != check){
        printf("ERROR: Cannot handle ':' after port\n");
        return;
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
        strcpy(port, default_port);
    }
}

/********************
 * CLEAN-UP FUNCTIONS
*********************/
void flush_str(char *str){
    if(str){
        memset(str, 0, sizeof(str));
    }
}

void flush_strs(char *str1, char *str2, char *str3){
    if(str1) memset(str1, 0, sizeof(str1));
    if(str2) memset(str2, 0, sizeof(str2));
    if(str3) memset(str3, 0, sizeof(str3));
}