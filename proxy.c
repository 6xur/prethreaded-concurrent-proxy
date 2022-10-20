#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36\r\n";

// Listen for client requests and create a new thread for each client
int main()
{
    printf("%s", user_agent_hdr);

    // Check commandline arguments


    while(1){
        int listenfd, *connfdp;
        struct sockaddr_storage client_addr;
        socklen_t clientlen;
        
        // Wait for and eventually accept an incoming connection
        printf("Waiting for connection...\n");
    }

    return 0;
}
