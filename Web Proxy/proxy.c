#include "cache.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define MAX_PROT_LEN 8
#define MAX_HOSTNAME_LEN 32
#define MAX_PORT_LEN 8
#define MAX_PATH_LEN 32
#define MAX_QUERY_LEN 64
#define MAX_FRAG_LEN 64

#define MAX_HEADER_KEY_LEN 32
#define MAX_HEADER_VAL_LEN 128

// static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
// static const char *connection = "Connection: close\r\n";
// static const char *proxy_connection = "Proxy-Connection: close\r\n";

/* First line of HTTP request */
typedef struct {
    char protocol[MAX_PROT_LEN];
    char hostname[MAX_HOSTNAME_LEN];
    char port[MAX_PORT_LEN];
    char path[MAX_PATH_LEN];
    char query[MAX_QUERY_LEN];
    char fragment[MAX_FRAG_LEN];
} reqLine;

/* One header pair of HTTP request */
typedef struct {
    char headerKey[MAX_HEADER_KEY_LEN];
    char headerVal[MAX_HEADER_VAL_LEN];
} reqHeader;

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void doit(int fd);
reqHeader parseHeader(char *line);
void parseReq(int fd, reqLine *reqline, reqHeader *reqheader, int *numHeader, char *uri);
void parseURI(char *uri, reqLine *reqline);
int send2Server(reqLine *reqline, reqHeader *reqheader, int numHeader);
void *thread(void *vargp);

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    signal(SIGPIPE, SIG_IGN);
    
    init_cache();

    int listenfd = Open_listenfd(argv[1]);
    struct sockaddr_storage clientaddr;
    char hostname[MAXLINE], port[MAXLINE];
    pthread_t tid;
    int *connfd;
    while (1) {
        socklen_t clientlen = sizeof(clientaddr);
        connfd = Malloc(sizeof(int));
        *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        Pthread_create(&tid, NULL, thread, connfd);
    }
    free_cache();
    return 0;
}

void *thread(void *vargp) {
    int connfd = *((int*)vargp);
    Pthread_detach(pthread_self()); // Reaped automatically by kernel when it terminates
    Free(vargp);
    doit(connfd);
    Close(connfd);
    return NULL;
}

void parseReq(int fd, reqLine *reqline, reqHeader *reqheader, int *numHeader, char *uri) {
    rio_t rio;
    // uri[MAXLINE], 
    char buf[MAXLINE], method[MAXLINE], version[MAXLINE];

    Rio_readinitb(&rio, fd);
    if (!Rio_readlineb(&rio, buf, MAXLINE)) {
        return;
    }
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);

/*    if (strcasecmp(method, "GET")) {
        clienterror(fd, method, "501", "Not Implemented", "TODO");
        return;
    }*/

    parseURI(uri, reqline);

    int hasHost = 0, hasUserAG = 0, hasConnect = 0, hasProConnect = 0;
    *numHeader = 0;
    // read the request header carried by the browser
    Rio_readlineb(&rio, buf, MAXLINE);
    while (strcmp(buf, "\r\n")) {
        if (!strncmp(buf, "Host:", 4)) {
            hasHost = 1;
        }
        if (!strncmp(buf, "User-Agent:", 10)) {
            hasUserAG = 1;
        } 
        if (!strncmp(buf, "Connection:", 10)) {
            hasConnect = 1;
        }
        if (!strncmp(buf, "Proxy-Connection:", 16)) {
            hasProConnect = 1;
        } 
//printf("reqheader %s\n", buf);
        reqheader[(*numHeader)++] = parseHeader(buf);
        Rio_readlineb(&rio, buf, MAXLINE);
    }

    if (hasHost == 0) {
        strcpy(reqheader[(*numHeader)].headerKey, "Host");
        strcpy(buf, reqline->hostname);
        strcpy(reqheader[(*numHeader)++].headerVal, strcat(buf, "\r\n"));
    }
    if (hasUserAG == 0) {
        strcpy(reqheader[(*numHeader)].headerKey, "User-agent");
        strcpy(reqheader[(*numHeader)++].headerVal, "Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n");
    }
 
    if (hasConnect == 0) {
        strcpy(reqheader[(*numHeader)].headerKey, "Connection");
        strcpy(reqheader[(*numHeader)++].headerVal, "close\r\n");
    }
    
    if (hasProConnect == 0) {
        strcpy(reqheader[(*numHeader)].headerKey, "Proxy-Connection");
        strcpy(reqheader[(*numHeader)++].headerVal, "close\r\n");
    }
}

