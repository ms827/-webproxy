#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/*
캐시 버퍼 구조체 생성
*/
typedef struct cache_node{
    char cache_uri[MAXLINE]; // 웹 객체의 path
    char cache_obj[MAX_OBJECT_SIZE]; // 웹 객체 데이터
    struct cache_node* prev; // 이전 항목의 포인터
    struct cache_node* next; // 다음 항목의 포인터
    size_t size; // 웹 객체 크기
}cache_node;

cache_node *cachehead = NULL;

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
void doit(int fd);
void parse_uri(char *uri, char *hostname, char *port, char *pathname);
void modify_HTTP_header(char *method, char *host, char *port, char *path, int server_fd);
void *thread(void *vargp);
void remove_cache();
cache_node *find_cache(char *uri);
void add_cache(int object_size, char *from_server_uri, char *from_server_data);

int cachesize = 0;

/*
doit: 클라이언트와 목적지 서버의 통신을 담당
*/
void doit(int fd) {
  int server_fd;
  char method[MAXLINE], uri[MAXLINE], version[MAXLINE], path[MAXLINE];
  char host[MAXLINE], port[MAXLINE];
  char server_buf[MAXLINE], from_client_buf[MAXLINE], buf[MAXLINE];
  char from_server_uri[MAXLINE], from_server_data[MAX_OBJECT_SIZE];
  rio_t client_rio, server_rio;
  
  Rio_readinitb(&client_rio, fd);                               // connfd를 연결하여 rio에 저장   
  Rio_readlineb(&client_rio, from_client_buf, MAXLINE);         // rio에 있는 string 한 줄을 모두 buffer에 옮긴다. 
  printf("=====from client=====\n");
  printf("Proxy Request headers:\n");
  sscanf(from_client_buf, "%s %s %s", method, uri, version);    // 읽어들인 HTTP method, uri, version 등을 파싱합니다.
  printf("%s\n", from_client_buf);
  if (strcasecmp(method,"GET")) {                               // GET메소드가 아니라면 통과 
        printf("Proxy does not implement the method\n");
        return;
  } 
  
  parse_uri(uri, host, port, path);                     // 서버의 host, port, path 추출

  char *buffer = find_cache(uri);                       // 캐시에 요청한 객체가 있는지 확인
  if (buffer != NULL)                                   // 요청한 객체가 있을경우
  {
    Rio_writen(fd, buffer, MAXLINE);                    // 프록시에서 해당 객체를 클라이언트에 보낸다.
  }
  else                                                  // 캐시에 요청한 객체가 없을경우
  {
    server_fd = Open_clientfd(host, port);              // proxy와 메인서버 소켓의 파일 디스크립터 생성
    Rio_readinitb(&server_rio, server_fd);              // 서버와 연결
    modify_HTTP_header(method, host, port, path, server_fd);    // 메인 서버로 보낼 헤더 생성 및 전송
    ssize_t n;
    while ((n = Rio_readlineb(&server_rio, server_buf, MAXLINE)) > 0) // 서버로부터 전송된 데이터 읽기
    {
      Rio_writen(fd, server_buf, n);                    // 실제 읽은 바이트 수(데이터 길이)만큼 데이터 클라이언트로 전송
      cachesize += n;
      if(cachesize < MAX_OBJECT_SIZE)
      {
        strcat(buf, server_buf);
      }    
    }
    printf("cachesize: %d\n", cachesize);
    Close(server_fd);
    add_cache(strlen(buf), uri, buf);                   // 캐시에 객체를 추가
  }
}

/*
parse_server : 서버로부터 받은 응답 uri 본문으로 파싱
*/
void parse_server(char *buf, char *from_server_uri, char *from_server_data)
{
  char *start_url = strstr(buf, "\r\n");
  strncpy(from_server_uri, buf, start_url - buf);
  strcpy(from_server_data,start_url+2);
  return;
}

/*
remove_cache : 캐시에서 제일 오래된 데이터를 지워준다(연결리스트의 꼬리)
*/
void remove_cache()
{
  cache_node *LRUcache = cachehead; // 새로 생성한 LRUcache노드는 NULL
  while(LRUcache->next != NULL)
  {
    LRUcache = LRUcache->next;
  }
  LRUcache->prev->next = NULL;
  free(LRUcache);
  cachesize -= LRUcache->size; // 캐시에서 빠져나간 객체의 크기만큼 빼준다.
}

