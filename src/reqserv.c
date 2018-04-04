#define _BSD_SOURCE

// General includes
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// UNIX
#include <unistd.h>

// Networking includes
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>

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

// Global variables
bool verbose_option = false;
struct timeval timeout_udp;

typedef struct Server {
  char id [BUFFER_SIZE];
  char ip[16];
  char port[16];
} Server;

// Local structures
typedef struct Connection {
  int fd;
  struct sockaddr_in addr;
} Connection;

typedef struct String {
  char string[BUFFER_SIZE];
} String;

// Local functions
state GetDespatch(Connection * central_server,
  String *service_id,
  Server *server) {
  char cs_buffer[BUFFER_SIZE];
  int tmp;

  memset((void*)&cs_buffer, (int)'\0', BUFFER_SIZE*sizeof(char));
  sprintf(cs_buffer, "GET_DS_SERVER %s", service_id->string);
  if (verbose_option)
    printf("reqserv.GetDespatch.request: %s\n", cs_buffer);
  int n = sendto(central_server->fd, cs_buffer,
    sizeof(char)*strlen(cs_buffer),
    0, (struct sockaddr*)&(central_server->addr),
    sizeof(central_server->addr));
  if (n == -1) {
    printf("reqserv.GetDespatch.sendto: error\n");
    return error;
  }
  // Waiting and saving response
  tmp = sizeof(central_server->addr);
  memset((void*)&cs_buffer, (int)'\0', BUFFER_SIZE*sizeof(char));
  n=recvfrom(central_server->fd, cs_buffer, BUFFER_SIZE,
    0,(struct sockaddr*)&(central_server->addr),(socklen_t*)&tmp);
  if (n==-1) {
    char error_buffer[BUFFER_SIZE];
    perror(error_buffer);
    printf("reqserv.GetDespatch.recvfrom: error\n");
    return error;
  }
  if (verbose_option)
    printf("reqserv.GetDespatch.response: %s\n", cs_buffer);
  char token_tmp[BUFFER_SIZE];
  if (sscanf(cs_buffer, "%s %[^;];%[^;];%s", token_tmp, server->id,
    server->ip, server->port) != 4) {
    return error;
  }
  return success;
}

