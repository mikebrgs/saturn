// General includes
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// UNIX
#include <unistd.h>

// Networking includes
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

typedef int bool;
#define true 1
#define false 0

typedef int state;
#define connected 1
#define disconnected 2
#define exiting 0

#define BUFFER_SIZE 128

#define success 200
#define error 201

state GetDespatch(int *cs_fd,
  char **splitted_buffer,
  struct sockaddr_in *cs_addr) {
  char cs_buffer[BUFFER_SIZE];
  int tmp;

  memset((void*)&cs_buffer, (int)'\0', BUFFER_SIZE*sizeof(char));
  strcat(cs_buffer, "GET_DS_SERVER ");
  strcat(cs_buffer, *splitted_buffer);
  printf("reqserv: request: %s\n", cs_buffer);
  int n = sendto(*cs_fd,
    cs_buffer,
    sizeof(char)*strlen(cs_buffer),
    0, (struct sockaddr*)cs_addr,
    sizeof(*cs_addr));
  if (n == -1) {
    char error_buffer[1024];
    perror(error_buffer);
    printf("reqserv: sendto() error - %s\n", error_buffer);
    return error;
  }
  // Waiting for response
  memset((void*)&cs_buffer, (int)'\0', BUFFER_SIZE*sizeof(char));
  n=recvfrom(*cs_fd,
    cs_buffer,
    BUFFER_SIZE,
    0,(struct sockaddr*)&cs_addr,&tmp);
  if (n==-1) {
    char error_buffer[1024];
    perror(error_buffer);
    printf("reqserv: recvfrom() error - %s\n", error_buffer);
    return error;
  }
  printf("reqserv: response: %s\n", cs_buffer);
}

int main(int argc, char const *argv[]) {
  // Reading input options
  struct sockaddr_in cs_addr;
  struct hostent *h;
  int cs_fd;
  memset((void*)&cs_addr, (int)'\0', sizeof(cs_addr));
  cs_addr.sin_family = AF_INET;

  cs_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (cs_fd == -1) {
    printf("reqserv: socket() error\n");
    exit(1);
  }

  // Preparing standard options
  h = gethostbyname("tejo.tecnico.ulisboa.pt");
  if (h==NULL){
    printf("reqserv: not able to connect to tejo\n");
    return -1;
  }
  cs_addr.sin_addr = *(struct in_addr*)h->h_addr_list[0];
  cs_addr.sin_port = htons(59000);

  // Evaluating arguments
  bool csip_acquired = false;
  bool cspt_acquired = false;
  for (size_t i=1; i<argc; i=i+2) {
    if (strcmp("-i",argv[i])==0 
      && csip_acquired==false
      && argc > i) {
      h = gethostbyname(argv[i+1]);
      if (h==NULL){
        printf("reqserv: not able to connect to %s\n", argv[i+1]);
        continue;
      }
      cs_addr.sin_addr = *(struct in_addr*)h->h_addr_list[0];
      csip_acquired = true;
      printf("reqserv: acquired ip\n");
    }
    else if (strcmp("-p",argv[i])==0
      && cspt_acquired==false
      && argc > i) {
      cs_addr.sin_port = htons(atoi(argv[i+1]));
      cspt_acquired = true;
      printf("reqserv: acquired pt\n");
    }
  }

  state reqserv_state = disconnected;
  char kb_buffer[BUFFER_SIZE];

  while (reqserv_state != exiting) {
    printf("user: ");
    fgets(kb_buffer, BUFFER_SIZE, stdin);
    kb_buffer[strlen(kb_buffer)-1]='\0';
    char * splitted_buffer = strtok(kb_buffer, " ");
    // State evaluation
    // Connection to central service
    if (strcmp(splitted_buffer, "request_service")==0
      && reqserv_state == disconnected) {
      splitted_buffer = strtok(NULL, " ");
      if (splitted_buffer==NULL) {
        goto invalid;
      }
      if (GetDespatch(&cs_fd,
        &splitted_buffer,
        &cs_addr) == error) {
        printf("reqserv: error in GetDespatch()\n");
        continue;
      }
      reqserv_state = connected;
    }
    // Disconnection of central service
    else if (strcmp(splitted_buffer, "terminate_service")==0
      && reqserv_state == connected) {
      // send to
      reqserv_state = disconnected;
      printf("terminating service\n");
    }
    // Exit client
    else if(strcmp(splitted_buffer, "exit")==0
      && reqserv_state == disconnected) {
      reqserv_state = exiting;
      printf("exiting\n");
    }
    // Invalid commands
    else {
      invalid:
      printf("reqserv: invalid command\n");
    }
  }

  return 0;
}