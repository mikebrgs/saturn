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

typedef struct Server {
  char id [BUFFER_SIZE];
  char ip[16];
  char port[16];
} Server;

typedef struct Connection {
  int fd;
  struct sockaddr_in addr;
} Connection;

state GetDespatch(int *cs_fd,
  char **splitted_buffer,
  struct sockaddr_in *cs_addr,
  Server *server) {
  char cs_buffer[BUFFER_SIZE];
  int tmp;

  memset((void*)&cs_buffer, (int)'\0', BUFFER_SIZE*sizeof(char));
  strcat(cs_buffer, "GET_DS_SERVER ");
  strcat(cs_buffer, *splitted_buffer);
  printf("reqserv.GetDespatch.request: %s\n", cs_buffer);
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
  // Waiting and saving response
  memset((void*)&cs_buffer, (int)'\0', BUFFER_SIZE*sizeof(char));
  n=recvfrom(*cs_fd,
    cs_buffer,
    BUFFER_SIZE,
    0,(struct sockaddr*)&cs_addr,(socklen_t*)&tmp);
  if (n==-1) {
    char error_buffer[1024];
    perror(error_buffer);
    printf("reqserv: recvfrom() error - %s\n", error_buffer);
    return error;
  }
  cs_buffer[strlen(cs_buffer)] = ';';
  printf("reqserv.GetDespatch.response: %s\n", cs_buffer);
  char *splitted_buffer_recv = strtok(cs_buffer, " ");
  splitted_buffer_recv = strtok(NULL, ";");
  if (splitted_buffer_recv == NULL) {
    return error;
  }
  strcpy(server->id, splitted_buffer_recv);
  splitted_buffer_recv = strtok(NULL, ";");
  if (splitted_buffer_recv == NULL) {
    return error;
  }
  strcpy(server->ip, splitted_buffer_recv);
    splitted_buffer_recv = strtok(NULL, ";");
  if (splitted_buffer_recv == NULL) {
    return error;
  }
  strcpy(server->port, splitted_buffer_recv);
  return success;
}

state StartService(Server * server_data,
  Connection * server_connection) {

  if ((server_connection->fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
    printf("1\n");
    return error;
  }

  memset((void*)&(server_connection->addr), (int)'\0', sizeof(server_connection->addr));
  server_connection->addr.sin_family = AF_INET;
  if (inet_aton(server_data->ip, &server_connection->addr.sin_addr) == 0) {
    printf("2\n");
    return error;
  }
  server_connection->addr.sin_port = htons(atoi(server_data->port));

  char server_buffer[BUFFER_SIZE];
  memset((void*)&(server_buffer), (int)'\0', sizeof(server_buffer));
  strcpy(server_buffer, "MY_SERVICE ON");
  if (sendto(server_connection->fd, server_buffer,
    strlen(server_buffer)*sizeof(char), 0,
    (struct sockaddr*)&(server_connection->addr), sizeof(server_connection->addr)) == -1) {
    printf("3\n");
    return error;
  }
  printf("reqserv.StartService.request: %s\n", server_buffer);
  int addrlen;
  if (recvfrom(server_connection->fd, server_buffer,
    BUFFER_SIZE, 0,
    (struct sockaddr*)&(server_connection->addr), (socklen_t*)&addrlen) == -1) {
    printf("4\n");
    return error;
  }
  printf("reqserv.StartService.response: %s\n", server_buffer);

  if (strcmp(server_buffer, "YOUR_SERVICE ON") != 0) {
    return error;
    printf("5\n");
  }

  return success;
}

state EndService(Server * server_data,
  Connection * server_connection) {
  char server_buffer[BUFFER_SIZE];
  memset((void*)&(server_buffer), (int)'\0', sizeof(server_buffer));
  strcpy(server_buffer, "MY_SERVICE OFF");
  printf("reqserv.EndService.request: %s\n", server_buffer);
  if (sendto(server_connection->fd, server_buffer,
    strlen(server_buffer)*sizeof(char), 0,
    (struct sockaddr*)&(server_connection->addr), sizeof(server_connection->addr)) == -1) {
    return error;
  }
  int addrlen;
  if (recvfrom(server_connection->fd, server_buffer,
    BUFFER_SIZE, 0,
    (struct sockaddr*)&(server_connection->addr), (socklen_t*)&addrlen) == -1) {
    return error;
  }
  printf("reqserv.EndService.response: %s\n", server_buffer);
  if (strcmp(server_buffer, "YOUR_SERVICE OFF") != 0) {
    return error;
  }
  close(server_connection->fd);
  return success;
}

int main(int argc, char const *argv[]) {
  // Reading input options
  struct sockaddr_in cs_addr;
  struct hostent *h;
  int cs_fd;
  Server server_data;
  Connection server_connection;

  memset((void*)&cs_addr, (int)'\0', sizeof(cs_addr));
  cs_addr.sin_family = AF_INET;

  cs_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (cs_fd == -1) {
    printf("reqserv: socket() error\n");
    exit(1);
  }

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
  if (csip_acquired == false) {
    h = gethostbyname("tejo.tecnico.ulisboa.pt");
    if (h==NULL){
      printf("reqserv: not able to connect to tejo\n");
      return -1;
    }
    cs_addr.sin_addr = *(struct in_addr*)h->h_addr_list[0];
  }
  if (cspt_acquired == false) {
    cs_addr.sin_port = htons(59000);
  }

  state reqserv_state = disconnected;
  char kb_buffer[BUFFER_SIZE];

  while (reqserv_state != exiting) {
    printf("user: ");
    fgets(kb_buffer, BUFFER_SIZE, stdin);
    if (strlen(kb_buffer) <= 1) continue;
    kb_buffer[strlen(kb_buffer)-1]='\0';
    char * splitted_buffer = strtok(kb_buffer, " ");
    // State evaluation
    // Connection to central service
    if ((strcmp(splitted_buffer, "request_service")==0
      || strcmp(splitted_buffer, "rs") == 0)
      && reqserv_state == disconnected) {
      splitted_buffer = strtok(NULL, " ");
      if (splitted_buffer==NULL) {
        goto invalid;
      }
      if (GetDespatch(&cs_fd, &splitted_buffer, &cs_addr, &server_data) == error) {
        printf("reqserv: error in GetDespatch()\n");
        continue;
      }
      if (StartService(&server_data, &server_connection) == error) {
        printf("reqserv: error in StartService()\n");
        continue;
      }
      reqserv_state = connected;
      printf("reqserv: connected\n");
    }
    // Disconnection of central service
    else if ((strcmp(splitted_buffer, "terminate_service")==0
      || strcmp(splitted_buffer, "ts") == 0)
      && reqserv_state == connected) {
      if (EndService(&server_data, &server_connection) == error) {
        printf("reqserv: error in EndService()\n");
        continue;
      }
      reqserv_state = disconnected;
      printf("reqserv: disconnected\n");
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