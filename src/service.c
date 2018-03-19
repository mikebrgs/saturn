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
// Service/Ring state
#define disconnected 1  // Ring
#define available 2  // Ring
#define busy 3  // Ring
#define exiting 4

// Function interaction
#define start 101
#define join 102
#define error 103
#define success 101

// Client interaction
#define start_service 201
#define terminate_service 202
#define invalid_service 203

typedef int token;
#define T 1

#define BUFFER_SIZE 128

// Structures
typedef struct Server {
  char id[BUFFER_SIZE];
  char ip[16];
  char port[16];
} Server;

typedef struct ServerNet {
  char id[BUFFER_SIZE];
  // struct sockaddr_in udp;
  // struct sockaddr_in tcp;
  char ip[16];
  char udp_port[16];
  char tcp_port[16];
  bool is_start;
  bool is_despatch;
  char service_id[BUFFER_SIZE];
  Server next_server;
} ServerNet;

typedef struct Connection {
  int fd;
  struct sockaddr_in addr;
} Connection;

// Global variables
state service_state = disconnected;
state ring_state = disconnected;

int Max(int a, int b) {
  if (a > b) {
    return a;
  }
  return b;
}

state GetStart(int *cs_fd,
  ServerNet * service_net,
  struct sockaddr_in * cs_addr) {
  char cs_buffer[128];
  int tmp;

  memset((void*)&cs_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
  strcpy(cs_buffer, "GET_START ");
  strcat(cs_buffer, service_net->service_id);
  strcat(cs_buffer, ";");
  strcat(cs_buffer, service_net->id);
  printf("service: request: %s\n", cs_buffer);
  // Enquiring the central server
  int n = sendto(*cs_fd,
    cs_buffer,
    sizeof(char)*strlen(cs_buffer),
    0, (struct sockaddr*)cs_addr,
    sizeof(*cs_addr));
  if (n == -1) {
    printf("service: sendto() error\n");
    return error;
  }
  // printf("SENT\n");
  // Waiting for response
  memset((void*)&cs_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
  n=recvfrom(*cs_fd,
    cs_buffer,
    BUFFER_SIZE,
    0,(struct sockaddr*)cs_addr,&tmp);
  if (n==-1) {
    char error_buffer[1024];
    perror(error_buffer);
    printf("service: recvfrom() error - %s\n", error_buffer);
    return error;
  }
  // printf("RECV\n");
  printf("service: response: %s\n", cs_buffer);
  // Check if there was an error
  char join_response[BUFFER_SIZE];
  memset((void*)&join_response, (int)'\0', sizeof(char)*BUFFER_SIZE);
  strcat(join_response, "OK 0;0;0.0.0.0;0");
  if (strcmp(cs_buffer, join_response)==0) {
    return error;
  }
  // Check if this should start new service
  memset((void*)&join_response, (int)'\0', sizeof(char)*BUFFER_SIZE);
  strcpy(join_response, "OK ");
  strcat(join_response, service_net->id);
  strcat(join_response, ";0;0.0.0.0;0");
  if (strcmp(cs_buffer, join_response)==0) {
    return start;
  }
  // Then we should join a service
  char * splitted_buffer = strtok(cs_buffer, " ");
  if (splitted_buffer == NULL) return error;
  splitted_buffer = strtok(NULL, ";");
  if (splitted_buffer == NULL) return error;
  splitted_buffer = strtok(NULL, ";");
  if (splitted_buffer == NULL) return error;
  strcpy(service_net->next_server.id, splitted_buffer);
  splitted_buffer = strtok(NULL, ";");
  if (splitted_buffer == NULL) return error;
  strcpy(service_net->next_server.ip, splitted_buffer);
  splitted_buffer = strtok(NULL, ";");
  if (splitted_buffer == NULL) return error;
  strcpy(service_net->next_server.port, splitted_buffer);
  return join;
}

state SetStart(int *cs_fd,
  ServerNet * service_net,
  struct sockaddr_in * cs_addr) {
  char cs_buffer[128];
  int tmp;

  memset((void*)&cs_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
  strcpy(cs_buffer, "SET_START ");
  strcat(cs_buffer, service_net->service_id);
  strcat(cs_buffer, ";");
  strcat(cs_buffer, service_net->id);
  strcat(cs_buffer, ";");
  strcat(cs_buffer, service_net->ip);
  strcat(cs_buffer, ";");
  strcat(cs_buffer, service_net->tcp_port);
  printf("service: request: %s\n", cs_buffer);
  // Informing the central server
  int n = sendto(*cs_fd,
    cs_buffer,
    sizeof(char)*strlen(cs_buffer),
    0, (struct sockaddr*)cs_addr,
    sizeof(*cs_addr));
  if (n == -1) {
    char error_buffer[1024];
    perror(error_buffer);
    printf("service: sendto() error - %s\n", error_buffer);
    return error;
  }
  // printf("SENT\n");
  // Waiting for response
  memset((void*)&cs_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
  n=recvfrom(*cs_fd,
    cs_buffer,
    BUFFER_SIZE,
    0,(struct sockaddr*)cs_addr,&tmp);
  if (n==-1) {
    char error_buffer[1024];
    perror(error_buffer);
    printf("service: recvfrom() error - %s\n", error_buffer);
    return error;
  }
  printf("service: response: %s\n", cs_buffer);
  char join_response[BUFFER_SIZE];
  memset((void*)&join_response, (int)'\0', sizeof(char)*BUFFER_SIZE);
  strcpy(join_response, "OK ");
  strcat(join_response, service_net->id);
  strcat(join_response, ";0;0.0.0.0;0");
  if (strcmp(cs_buffer, join_response)!=0) {
    printf("service: error with central service\n");
    return error;
  }
  return success;
}

state SetDespatch(int *cs_fd,
  ServerNet * service_net,
  struct sockaddr_in * cs_addr) {
  char cs_buffer[128];
  int tmp;

  memset((void*)&cs_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
  strcpy(cs_buffer, "SET_DS ");
  strcat(cs_buffer, service_net->service_id);
  strcat(cs_buffer, ";");
  strcat(cs_buffer, service_net->id);
  strcat(cs_buffer, ";");
  strcat(cs_buffer, service_net->ip);
  strcat(cs_buffer, ";");
  strcat(cs_buffer, service_net->udp_port);
  printf("service: request: %s\n", cs_buffer);

  // Informing the central server
  int n = sendto(*cs_fd,
    cs_buffer,
    sizeof(char)*strlen(cs_buffer),
    0, (struct sockaddr*)cs_addr,
    sizeof(*cs_addr));
  if (n == -1) {
    printf("service: sendto() error\n");
    return error;
  }
  // printf("SENT\n");
  // Waiting for response
  memset((void*)&cs_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
  n=recvfrom(*cs_fd,
    cs_buffer,
    BUFFER_SIZE,
    0,(struct sockaddr*)cs_addr,&tmp);
  if (n==-1) {
    char error_buffer[1024];
    perror(error_buffer);
    printf("service: recvfrom() error - %s\n", error_buffer);
    return error;
  }
  printf("service: response: %s\n", cs_buffer);
  char join_response[BUFFER_SIZE];
  memset((void*)&join_response, (int)'\0', sizeof(char)*BUFFER_SIZE);
  strcpy(join_response, "OK ");
  strcat(join_response, service_net->id);
  strcat(join_response, ";0;0.0.0.0;0");
  if (strcmp(cs_buffer, join_response)!=0) {
    printf("service: error with central service\n");
    return error;
  }
  return success;
}

// Auxiliary functions
state JoinRing(ServerNet * service_net,
  Connection *next_server,
  Connection *prev_server) {
  // Convert strings to net format and prepare address
  memset((void*)&(next_server->addr),
    (int)'\0', sizeof(next_server->addr));
  next_server->addr.sin_family = AF_INET;
  if (inet_aton(service_net->next_server.ip, &next_server->addr.sin_addr)==0) {
    return error;
  }
  next_server->addr.sin_port = htons(atoi(service_net->next_server.port));
  // Prepare connection
  if (connect(next_server->fd,
    (struct sockaddr*)&(next_server->addr),
    sizeof(next_server->addr))==-1) {
    return error;
  }
  // Send token
  char token_buffer[BUFFER_SIZE];
  memset((void*)&token_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
  strcpy(token_buffer, "NEW ");
  strcat(token_buffer, service_net->id);
  strcat(token_buffer, ";");
  strcat(token_buffer, service_net->ip);
  strcat(token_buffer, ";");
  strcat(token_buffer, service_net->tcp_port);
  strcat(token_buffer, "\n");
  printf("service: token: %s", token_buffer);

  int token_size, token_left, token_done;
  char * token_pointer;
  token_size = strlen(token_buffer)*sizeof(char);
  token_left = token_size;
  token_pointer = &token_buffer[0];
  // Send token to server and wait for response
  while (token_left > 0) {
    token_done = write(next_server->fd, token_pointer, token_left);
    if (token_done <= 0) {
      return error;
    }
    token_left -= token_done;
    token_pointer += token_done;
  }

  // Prepare listen address
  memset((void*)&token_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
  memset((void*)&(prev_server->addr),
    (int)'\0', sizeof(prev_server->addr));
  prev_server->addr.sin_family = AF_INET;
  prev_server->addr.sin_addr.s_addr = htonl(INADDR_ANY);
  prev_server->addr.sin_port = htons(atoi(service_net->tcp_port));

  if (bind(prev_server->fd,
    (struct sockaddr*)&(prev_server->addr),
    sizeof(prev_server->addr)) == -1) {
    return error;
  }
  if (listen(prev_server->fd, 5) == -1) {
    return error;
  }
  int addrlen, n, newfd;
  addrlen = sizeof(prev_server->addr);
  while (1) {
    if ((newfd = accept(prev_server->fd,
      (struct sockaddr*)&prev_server->addr,
      &addrlen))==-1) {
      return error;
    }
    while ((n=read(newfd, token_buffer, 128)) != 0) {
      if (n == -1) {
        return error;
      }

    }
  }



  return success;
}


state WithdrawDespatch(int *cs_fd,
  ServerNet * service_net,
  struct sockaddr_in * cs_addr) {
  char cs_buffer[128];
  int tmp;

  memset((void*)&cs_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
  strcpy(cs_buffer, "WITHDRAW_DS ");
  strcat(cs_buffer, service_net->service_id);
  strcat(cs_buffer, ";");
  strcat(cs_buffer, service_net->id);
  printf("service: request: %s\n", cs_buffer);

  // Informing the central server
  int n = sendto(*cs_fd,
    cs_buffer,
    sizeof(char)*strlen(cs_buffer),
    0, (struct sockaddr*)cs_addr,
    sizeof(*cs_addr));
  if (n == -1) {
    printf("service: sendto() error\n");
    return error;
  }
  // printf("SENT\n");
  // Waiting for response
  memset((void*)&cs_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
  n=recvfrom(*cs_fd,
    cs_buffer,
    BUFFER_SIZE,
    0,(struct sockaddr*)cs_addr,&tmp);
  if (n==-1) {
    printf("service: recvfrom() error\n");
    return error;
  }
  printf("service: responde: %s\n", cs_buffer);
  char join_response[BUFFER_SIZE];
  memset((void*)&join_response, (int)'\0', sizeof(char)*BUFFER_SIZE);
  strcpy(join_response, "OK ");
  strcat(join_response, service_net->id);
  strcat(join_response, ";0;0.0.0.0;0");
  if (strcmp(cs_buffer, join_response)!=0) {
    printf("service: error with central service\n");
    return error;
  }
  return success;
}

state WithdrawStart(int *cs_fd,
  ServerNet * service_net,
  struct sockaddr_in * cs_addr) {
  char cs_buffer[128];
  int tmp;

  memset((void*)&cs_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
  strcpy(cs_buffer, "WITHDRAW_START ");
  strcat(cs_buffer, service_net->service_id);
  strcat(cs_buffer, ";");
  strcat(cs_buffer, service_net->id);
  printf("service: request: %s\n", cs_buffer);

  // Informing the central server
  int n = sendto(*cs_fd,
    cs_buffer,
    sizeof(char)*strlen(cs_buffer),
    0, (struct sockaddr*)cs_addr,
    sizeof(*cs_addr));
  if (n == -1) {
    char error_buffer[1024];
    perror(error_buffer);
    printf("service: sendto() error - %s\n", error_buffer);
    return error;
  }
  // printf("SENT\n");
  // Waiting for response
  memset((void*)&cs_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
  n=recvfrom(*cs_fd,
    cs_buffer,
    BUFFER_SIZE,
    0,(struct sockaddr*)cs_addr,&tmp);
  if (n==-1) {
    char error_buffer[1024];
    perror(error_buffer);
    printf("service: recvfrom() error - %s\n", error_buffer);
    return error;
  }
  printf("service: response: %s\n", cs_buffer);
  char join_response[BUFFER_SIZE];
  memset((void*)&join_response, (int)'\0', sizeof(char)*BUFFER_SIZE);
  strcpy(join_response, "OK ");
  strcat(join_response, service_net->id);
  strcat(join_response, ";0;0.0.0.0;0");
  if (strcmp(cs_buffer, join_response)!=0) {
    printf("service: error with central service\n");
    return error;
  }
  return success;
}

state ClassifyToken() {
  return error;
}

state SendToken() {
  return error;
}

state OpenClientServer(ServerNet *service_net,
  Connection *client) {
  memset((void*)&(client->addr), (int)'\0', sizeof(client->addr));
  client->addr.sin_family = AF_INET;
  client->addr.sin_addr.s_addr = htonl(INADDR_ANY);
  client->addr.sin_port = htons(atoi(service_net->udp_port));
  if (bind(client->fd, (struct sockaddr*)&(client->addr), sizeof(client->addr)) == -1) {
    return error;
  }
  return success;
}

state InteractClient(Connection *client) {
  int addrlen = sizeof(client->addr);
  char client_buffer[BUFFER_SIZE];
  memset((void*)&(client_buffer), (int)'\0', sizeof(char)*BUFFER_SIZE);
  if (recvfrom(client->fd,
    client_buffer,
    BUFFER_SIZE*sizeof(char), 0,
    (struct sockaddr*)&(client->addr), &addrlen) == -1) {
    return error;
  }
  printf("service: request: %s\n", client_buffer);
  if (strcmp(client_buffer, "MY_SERVICE ON") == 0
    && service_state == available) {
    memset((void*)&(client_buffer), (int)'\0', sizeof(char)*BUFFER_SIZE);
    strcpy(client_buffer, "YOUR_SERVICE ON");
    printf("service: response: %s\n", client_buffer);
    if (sendto(client->fd, client_buffer,
      sizeof(char)*strlen(client_buffer),
      0, (struct sockaddr*)&(client->addr), addrlen) == -1) {
      return error;
    }
    return start_service;
  } else if (strcmp(client_buffer, "MY_SERVICE OFF") == 0
    && service_state == busy) {
    memset((void*)&(client_buffer), (int)'\0', sizeof(char)*BUFFER_SIZE);
    strcpy(client_buffer, "YOUR_SERVICE OFF");
    printf("service: response: %s\n", client_buffer);
    if (sendto(client->fd, client_buffer,
      sizeof(char)*strlen(client_buffer),
      0, (struct sockaddr*)&(client->addr), addrlen) == -1) {
      return error;
    }
    return terminate_service;
  }
  return invalid_service;
}

state CloseClient(Connection *client) {
  close(client->fd);
  return success;
}

int main(int argc, char const *argv[])
{
  ServerNet service_net;
  struct sockaddr_in cs_addr;
  struct hostent *h;
  memset((void*)&cs_addr, (int)'\0', sizeof(cs_addr));
  cs_addr.sin_family = AF_INET;

  // Socket descriptors
  fd_set rfds;
  int cs_fd, next_server_fd, prev_server_fd, client_fd;
  cs_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (cs_fd == -1) {
    printf("service: socket() error\n");
    exit(1);
  }
  Connection next_server;
  next_server.fd = socket(AF_INET, SOCK_STREAM, 0);
  if (next_server.fd == -1) {
    printf("service: socket() error\n");
    exit(1);
  }
  Connection prev_server;
  prev_server.fd = socket(AF_INET, SOCK_STREAM, 0);
  if (prev_server.fd == -1) {
    printf("service: socket() error\n");
    exit(1);
  }
  Connection client;
  client.fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (client.fd == -1) {
    printf("service: socket() error\n");
    exit(1);
  }

  // Preparing standard options
  h = gethostbyname("tejo.tecnico.ulisboa.pt");
  if (h==NULL){
    printf("service: not able to connect to tejo\n");
    return -1;
  }
  cs_addr.sin_addr = *(struct in_addr*)h->h_addr_list[0];
  cs_addr.sin_port = htons(59000);

  // Evaluate arguments
  bool csip_acquired = false;
  bool cspt_acquired = false;
  bool id_acquired = false;
  bool ip_acquired = false;
  bool upt_acquired = false;
  bool tpt_acquired = false;
  for (size_t i=1; i<argc; i=i+2) {
    // CS IP
    if (strcmp("-i",argv[i])==0 
      && csip_acquired==false
      && argc > i) {
      h = gethostbyname(argv[i+1]);
      if (h==NULL){
        printf("service: not able to connect to %s\n", argv[i+1]);
        continue;
      }
      cs_addr.sin_addr = *(struct in_addr*)h->h_addr_list[0];
      csip_acquired = true;
      printf("service: acquired ip\n");
    }
    // CS port
    else if (strcmp("-p",argv[i])==0
      && cspt_acquired==false
      && argc > i) {
      cs_addr.sin_port = htons(atoi(argv[i+1]));
      cspt_acquired = true;
      printf("service: acquired pt\n");
    }
    // ID
    else if (strcmp("-n", argv[i])==0
      && id_acquired == false
      && argc > i) {
      strcpy(service_net.id, argv[i+1]);
      id_acquired = true;
      printf("service: acquired id %s\n", service_net.id);
    }
    // IP
    else if (strcmp("-j", argv[i])==0
      && ip_acquired == false
      && argc > i) {
      strcpy(service_net.ip, argv[i+1]);
      ip_acquired = true;
      printf("service: acquired ip\n");
    }
    // UDP port
    else if (strcmp("-u", argv[i])==0
      && upt_acquired == false
      && argc > i) {
      strcpy(service_net.udp_port, argv[i+1]);
      upt_acquired = true;
      printf("service: acquired udp port\n");
    }
    // TCP port
    else if (strcmp("-t", argv[i])==0
      && tpt_acquired == false
      && argc > i) {
      strcpy(service_net.tcp_port, argv[i+1]);
      tpt_acquired = true;
      printf("service: acquired tcp port\n");
    }
  }

  // Test if all the required arguments were acquired
  if (!id_acquired
    || !ip_acquired
    || !upt_acquired
    || !tpt_acquired) {
    printf("service: invalid call:\n");
    printf("./build/service –n id –j ip -u upt –t tpt [-i csip] [-p cspt]\n");
    return 1;
  }

  char kb_buffer[128];
  int max_fd = -1;
  printf("user: ");
  fflush(stdout);

  while (service_state != exiting) {
    // Preparing multitasking
    FD_ZERO(&rfds);
    FD_SET(prev_server.fd, &rfds);
    max_fd = prev_server.fd;
    FD_SET(fileno(stdin), &rfds);
    max_fd = Max(max_fd, fileno(stdin));
    // If it's available, listen to clients
    if (service_state == available ||
      service_state == busy) {
      FD_SET(client.fd, &rfds);
      max_fd = Max(max_fd, client.fd);
    }

    // Block until some action happens
    if (select(max_fd+1, &rfds, NULL, NULL, NULL) <= 0) {
      exit(1);
    }

    // Keyboard action
    if (FD_ISSET(fileno(stdin), &rfds)) {
      fgets(kb_buffer, sizeof(kb_buffer), stdin);
      if (strlen(kb_buffer) <= 1) continue;
      kb_buffer[strlen(kb_buffer)-1]='\0';
      char *splitted_buffer = strtok(kb_buffer, " ");

      // Detect the command
      if (strcmp(splitted_buffer, "join")==0
        && service_state == disconnected) {
        // Verify if ring's id is specified
        splitted_buffer = strtok(NULL, " ");
        if (splitted_buffer==NULL) {
          goto invalid;
        }
        strcpy(service_net.service_id, splitted_buffer);
        // Struct to save data in case of joinning ring
        switch (GetStart(&cs_fd, &service_net, &cs_addr)) {
          case start :
            if (SetStart(&cs_fd, &service_net, &cs_addr) == success
              && SetDespatch(&cs_fd, &service_net, &cs_addr) == success
              && OpenClientServer(&service_net, &client) == success) {
              printf("service: start success\n");
              service_state = available;
              ring_state = available;
            } else {
              printf("service: start error\n");
            }
            break;
          case join :
            // if (JoinRing(&service_net, &next_server, &prev_server) == success) {
            //   printf("service: join success\n");
            //   service_state = available;
            // } else {
            //   printf("service: join error\n");
            // }
              printf("service: join -- not implemented\n");
            break;
          default :
            goto invalid;
        }
      } else if (strcmp(splitted_buffer, "show_state")==0) {
        printf("service: ");
        // Self info
        switch(service_state){
          case disconnected :
            printf("Disconnected ");
            break;
          case available :
            printf("Connected & Available ");
            break;
          case busy :
            printf("Connected & Busy ");
            break;
        }
        // Ring info
        printf("Ring... ");
        // Successor info
        printf("Successor...\n");
      } else if (strcmp(splitted_buffer, "leave")==0
        && service_state == available) {
        printf("service: left\n");
        service_state = disconnected;
      } else if (strcmp(splitted_buffer, "exit")==0
      && service_state == disconnected) {
      service_state = exiting;
      printf("service: exiting\n");
      } else {
        invalid:
        printf("service: invalid\n");
      }

      printf("user: ");
      fflush(stdout);
    }

    // Socket action - servers
    if (FD_ISSET(prev_server.fd, &rfds)) {
      // Manage tokens
    }

    if (FD_ISSET(client.fd, &rfds)) {
      switch (InteractClient(&client)) {
        case start_service :
          service_state = busy;
          break;
        case terminate_service :
          service_state = available;
          break;
        case invalid_service :
        default :
          break;
      }
    }
















    // continue;
    // printf("user: ");
    // fgets(kb_buffer, BUFFER_SIZE, stdin);
    // kb_buffer[strlen(kb_buffer)-1]='\0';
    // char * splitted_buffer = strtok(kb_buffer, " ");
    // // 
    // if (strcmp(splitted_buffer, "join")==0
    //   && service_state == disconnected) {
    //   splitted_buffer = strtok(NULL, " ");
    //   if (splitted_buffer==NULL) {
    //     goto invalid;
    //   }
    //   strcpy(service_net.service_id, splitted_buffer);
    //   switch (GetStart(&cs_fd,
    //     &service_net,
    //     &cs_addr)) {
    //     case start :
    //       if (SetStart(&cs_fd,
    //         &service_net,
    //         &cs_addr) == success
    //         && SetDespatch(&cs_fd,
    //         &service_net,
    //         &cs_addr) == success) {
    //         service_net.is_start = true;
    //         service_net.is_despatch = true;
    //         printf("service: started ring and ds\n");
    //       } else {
    //         printf("service: error starting ring or ds\n");
    //       }
    //       break;
    //     case join :
    //       printf("service: join ring\n");
    //       break;
    //     case error :
    //       printf("service: error in GetStart()\n");
    //       continue;
    //   }
    //   service_state = available;
    // }
    // else if (strcmp(splitted_buffer, "show_state")==0) {
    //   printf("service: state\n");
    // }
    // else if (strcmp(splitted_buffer, "leave")==0
    //   && service_state == available) {
    //   if (service_net.is_despatch 
    //     && WithdrawDespatch(&cs_fd,
    //       &service_net,
    //       &cs_addr) == success){
    //     service_net.is_despatch = false;
    //   } else {printf("error in despatch\n");}
    //   if (service_net.is_start
    //     && WithdrawStart(&cs_fd,
    //       &service_net,
    //       &cs_addr) == success) {
    //     service_net.is_start = false;
    //   } else {printf("error in start\n");}
    //   service_state = disconnected;
    //   printf("service: leaving\n");
    // }
    // else if (strcmp(splitted_buffer, "exit")==0
    //   && service_state == disconnected) {
    //   service_state = exiting;
    //   printf("service: exiting\n");
    // }
    // else {
    //   // invalid:
    //   printf("service: invalid command\n");
    // }
  }

  return 0;
}