/* Parse URI into reqline: hostname, port, path */
void parseURI(char *uri, reqLine *reqline) {
    if (uri == NULL || reqline == NULL) {
        return;
    }

    const char *p2 =  uri;
    const char *p1 = strchr(p2, ':');
    if (p1 == NULL) {
        fprintf(stderr, "Error: Invalid URI %s\n", uri);
        return;
    }
    int len = p1 - p2;
    for (int i = 0; i < len; ++i) {
        if (!isalpha(p2[i])) {
            return;
        }
    }
    memcpy(reqline->protocol, p2, len);
    ++p1;
    p2 = p1;
    for (int i = 0; i < 2; ++i) {
        if (*p2 != '/') {
            return;
        }
        ++p2;
    }

    p1 = strchr(p2, ':');
    // default port
    if (p1 == NULL) {
        p1 = strchr(p2, '/');
        if (p1 == NULL) {
            return;
        }
        memcpy(reqline->hostname, p2, p1 - p2);
        strcpy(reqline->port, "80");
    }
    else {
        memcpy(reqline->hostname, p2, p1 - p2);
        ++p1;
        p2 = p1;
        p1 = strchr(p2, '/');
        if (p1 == NULL) {
            return;
        }
        memcpy(reqline->port, p2, p1 - p2);
    }

    // for convenience, I count '/' as part of path, but not for query and fragment
    p2 = p1;
    while (*p1 != '\0' && *p1 != '?' && *p1 != '#') {
        ++p1;
    }
    memcpy(reqline->path, p2, p1 - p2);
    p2 = p1;
    if (*p1 == '?') {
        ++p2;
        p1 = p2;
        while (*p1 != '\0' && *p1 != '#') {
            ++p1;
        }
        memcpy(reqline->query, p2, p1 - p2);
    }
    if (*p1 == '#') {
        ++p1;
        p2 = p1;
        while (*p1 != '\0') {
            ++p1;
        }
        memcpy(reqline->fragment, p2, p1 - p2);
    }
}

/* Return one pair of header */
reqHeader parseHeader(char *buf) {
    reqHeader header;

    char *end = strstr(buf, ": ");
    *end = '\0';
    strcpy(header.headerKey, buf);
    strcpy(header.headerVal, end + 2);
    *end = ':';
    return header;
}

/* read and parse req, send to server, read from server, send to client */
void doit(int fd) {
    reqLine reqline;
    reqHeader reqheader[32];
    int numHeader;
    char uri[MAXLINE];

    parseReq(fd, &reqline, reqheader, &numHeader, uri);

/*
for (int i = 0; i < numHeader; ++i) {
    printf("%s %s\n", reqheader[i].headerKey, reqheader[i].headerVal);
}*/

    int cacheHit = find_cache(uri, fd);
    if (cacheHit == 0) {
        return;
    }

    // Let the proxy send the correct request
    int connfd = send2Server(&reqline, reqheader, numHeader);

    rio_t rio;
    Rio_readinitb(&rio, connfd);

    int size = 0;
    char buf[MAXLINE];
    char obj[MAX_OBJECT_SIZE];
    int objSize = 0;

    while ((size = Rio_readlineb(&rio, buf, MAXLINE)) > 0) {
        Rio_writen(fd, buf, size);
        printf("receive %d bytes\n", size);
        objSize += size;
        if (objSize <= MAX_OBJECT_SIZE) {
            memcpy(obj + objSize - size, buf, size);
        }
    }

    printf("%d\n", objSize);

    // write into cache
    if (objSize <= MAX_OBJECT_SIZE)
        write_cache(uri, obj, objSize);

    Close(connfd);
}

/* Return the connection fd between proxy and server for the sake of read response */
int send2Server(reqLine *reqline, reqHeader *reqheader, int numHeader) {
    int clientfd = Open_clientfd(reqline->hostname, reqline->port);
    char buf[MAXLINE], *p = buf;
    
    sprintf(buf, "GET %s HTTP/1.0\r\n", reqline->path);
    p = buf + strlen(buf);
    for (int i = 0; i < numHeader; ++i) {
        sprintf(p, "%s: %s\r\n", reqheader[i].headerKey, reqheader[i].headerVal);
        p = buf + strlen(buf);
    }

    sprintf(p, "\r\n");
    Rio_writen(clientfd, buf, strlen(buf));
    return clientfd;
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Tiny Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}

