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

/* Global variables */
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

/* Global web cache */
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
        fprintf(stderr, "ERROR: usage: %s <port>\n", argv[0]);
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
        fprintf(stderr, "ERROR: cannot read this request path\n");
        flush_strs(host, port, path);
    }
    /* Parsing succeeded, continue */
    else{
        /* READING */
        pthread_rwlock_rdlock(&lock);
        line *lion = in_cache(C, host, path);
        pthread_rwlock_unlock(&lock);

        /* If the URL has been seen before and is in the cache,
         * do not connect to the server. Instead, we send the file
         * content directly from the cache
         */
        if(lion != NULL){
            printf("my_obj: %s\n", lion->obj);
            if(rio_writen(client, lion->obj, lion->size) < 0){
                fprintf(stderr, "ERROR: rio_writen error: bad connection\n");
            }
            printf("Debug: using cache\n");
            flush_strs(host, port, path);
        }
        
        /* Otherwise, it is a new request. We connect to server
         * and forward request
         */
        else{
            printf("Debug: no cache\n");
            if((server = open_clientfd(host, port)) < 0){  // open connection to server
                fprintf(stderr, "ERROR: could not establish connection to server\n");
                flush_strs(host, port, path);
            } else{
                forward_req(server, client, host, path);
                flush_strs(host, port, path);
                Close(server);
            }
        }
    }
}

/* Forward request to the server and back to the client. Store content
 * into the cache */
void forward_req(int server, int client, char *host, char *path){
    /* Client-side reading */
    char buf[MAXLINE];

    /* Server-side reading */
    char svbuf[MAXLINE];
    rio_t respio;
    ssize_t n = 0;
    
    /* Web object cache */
    char object[MAX_OBJECT_SIZE];
    size_t obj_size = 0;

    /* ---- BUILD & FORWARD REQUEST TO SERVER ---- */
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

    /* ---- FORWARD SERVER RESPONSE TO CLIENT ---- */
    Rio_readinitb(&respio, server);

    /* Read n bytes from the server. If the total number of bytes
     * read is less than or equal to MAX_OBJECT_SIZE, we save it
     * into the cache. 
     */
    while((n = rio_readlineb(&respio, svbuf, MAXLINE)) != 0){
        /* For caching */
        if((obj_size + n) <= MAX_OBJECT_SIZE){
            memcpy(object + obj_size, svbuf, n);
            obj_size += n;
        }

        /* Write to client */
        Rio_writen(client, svbuf, n);
        flush_str(svbuf);
    }

    /* Object is not cached, if the total bytes read is less
     * small enough, we store it into the cache
     */
     if(obj_size <= MAX_OBJECT_SIZE && !is_error(object)){
        pthread_rwlock_wrlock(&lock);
        add_line(C, make_line(host, path, object, obj_size));
        pthread_rwlock_unlock(&lock);
     }
}


/* Determine if obj is a server error or not
 * 1 if it is, 0 if it isn't*/
int is_error(char *obj){
    size_t obj_size = strlen(obj) + 1;
    char object[obj_size];
    memset(object, 0, sizeof(object));
    memcpy(object, obj, obj_size);
    return strstr(object, "200") == NULL ? 1 : 0;
}

int parse_req(int client, rio_t *rio, char *host, char *port, char *path){
    /* Parse request into method, uri, and version */
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    
    /* Initialize rio */
    Rio_readinitb(rio, client);

    /* Read the first line of the request */
    if(!Rio_readlineb(rio, buf, MAXLINE)){
        fprintf(stderr, "ERROR: bad request\n");
        return -1;
    }

    /* Splice the client request */
    sscanf(buf, "%s %s %s", method, uri, version);

    /***************************************/
    printf("---------Parsing request-----------\n");
    printf("Method: %s\n", method);
    printf("URI: %s\n", uri);
    printf("Version: %s\n", version);
    printf("\n");
    /***************************************/

    if(strcasecmp(method, "GET")){
        fprintf(stderr, "ERROR: proxy does not implement this method\n");
        return -1;
    }
    
    /* Parse the URI to get hostname, path and port*/
    parse_uri(uri, host, port, path);

    /*****************************************/
    printf("_________Parsing URI_________\n");
    printf("Host: %s\n", host);
    printf("Port: %s\n", port);
    printf("Path: %s\n", path);
    printf("\n");
    /*****************************************/

    return 0;
}

void parse_uri(char *uri, char *host, char *port, char *path){
    char *host_ptr, *path_ptr, *port_ptr;
	char hostname[MAXLINE];

    /* Ignore "http://" */
	host_ptr = uri + strlen("http://");

    if((path_ptr = strchr(host_ptr, '/')) == NULL){  // path is empty
        strcpy(path, "/");
		strcpy(hostname, host_ptr);
    } else{
        strcpy(path, path_ptr);
		strncpy(hostname, host_ptr, (path_ptr - host_ptr));
		hostname[path_ptr - host_ptr] = '\0';
    }

	if((port_ptr = strchr(hostname, ':')) == NULL){  // port undefined
		strcpy(host, hostname);
		strcpy(port, default_port);
	}else{
        strcpy(port, port_ptr + 1);
		strncpy(host, hostname, (port_ptr - hostname));
		host[port_ptr - hostname] = '\0';
	}
}

/********************
 * CLEAN-UP FUNCTIONS
 ********************/
void flush_str(char *str){
    if(str){
        memset(str, 0, sizeof(str));
    }
}

void flush_strs(char *str1, char *str2, char *str3){
    if(str1) flush_str(str1);
    if(str2) flush_str(str2);
    if(str3) flush_str(str3);
}