/*
클라이언트가 요청한 객체가 캐시에 있는지 확인하고 있다면 응답 데이터 반환, 요청 객체를 가장 최근 리스트로 변경
*/
cache_node *find_cache(char *uri)
{
  cache_node *current = cachehead;
  while(current != NULL)
  {
    if(strcmp(current->cache_uri, uri) == 0)
    {
      if(current->prev != NULL) // 해당 객체가 최근 객체가 아니라면, 최근 리스트로 변경
      {
        printf("같은 객체 하지만 처음은 아닌 찾음\n");
        current->prev->next = current->next;
        if(current->next != NULL)
        {
          current->next->prev = current->prev;
        }
        current->prev = NULL;
        current->next = cachehead;
        cachehead->prev = current;
        cachehead = current;
      }
      printf("같은 객체면서 첫번째 객체 찾음: %d\n", current->cache_obj);
      return current->cache_obj;
    }
    current = current->next;
  }
  return NULL;
}

/*
add_cache : 최종 서버로부터 받은 객체를 캐시에 저장
*/
void add_cache(int object_size, char *from_server_uri, char *from_server_data)
{
  
  if (cachesize >= MAX_CACHE_SIZE) {
    remove_cache();
  }
  
  cache_node *new_cache = malloc(sizeof(cache_node));
  strcpy(new_cache->cache_uri, from_server_uri);
  strcpy(new_cache->cache_obj, from_server_data);
  new_cache->size = object_size;
  
  new_cache->prev = NULL;
  new_cache->next = cachehead;
  if (cachehead != NULL) {
    cachehead->prev = new_cache;
  }
  cachehead = new_cache;
}

/*
modify_HTTP_header : 메인 서버에게 보낼 헤더 생성 및 전송
*/
void modify_HTTP_header(char *method, char *host, char *port, char *path, int server_fd)
{
  char buf[MAXLINE];

  sprintf(buf, "%s %s %s\r\n", method, path, "HTTP/1.0");
  sprintf(buf, "%sHost: %s\r\n", buf, host);
  sprintf(buf, "%s%s", buf, user_agent_hdr);
  sprintf(buf, "%sConnection: %s\r\n", buf, "close");
  sprintf(buf, "%sProxy-Connection: %s\r\n\r\n", buf, "close");
  Rio_writen(server_fd, buf, strlen(buf));
  return;
}


/*
 * parse_uri : URI를 파싱하여 호스트 이름, 포트 번호, 파일 경로 등의 정보를 추출하는 함수
 *             파싱된 정보는 인자로 전달된 세 개의 문자열에 저장됩니다.
 *             인자로 전달된 uri 문자열은 "[http://]<hostname>[:<port>]/<pathname>"의 형식을 따릅니다.
 *             ex) http://localhost:80/home.html -> hostname: "localhost", port: "80", pathname: "/home.html"
 *
 * Returns: 0 if OK, -1 if error occurred
 */
void parse_uri(char *uri, char *hostname, char *port, char *pathname) {
    // 1. "http://" 접두어를 파싱
    // http://43.200.171.41:3001/home.html 
    char *start = strstr(uri, "http://");
    char *port_end;
    if (start == NULL) {
        return -1;
    }
    start += strlen("http://");
    // 43.200.171.41(ptr):(end)3001(port_end)/home.html

    // 2. 호스트 이름을 파싱
    char *end = strstr(start, ":");
    if (end == NULL) {
        end = strstr(start, "/");
        if (end == NULL) {
            return -1;
        }
    }
    strncpy(hostname, start, end - start);

    // 3. 포트 번호, pathname 파싱
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


/*
main : 지정된 포트에서 들어오는 연결을 수신 대기하고 각 연결을 처리할 새 스레드를 만듭니다.
*/
int main(int argc, char **argv) {
  int listenfd, *connfd;                         // 소켓 파일 디스크립터를 저장하기 위한 변수
  pthread_t tid;   
  socklen_t clientlen;                          // 클라이언트의 주소 구조체의 길이를 저장하기 위한 변수
  struct sockaddr_storage clientaddr;           // 클라이언트의 주소 구조체를 저장하기 위한 변수
  /* Check command line args */
  if (argc != 2) { // 입력받은 인자의 개수가 2가 아닐 경우, 종료합니다.
    fprintf(stderr, "usage: %s <port>\n", argv[0]); // 사용법 출력
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); // 서버의 소켓을 열고, 클라이언트 요청 대기
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = malloc(sizeof(int));
    *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // 클라이언트와의 연결 수락, 클라이언트의 주소 정보를 가져온다
    Pthread_create(&tid, NULL, thread, connfd);                 // main함수 전체를 무한 반복하여 클라이언트의 연결 요청을 처리
  }
  printf("%s", user_agent_hdr);
  return 0;
}

void *thread(void *vargp)
{
  int connfd = *((int *)vargp);
  Pthread_detach(Pthread_self());
  Free(vargp);
  doit(connfd);
  Close(connfd);
  return NULL;
}