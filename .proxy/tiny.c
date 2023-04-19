/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 * 
 * main: 서버의 주요 기능입니다. 지정된 포트에서 들어오는 연결을 수신 대기하고 각 연결을 처리할 새 스레드를 만듭니다.

 * doit: 이 함수는 클라이언트의 HTTP 요청을 처리합니다. 요청 줄과 헤더를 읽고, URI를 구문 분석하여 요청이 정적 또는 동적 콘텐츠에 대한 것인지 확인하고, 적절한 콘텐츠를 제공합니다.

 * read_requesthdrs: 이 함수는 요청 헤더를 읽고 처리합니다.

 * parse_uri: 이 함수는 요청 줄에서 URI를 구문 분석하여 요청이 정적 또는 동적 콘텐츠에 대한 것인지 확인합니다. 또한 URI에서 파일 이름 및 CGI 인수를 추출합니다.

 * serve_static: 이 함수는 파일의 내용을 클라이언트로 전송하여 정적 콘텐츠를 제공합니다.

 * get_filetype: 이 함수는 파일 이름 확장자를 기반으로 파일의 MIME 유형을 결정합니다.

 * serve_dynamic: 이 함수는 서버 측 프로그램을 실행하고 출력을 클라이언트로 전송하여 동적 콘텐츠를 제공합니다.

 * clienterror: 이 함수는 클라이언트에 오류 메시지를 보냅니다.
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);

void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);


void doit(int fd) {
  int is_static;     // 요청된 콘텐츠가 정적인지 동적인지
  struct stat sbuf;  // struct stat 구조체
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];   // 요청라인 저장
  char filename[MAXLINE], cgiargs[MAXLINE];         // URI에서 추출된 파일 이름 및 CGI인수 저장
  rio_t rio;
  
  /* Read request line and headers */
  /* 클라이언트가 rio로 보낸 request라인과 헤더를 읽고 분석한다. */
  Rio_readinitb(&rio, fd);                           // connfd를 연결하여 rio에 저장       
  Rio_readlineb(&rio, buf, MAXLINE);                 // rio에 있는 string 한 줄을 모두 buffer에 옮긴다. 
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);     // 읽어들인 HTTP 요청에서 요청 방식(method), 요청 URI(uri), HTTP 버전(version) 등을 파싱합니다.
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) {                   // 요청 방식이 GET이 아닌 경우, 에러를 반환합니다.
    clienterror(fd, method, "501", "Not implemented","Tiny does not implement this method");
    
    return; 
  }
  
  read_requesthdrs(&rio);                       //요청 헤더를 읽어들입니다.
  
  /* Parse URI from GET request */
  /*요청 URI를 파싱하고, 해당 파일이 존재하는지 여부를 확인합니다.*/
  is_static = parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0) {
      clienterror(fd, filename, "404", "Not found","Tiny couldn't find this file");
      return; 
  }
  /*해당 파일이 정적인 컨텐츠인지 동적인 컨텐츠인지 구분합니다.*/
  if (is_static) { /* Serve static content */
  /*만약 해당 파일이 정적인 컨텐츠인 경우, 파일이 읽을 수 있는 권한이 있는지 확인하고, 정적인 컨텐츠를 제공합니다.
  is_static 변수가 1일 경우, 즉 요청된 파일이 정적인 컨텐츠인 경우, 해당 파일이 읽을 수 있는 권한이 있는지 확인합니다.
  S_ISREG(sbuf.st_mode)는 st_mode 필드에서 파일 타입을 가져와서, 이 값이 일반 파일인지 검사합니다.
  S_IRUSR & sbuf.st_mode는 파일 소유자의 읽기 권한을 확인합니다. 파일 소유자의 읽기 권한이 없는 경우, 
  S_IRUSR 상수와 sbuf.st_mode 값의 비트 AND 연산 결과는 0이 되므로, if문의 조건은 참이 됩니다.
*/
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { 
        clienterror(fd, filename, "403", "Forbidden",
                    "Tiny couldn't read the file");
        return; 
    }
    serve_static(fd, filename, sbuf.st_size); 
  }
  else { /* Serve dynamic content */
    /*만약 해당 파일이 동적인 컨텐츠인 경우, 파일이 실행 가능한 권한이 있는지 확인하고, 동적인 컨텐츠를 제공합니다.*/
      if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
          clienterror(fd, filename, "403", "Forbidden",
                      "Tiny couldn't run the CGI program");
          return; 
      }
      serve_dynamic(fd, filename, cgiargs);
  }
}

