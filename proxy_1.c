#include "csapp.h"
#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";


void parse_uri(char *uri, char *host, char* port, char *path);
void doit(int fd) ;
void *thread(void *vargp);


int main(int argc, char **argv) {
  //printf("%s", user_agent_hdr);
  //return 0;
  int listenfd, *connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage client_addr;
  pthread_t tid;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(client_addr);
    connfd = Malloc(sizeof(int));
    *connfd = Accept(listenfd, (SA * ) &client_addr, &clientlen);
    Pthread_create(&tid, NULL, thread, connfd);
  
  }
  printf("%s", user_agent_hdr);
  return 0;
}

void *thread(void *vargp)
{
  int connfd = *((int *)vargp);
  Pthread_detach(pthread_self());
  Free(vargp);
  doit(connfd);
  Close(connfd);
  return NULL;
}


void doit(int fd) {
  int finalfd; // proxy 서버가 client로서 최종 서버에게 보내는 소켓 식별자
  char client_buf[MAXLINE], server_buf[MAXLINE];

  char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char host[MAXLINE], port[MAXLINE], path[MAXLINE];
  rio_t client_rio, server_rio;

  // 1. 클라이언트로부터 요청을 수신한다 : client - proxy(server 역할)
  Rio_readinitb(&client_rio, fd);
  Rio_readlineb(&client_rio, client_buf, MAXLINE);      // 클라이언트 요청 읽고 파싱
  sscanf(client_buf, "%s %s %s", method, uri, version);
  printf("===FROM CLIENT===\n");
  printf("Request headers:\n");
  printf("%s", client_buf);
  
  if (strcasecmp(method, "GET")) {  // GET 요청만 처리한다
    printf("Proxy does not implement the method\n");
    return; 
  }

  // 2. 요청에서 목적지 서버의 호스트 및 포트 정보를 추출한다
  parse_uri(uri, host, port, path);
  
  // Proxy 서버: client 서버로서의 역할 수행
  // 3. 추출한 호스트 및 포트 정보를 사용하여 목적지 서버로 요청을 날린다
  finalfd = Open_clientfd(host, port);   // clientfd = proxy가 client로서 목적지 서버로 보낼 소켓 식별자
  sprintf(server_buf, "%s %s %s\r\n", method, path, version);
  printf("===TO SERVER===\n");
  printf("%s\n", server_buf);
  
  Rio_readinitb(&server_rio, finalfd);
  sprintf(server_buf, "GET %s HTTP/1.0\r\n\r\n", path); // 클라이언트로부터 받은 요청의 헤더를 수정하여 보냄
  Rio_writen(finalfd, server_buf, strlen(server_buf));

  // 4. 서버로부터 응답을 읽어 클라이언트에 반환한다
  size_t n;
  while((n = Rio_readlineb(&server_rio, server_buf, MAXLINE)) != 0) {
    printf("Proxy received %d bytes from server and sent to client\n", n);
    Rio_writen(fd, server_buf, n);
  }
  Close(finalfd);
}


void parse_uri(char *uri, char *hostname, char *port, char *pathname) {
    // 1. "http://" 접두어를 파싱합니다.
    // http://52.79.166.31:3001/home.html 
    char *start = strstr(uri, "http://");
    char *port_end;
    if (start == NULL) {
        return -1;
    }
    start += strlen("http://");
    // 52.79.166.31(p):(end)3001(port_end)/home.html

    // 2. 호스트 이름을 파싱합니다.
    char *end = strstr(start, ":");
    if (end == NULL) {
        end = strstr(start, "/");
        if (end == NULL) {
            return -1;
        }
    }
    strncpy(hostname, start, end - start);
    // 52.79.166.31
    // printf("====hostname %s\n",hostname);

    // 3. 포트 번호를 파싱합니다.
    if (*end == ':') {
        end++;
        char *port_end = strstr(end, "/");
        if (port_end == NULL) {
            return -1;
        }
        strncpy(port, end, port_end - end);
        strcpy(pathname, port_end);

    } else {

        strcpy(port, "80");
        strcpy(pathname,end);

    }



    return 0;
}