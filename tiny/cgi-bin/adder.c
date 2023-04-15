/*
 * adder.c - a minimal CGI program that adds two numbers together
 http://3.35.3.78:3030/cgi-bin/adder?3&3
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p;                                              //buf는 CGI 프로그램에 전달된 QUERY_STRING을 저장할 포인터
  char arg1[MAXLINE],arg2[MAXLINE], content[MAXLINE];         //arg1과 arg2는 더하기 연산의 두 인자를 저장할 문자열 배열입니다. content는 HTTP 응답의 본문을 저장할 문자열 배열입니다.
  int n1=0, n2=0;                                             //n1과 n2는 각각 첫 번째와 두 번째 더할 값으로 초기화됩니다.

  if ((buf = getenv("QUERY_STRING")) != NULL) {               // CGI 프로그램에 QUERY_STRING이 전달되었는지 확인 
                                                              // 만약 전달된 QUERY_STRING이 있다면, getenv 함수를 통해 해당 문자열을 가져와 buf에 저장합니다.
    p = strchr(buf, '&');                                     // strchr 함수를 통해 & 문자를 찾아 
    *p = '\0';                                                // buf를 첫 번째 인자와 두 번째 인자로 나눕니다. 
    strcpy(arg1, buf);                                        // 이를 각각 arg1과 arg2에 저장
    strcpy(arg2, p+1);
    n1 = atoi(arg1);                                          // atoi 함수를 통해 arg1과 arg2를 정수로 변환하고, 각각 n1과 n2에 저장합니다.
    n2 = atoi(arg2);
  }

  /* Make the response body */
  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>",content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  /* Generate the HTTP response */
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content);
  fflush(stdout);

  exit(0);
}
/* $end adder */