/* 요청 헤더를 읽어들이는 함수*/
void read_requesthdrs(rio_t *rp) {
    char buf[MAXLINE];
    Rio_readlineb(rp, buf, MAXLINE);

    while(strcmp(buf, "\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE);    
        printf("%s", buf);
    }
    return;
}

/* uri를 받아 요청받은 파일의 이름과 요청 인자를 채워준다. */
int parse_uri(char *uri, char *filename, char *cgiargs) {   // uri를 받아 요청받은 filename, cgiargs를 반환한다. 
    char *ptr;
    if (!strstr(uri, "cgi-bin")) {  /*주어진 URI 문자열에 "cgi-bin" 이라는 문자열이 포함되어 있는지 확인 만약 없다면, Static content */
        strcpy(cgiargs, "");        // cgiargs는 비어있는 문자열("")로 설정
        strcpy(filename, ".");
        strcat(filename, uri);      // filename은 현재 디렉토리(".")에 URI를 이어붙인 문자열로 설정
        if (uri[strlen(uri)-1] == '/') {
            strcat(filename, "home.html");      // 약 URI의 마지막이 "/"인 경우, filename에 "home.html"을 이어붙인다
        }
        else if (strcmp(uri,"/mp4") == 0) {     // URI 문자열이 "/mp4"와 같은 값과 일치하는 경우, 서버는 "sample.mp4" 파일을 반환
            strcpy(filename, "sample.mp4");
        }
        return 1;
        
    }

    else {  /* Dynamic content */
        ptr = index(uri, '?');
        if (ptr) {
            strcpy(cgiargs, ptr+1);
            *ptr = '\0';
        }
        else {
            strcpy(cgiargs, "");
        }
        strcpy(filename, ".");
        strcat(filename, uri);
        return 0;
    }
}

/* 정적 컨텐츠의 디렉토리를 받아 request 헤더 작성 후 서버에게 보낸다. */
void serve_static(int fd, char *filename, int filesize) {
    int srcfd;                                      // 요청된 파일을 여는 데 사용되는 fd
    char *srcp, filetype[MAXLINE], buf[MAXBUF];     // srcp-요청된 파일을 메모리매핑 하는데 사용할 포인터
                                                    // filetype - 요청된 파일의 MIME유형을 저장하는 데 사용되는 문다 배열
                                                    // buf - HTTP응답을 저장하는 데 사용되는 문자 배열
    
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    
    /* Send response headers to client */
    /* 응답 라인, 헤더 작성 */

    /* 확장자를 기준으로 파일 유형 결정 */ 
    get_filetype(filename, filetype); 
    /*응답 라인과 헤더를 버퍼에 씁니다.*/

    sprintf(buf, "HTTP/1.0 200 OK\r\n");                            // HTTP 응답 라인 작성
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);             // HTTP 응답 헤더 작성
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);        // 응답 바디의 길이 작성
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);      // 응답 바디의 타입 작성
    
    Rio_writen(fd, buf, strlen(buf));                               // HTTP 응답 전송
    sscanf(buf, "%s %s %s", method, uri, version);
    /* 응답 헤더를 서버 콘솔에 출력 */
    printf("Response headers:\n");
    printf("%s", buf);
    
    srcfd = Open(filename, O_RDONLY, 0);    //line:netp:servestatic:open
    srcp = malloc(filesize);                // 요청한 파일의 크기만큼 메모리 할당
    Rio_readn(srcfd, srcp, filesize);       // 파일 내용을 srcp에 저장 srcfd 파일 디스크립터로부터 filesize 바이트의 데이터를 읽어서 srcp가 가리키는 메모리 영역에 저장
    Close(srcfd);                           // 파일 디스크립터 닫기
    Rio_writen(fd, srcp, filesize);         // Rio_writen 함수는 클라이언트로 응답 본문(srcp)을 보내는 역할을 합니다. 함수의 마지막 매개변수 filesize는 보낼 데이터의 크기를 나타냅니다. 함수 호출 후, srcp 버퍼의 크기가 filesize와 동일하다는 가정 하에, filesize바이트만큼의 데이터가 fd 소켓에 안전하게 전송됩니다.
    free(srcp);                             // 동적으로 할당한 메모리 해제    

}