state StartService(Server * server_data,
  Connection * server_connection) {

  if ((server_connection->fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
    return error;
  }
  if (setsockopt(server_connection->fd, SOL_SOCKET, SO_RCVTIMEO,
    &timeout_udp, sizeof(struct timeval)) < 0) {
    printf("service: socket options error\n");
  }


  memset((void*)&(server_connection->addr), (int)'\0', sizeof(server_connection->addr));
  server_connection->addr.sin_family = AF_INET;
  if (inet_aton(server_data->ip, &(server_connection->addr.sin_addr)) == 0) {
    return error;
  }
  server_connection->addr.sin_port = htons(atoi(server_data->port));

  char server_buffer[BUFFER_SIZE];
  memset((void*)&(server_buffer), (int)'\0', sizeof(server_buffer));
  strcpy(server_buffer, "MY_SERVICE ON");
  if (verbose_option)
    printf("reqserv.StartService.request: %s\n", server_buffer);
  if (sendto(server_connection->fd, server_buffer,
    strlen(server_buffer)*sizeof(char), 0,
    (struct sockaddr*)&(server_connection->addr), sizeof(server_connection->addr)) == -1) {
    return error;
  }
  int addrlen = sizeof(server_connection->addr);
  if (recvfrom(server_connection->fd, server_buffer,
    BUFFER_SIZE, 0,
    (struct sockaddr*)&(server_connection->addr), (socklen_t*)&addrlen) == -1) {
    return error;
  }
  if (verbose_option)
    printf("reqserv.StartService.response: %s\n", server_buffer);

  if (strcmp(server_buffer, "YOUR_SERVICE ON") != 0) {
    return error;
  }

  return success;
}

state EndService(Server * server_data,
  Connection * server_connection) {
  char server_buffer[BUFFER_SIZE];
  memset((void*)&(server_buffer), (int)'\0', sizeof(server_buffer));
  strcpy(server_buffer, "MY_SERVICE OFF");
  if (verbose_option)
    printf("reqserv.EndService.request: %s\n", server_buffer);
  if (sendto(server_connection->fd, server_buffer,
    strlen(server_buffer)*sizeof(char), 0,
    (struct sockaddr*)&(server_connection->addr), sizeof(server_connection->addr)) == -1) {
    return error;
  }
  int addrlen = sizeof(server_connection->addr);
  if (recvfrom(server_connection->fd, server_buffer,
    BUFFER_SIZE, 0,
    (struct sockaddr*)&(server_connection->addr), (socklen_t*)&addrlen) == -1) {
    return error;
  }
  if (verbose_option)
    printf("reqserv.EndService.response: %s\n", server_buffer);
  if (strcmp(server_buffer, "YOUR_SERVICE OFF") != 0) {
    return error;
  }
  close(server_connection->fd);
  return success;
}

int main(int argc, char const *argv[]) {
  // Reading input options
  Server service;

  // UDP timeout times
  timeout_udp.tv_sec = 2;
  timeout_udp.tv_usec = 0;

  // Central server connection
  Connection central_server;
  central_server.fd = -1;
  
  // Service server connection
  Connection server_connection;
  server_connection.fd = -1;
  

  memset((void*)&(central_server.addr), (int)'\0', sizeof(central_server.addr));
  central_server.addr.sin_family = AF_INET;
  central_server.fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (central_server.fd == -1) {
    printf("reqserv: socket() error\n");
    exit(1);
  }
  if (setsockopt(central_server.fd, SOL_SOCKET, SO_RCVTIMEO,
    &timeout_udp, sizeof(struct timeval)) < 0) {
    printf("service: socket options error\n");
  }


  // Evaluating arguments
  bool csip_acquired = false;
  bool cspt_acquired = false;
  struct hostent *h;
  size_t i;
  for (i=1; i<argc; i++) {
    if (strcmp("-i",argv[i])==0 
      && csip_acquired==false
      && argc > i) {
      h = gethostbyname(argv[i+1]);
      if (h==NULL){
        printf("reqserv: not able to connect to %s\n", argv[i+1]);
        continue;
      }
      central_server.addr.sin_addr = *(struct in_addr*)h->h_addr_list[0];
      csip_acquired = true;
      printf("reqserv: acquired ip\n");
      i++;
    }
    else if (strcmp("-p",argv[i])==0
      && cspt_acquired==false
      && argc > i) {
      central_server.addr.sin_port = htons(atoi(argv[i+1]));
      cspt_acquired = true;
      printf("reqserv: acquired pt\n");
      i++;
    }
    else if (strcmp("-v", argv[i]) == 0) {
      printf("service: debug info will be printed :)\n");
      verbose_option = true;
    }
    else if (strcmp("-h", argv[i]) == 0) {
      printf("-i <central server's ip> -- not mandatory\n");
      printf("-p <central server's port> -- not mandatory\n");
      printf("-v -- print debug info during runtime -- not mandatory\n");
      printf("-h -- show info -- not mandatory\n");
      printf("./build/service [-i csip] [-p cspt] [-v] [-h]\n");
      return 0;
    }
  }
  if (csip_acquired == false) {
    h = gethostbyname("tejo.tecnico.ulisboa.pt");
    if (h==NULL){
      printf("reqserv: not able to connect to tejo\n");
      return -1;
    }
    central_server.addr.sin_addr = *(struct in_addr*)h->h_addr_list[0];
  }
  if (cspt_acquired == false) {
    central_server.addr.sin_port = htons(59000);
  }

  state reqserv_state = disconnected;
  char kb_buffer[BUFFER_SIZE], command_buffer[BUFFER_SIZE];
  String service_buffer;

  while (reqserv_state != exiting) {
    // printf("user: ");
    fgets(kb_buffer, BUFFER_SIZE, stdin);
    // Command evaluation
    int scan_args = sscanf(kb_buffer, "%s %s\n", command_buffer,
      service_buffer.string);
    if (scan_args < 1) continue;
    // State evaluation
    // Connection to central service
    if (scan_args >= 2
      && (strcmp(command_buffer, "request_service")==0
      || strcmp(command_buffer, "rs") == 0)
      && reqserv_state == disconnected) {
      if (GetDespatch(&central_server, &service_buffer, &service) == error) {
        printf("reqserv: error in GetDespatch()\n");
        continue;
      }
      if (StartService(&service, &server_connection) == error) {
        printf("reqserv: error in StartService()\n");
        continue;
      }
      reqserv_state = connected;
      printf("reqserv: connected\n");
    }
    // Disconnection of central service
    else if ((strcmp(command_buffer, "terminate_service")==0
      || strcmp(command_buffer, "ts") == 0)
      && reqserv_state == connected) {
      if (EndService(&service, &server_connection) == error) {
        printf("reqserv: error in EndService()\n");
        continue;
      }
      reqserv_state = disconnected;
      printf("reqserv: disconnected\n");
    }
    // Exit client
    else if(strcmp(command_buffer, "exit")==0
      && reqserv_state == disconnected) {
      reqserv_state = exiting;
      printf("exiting\n");
    }
    // Invalid commands
    else {
      printf("reqserv: invalid command\n");
    }
  }

  return 0;
}