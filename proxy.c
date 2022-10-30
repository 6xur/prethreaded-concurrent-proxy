#include <stdio.h>
#include "sbuf.h"
#include "sbuf.c"
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define NTHREADS 32
#define SBUFSIZE 32

sbuf_t sbuf;  // shared buffer of connection file descriptors

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *pconn_hdr = "Proxy-Connection: close\r\n";
static const char *end_hdr = "\r\n";
static const char *default_port = "80";>

/* Request handling functions */
void *thread(void *vargp);
void connect_req(int client);
int parse_req(int client, rio_t *rio, char *host, char *port, char *path);
void parse_uri(char *uri, char *host, char *port, char *path);
void forward_req(int server, int client, rio_t *requio, char *host, char *path);

/* Error handling functions */
void flush_str(char *str);
void flush_strs(char *str, char *str2, char *str3);

/* sbuf functions */
void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);

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
}

void *thread(void *vargp){
    Pthread_detach(pthread_self());
    while(1){
        int client = sbuf_remove(&sbuf);  // remove a client fd from the shared buffer
        printf("connfd: %d\n", client);
        connect_req(client);
        Close(client);
    }
}

/* Check for errors in the client request, parse the request,
 * forward the request to server, and finally forward server
 * response to client */
void connect_req(int client){
    int server;  // connection to the server
    char host[MAXLINE], port[MAXLINE], path[MAXLINE];
    rio_t rio;

    /* Parse client request into host, port, and path */
    if(parse_req(client, &rio, host, port, path) < 0){
        fprintf(stderr, "ERROR: Cannot read this request path\n");
        flush_strs(host, port, path);
    }
    /* Parsing succeeded, continue */
    else{
        if((server = Open_clientfd(host, port)) < 0){  // open connection to server
            fprintf(stderr, "ERROR: Could not establish connection to the server\n");
            flush_strs(host, port, path);
        } else{
            forward_req(server, client, &rio, host, path);
            flush_strs(host, port, path);
            Close(server);
        }
    }
}

void forward_req(int server, int client, rio_t *requio, char *host, char *path){
    /* Client-side reading */
    char buf[MAXLINE];
    char cbuf[MAXLINE];
    ssize_t n = 0;

    /* Server-side reading */
    char svbuf[MAXLINE];
    rio_t respio;
    ssize_t m = 0;

    /* -- BUILD & FORWARD REQUEST TO SERVER -- */
    sprintf(buf, "GET %s HTTP/1.0\r\n", path);
    sprintf(buf, "%sHost: %s\r\n", buf, host);
    sprintf(buf, "%s%s", buf, user_agent_hdr);
    sprintf(buf, "%s%s", buf, conn_hdr);
    sprintf(buf, "%s%s", buf, pconn_hdr);
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

    while((m = Rio_readlineb(&respio, svbuf, MAXLINE)) != 0){
        if(rio_writen(client, svbuf, m) < 0){
            fprintf(stderr, "ERROR: Writing to client failed");
            return;
        }
    }

    /* Clean up */
    flush_strs(buf, cbuf, svbuf);
    flush_strs(host, path, path);
}

int parse_req(int client, rio_t *rio, char *host, char *port, char *path){
    /* Parse request into method, uri, and version */
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char rbuf[MAXLINE];

    /* Initialize rio */
    Rio_readinitb(rio, client);
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