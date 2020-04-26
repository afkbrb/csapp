/**
 * Concurrent version.
 */

#include <stdio.h>

#include "csapp.h"

/********************************* request ***********************************/
#define MAXREQ 8192

typedef struct {
    char host[MAXLINE];
    char port[MAXLINE];
    char path[MAXLINE];
    char *headers;  // the "Host", "User-Agent", "Connection" and "Proxy-Connection" headers are not included
} request_t;

request_t *request_parse(int clientfd);
int request_send(request_t *requestp);
void request_free(request_t *requestp);

/********************************* proxy ***********************************/

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_connection_hdr = "Proxy-Connection: close\r\n";

void *thread(void *vargp);
void do_proxy(int clientfd);
void forward_response(int serverfd, int clientfd);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv) {
    int listenfd, *clientfdp;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  // Enough space for any address
    char client_hostname[MAXLINE], client_port[MAXLINE];
    pthread_t tid;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        clientfdp = (int *)Malloc(sizeof(int));
        *clientfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        Pthread_create(&tid, NULL, thread, clientfdp);
    }
}

void *thread(void *vargp) {
    int clientfd = *((int *)vargp);
    Free(vargp);
    Pthread_detach(pthread_self());
    do_proxy(clientfd);
    Close(clientfd);
    return NULL;
}

/**
 * Send a request to the server on behalf of the client,
 * and then forward the response to the client.
 */
void do_proxy(int clientfd) {
    request_t *requestp;
    if ((requestp = request_parse(clientfd)) == NULL) return;
    int serverfd = request_send(requestp);
    forward_response(serverfd, clientfd);
    request_free(requestp);
    Close(serverfd);
}

/**
 * Parse host, port and path from uri.
 */
static void parse_uri(request_t *requestp, char *uri) {
    strcpy(requestp->port, "80");  // default port
    uri += strlen("http://");

    strcpy(requestp->host, uri);
    char *pos = strstr(requestp->host, "/");
    strcpy(requestp->path, pos);
    *pos = '\0';

    char *pos1 = strstr(requestp->host, ":");
    if (pos1) {  // host:port
        strcpy(requestp->port, pos1 + 1);
        *pos1 = '\0';
    }

    // printf("host: %s, port: %s, path: %s\n", requestp->host, requestp->port, requestp->path);
}

/**
 * The "Host", "User-Agent", "Connection" and "Proxy-Connection" headers are ignored.
 */
static char *parse_headers(rio_t *rp) {
    char buf[MAXLINE];
    char *headers = (char *)malloc(MAXREQ * sizeof(char));
    *headers = '\0';
    while (1) {
        Rio_readlineb(rp, buf, MAXLINE);
        if (strcmp(buf, "\r\n")) break;
        if (strstr(buf, "Host:") != buf && strstr(buf, "User-Agent:") != buf && strstr(buf, "Connection:") != buf && strstr(buf, "Proxy-Connection:") != buf) {
            strcat(headers, buf);
        }
    }

    return headers;
}

/**
 * Read and parse the request from client.
 * The request contains server host, the server port, request path and headers.
 * 
 * Return the request pointer.
 * Return NULL if an error occurs.
 */
request_t *request_parse(int clientfd) {
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    rio_t rio;
    request_t *requestp;

    Rio_readinitb(&rio, clientfd);
    if (!Rio_readlineb(&rio, buf, MAXLINE)) return NULL;  // read the request line
    sscanf(buf, "%s %s %s", method, uri, version);        // parse request line
    if (strcasecmp(method, "GET")) {                      // only support GET method
        clienterror(clientfd, method, "501", "Not Implemented", "The proxy does not implement this method");
        return NULL;
    }

    requestp = (request_t *)Malloc(sizeof(request_t));
    parse_uri(requestp, uri);
    requestp->headers = parse_headers(&rio);

    return requestp;
}

/**
 * Send the content of the request to the server.
 * 
 * Return the server fd.
 */
int request_send(request_t *requestp) {
    char buf[MAXLINE], host[MAXLINE], msg[MAXREQ];
    int serverfd = Open_clientfd(requestp->host, requestp->port);

    msg[0] = '\0';
    sprintf(buf, "GET %s HTTP/1.0\r\n", requestp->path);
    strcat(msg, buf);

    strcpy(host, requestp->host);
    if (strcmp(requestp->port, "80")) {  // append the port to the host if it's not "80"
        strcat(host, ":");
        strcat(host, requestp->port);
    }
    sprintf(buf, "Host: %s\r\n", host);
    strcat(msg, buf);

    strcat(msg, user_agent_hdr);
    strcat(msg, connection_hdr);
    strcat(msg, proxy_connection_hdr);
    strcat(msg, requestp->headers);
    strcat(msg, "\r\n");

    printf("proxy sending the following request to server:\n%s", msg);
    Rio_writen(serverfd, msg, strlen(msg));

    return serverfd;
}

void request_free(request_t *requestp) {
    free(requestp->headers);
    free(requestp);
}

/**
 * Forward the response from the server to the client.
 */
void forward_response(int serverfd, int clientfd) {
    char buf[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, serverfd);
    int n;
    while ((n = Rio_readnb(&rio, buf, MAXLINE))) {
        Rio_writen(clientfd, buf, n);
    }
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
    sprintf(buf, "<body bgcolor=ffffff>\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}
