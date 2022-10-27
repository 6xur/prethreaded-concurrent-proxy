#include <stdio.h>
#include "sbuf.h"
#include "sbuf.c"
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define NTHREADS 4
#define SBUFSIZE 16

sbuf_t sbuf;  // shared buffer of connected descriptors

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36\r\n";

static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *pconn_hdr = "Proxy-Connection: close\r\n";
static const char *end_hdr = "\r\n";

static const char *web_port = "80";

/* Request handling functions */
void *thread(void *vargp);
void connect_req(int connfd);
int parse_req(int connection, rio_t *rio, char *host, char *port, char *path);
void forward_req(int server, int client, rio_t *requio, char *host, char *path);
int ignore_hdr(char *hdr);

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
    
    /* Create a thread pool */
    sbuf_init(&sbuf, SBUFSIZE);
    for(int i = 0; i < NTHREADS; i++){
        Pthread_create(&tid, NULL, thread, NULL);
    }

    /* Wait for and eventually accept a connection */
    while(1){
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        sbuf_insert(&sbuf, connfd);  // insert the connection fd in buffer
    }
}

void *thread(void *vargp){
    Pthread_detach(pthread_self());
    while(1){
        int connfd = sbuf_remove(&sbuf);  // remove a connected fd from the shared buffer
        printf("connfd: %d\n", connfd);
        connect_req(connfd);
        Close(connfd);
    }
}

void connect_req(int connfd){
    int middleman;  // connection to the server
    char host[MAXLINE], port[MAXLINE], path[MAXLINE];
    rio_t rio;

    /* Parse client request into host, port, and path */
    if(parse_req(connfd, &rio, host, port, path) < 0){
        fprintf(stderr, "ERROR: Parsing failed...\n");
    } 
    /* Parsing succeeded, continue */
    else{
        if((middleman = Open_clientfd(host, port)) < 0){  // open connection to server
            printf("ERROR: Could not establish connection to the server\n");
        } else{
            printf("Debug: Successfully established connection with server\n");
            forward_req(middleman, connfd, &rio, host, path);
            printf("Debug: This should run\n");
            Close(middleman);
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
    /* Build client headers */
    while((n = rio_readlineb(requio, cbuf, MAXLINE)) != 0){
        if(!strcmp(cbuf, "\r\n")){
            printf("Breaking\n");
            break;  // empty line found => end of headers
        }
        if(!ignore_hdr(cbuf)){
            printf("Not ignoring\n");
            sprintf(buf, "%s%s\r\n", buf, cbuf);
        }
    }
    /* Build proxy headers */
    sprintf(buf, "%sHost: %s\r\n", buf, host);
    sprintf(buf, "%s%s", buf, user_agent_hdr);
    sprintf(buf, "%s%s", buf, accept_hdr);
    sprintf(buf, "%s%s", buf, accept_encoding_hdr);
    sprintf(buf, "%s%s", buf, conn_hdr);
    sprintf(buf, "%s%s", buf, pconn_hdr);
    sprintf(buf, "%s%s", buf, end_hdr);
    /* Forward request to server */
    if(rio_writen(server, buf, strlen(buf)) < 0){
        printf("ERROR: rio_writen failed");
        return;
    }

    printf("%s\n", buf);

    /* -- BUILD & FORWARD SERVER RESPONSE TO CLIENT -- */
    /* Initialize rio to read server's response */
    Rio_readinitb(&respio, server);
    printf("Debug: before forward to client\n");
    /* Read from fd[server] and write to fd[client] */

    // TODO: Uncomment the code block below causes 159 to not execute
    while((m = Rio_readlineb(&respio, svbuf, MAXLINE)) != 0){
        // RIO error check
        if(m < 0){
            printf("ERROR: RIO error");
            return;
        }

        // Write to client
        if(rio_writen(client, svbuf, m) < 0){
            printf("ERROR: Writing to client error");
            return;
        }
    }

    printf("Debug: after forward to client\n");


}

int ignore_hdr(char *hdr){
    /* Ignore header for Proxy-Connection */
    if(strstr(hdr, "Proxy-Connection")){
        return 1;  // ignore
    } else if(strstr(hdr, "Connection")){  // ignore header for connection
        return 1;  // ignore
    } else if(strcmp(hdr, "\r\n")){  // ignore empty line for client headers
        return 1;  // ignore
    } else{  // everything else ia acceptable
        return 0;
    }
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