/*
 * get_filetype - Derive file type from filename
 */
// 확장자 바꾸기
void get_filetype(char *filename, char *filetype) {
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".mp4"))
        strcpy(filetype, "video/mp4");
    else
        strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs) {
    char buf[MAXLINE], *emptylist[] = { NULL }; // buf - HTTP응답을 저장하는 데 사용되는 문자 배열, emptylist-자식의 환경으로 사용될 문자열 배열
    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); // HTTP 응답 라인 작성, HTTP/1.0 버전을 사용하고, 200 상태 코드를 가진 "OK" 메시지를 응답하는 것
    Rio_writen(fd, buf, strlen(buf)); // 클라이언트에게 응답 라인 전송
    sprintf(buf, "Server: Tiny Web Server\r\n"); // 서버 정보를 작성, 서버의 이름이 "Tiny Web Server"임을 나타냅니다.
    Rio_writen(fd, buf, strlen(buf)); // 클라이언트에게 서버 정보 전송
    if (Fork() == 0) { /* Child */ // 자식 프로세스를 생성
        /* Real server would set all CGI vars here */
        setenv("QUERY_STRING", cgiargs, 1); // CGI 스크립트에 인자를 전달하기 위해 환경 변수를 설정  // setenv함수를 사용하여 QUERY_STRING 환경 변수를 CGI인수로 설정
        // method를 cgi-bin/adder.c에 넘겨주기 위해 환경변수 set
        
        Dup2(fd, STDOUT_FILENO);            // fd를 표준 출력(STDOUT_FILENO)으로 복제한다
        Execve(filename, emptylist, environ); /* CGI 프로그램 실행  */ // execve함수를 사용하여 filename으로 지정된 서버 측 프로그램 실행
    }
    Wait(NULL); /* Parent waits for and reaps child */ // 자식 프로세스가 종료될 때까지 기다린 후 종료
}


/* 에러메세지와 응답 본체를 서버 소켓을 통해 클라이언트에 보낸다*/
void clienterror(int fd, char *cause, char *errnum,
                  char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];
  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);
  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  /* 에러메세지와 응답 본체를 서버 소켓을 통해 클라이언트에 보낸다. */
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

/* port 번호를 인자로 받아 클라이언트의 요청이 올 때마다 새로 연결 소켓을 만들어 doit 함수를 호출한다. */
int main(int argc, char **argv) {
  int listenfd, connfd;                         // 소켓 파일 디스크립터를 저장하기 위한 변수
  char hostname[MAXLINE], port[MAXLINE];        // 클라이언트의 호스트명과 포트번호를 저장하기 위한 변수입니다.
  socklen_t clientlen;                          // 클라이언트의 주소 구조체의 길이를 저장하기 위한 변수
  struct sockaddr_storage clientaddr;           // 클라이언트의 주소 구조체를 저장하기 위한 변수

  /* Check command line args */
  if (argc != 2) {                              // 입력받은 인자의 개수가 2가 아닐 경우,종료합니다.
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);            // 서버의 소켓을 열고, 해당 소켓 파일 디스크립터를 listenfd에 저장
  while (1) {                                   // 무한히 반복하여 클라이언트의 요청을 받고 처리합니다.
    clientlen = sizeof(clientaddr);             // 클라이언트의 주소 구조체의 크기를 저장
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  // line:netp:tiny:accept //클라이언트의 요청이 들어오면, 해당 요청을 받아들이고, 연결 소켓의 파일 디스크립터를 connfd에 저장
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE,   // getnameinfo : 소켓 주소에서 호스트 이름을 추출하여 IP 주소와 연결
    port, MAXLINE, 0); // 클라이언트의 호스트명과 포트번호를 가져와 hostname과 port에 저장합니다. // 유효하면 1 않으면 0
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit // doit 함수를 호출하여 클라이언트의 요청을 처리합니다.
    Close(connfd);  // line:netp:tiny:close // Close(connfd);: 연결 소켓을 닫습니다.
  }
}