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
#define disconnected 1
#define connected 2
#define busy 3
#define exiting 4

#define start 101
#define join 102
#define error 103
#define success 101


#define BUFFER_SIZE 128

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
} ServerNet;

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
  printf("service:request: %s\n", cs_buffer);
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
  char join_response[BUFFER_SIZE];
  memset((void*)&join_response, (int)'\0', sizeof(char)*BUFFER_SIZE);
  strcpy(join_response, "OK ");
  strcat(join_response, service_net->id);
  strcat(join_response, ";0;0.0.0.0;0");
  if (strcmp(cs_buffer, join_response)==0) {
    return start;
  }
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
  strcat(cs_buffer, service_net->tcp_port);
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

int main(int argc, char const *argv[])
{
  ServerNet service_net;
  struct sockaddr_in cs_addr;
  struct hostent *h;
  int cs_fd;
  memset((void*)&cs_addr, (int)'\0', sizeof(cs_addr));
  cs_addr.sin_family = AF_INET;

  cs_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (cs_fd == -1) {
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
      // bool tmp = false;
      // if (inet_aton(argv[i+1], (struct in_addr*)&service_net.udp) == 0
      //   || inet_aton(argv[i+1], (struct in_addr*)&service_net.tcp) == 0) {
      //   printf("service: error with ip %s\n",  argv[i+1]);
      //   continue;
      // }
      strcpy(service_net.ip, argv[i+1]);
      ip_acquired = true;
      printf("service: acquired ip\n");
    }
    // UDP port
    else if (strcmp("-u", argv[i])==0
      && upt_acquired == false
      && argc > i) {
      // service_net.udp.sin_port = htons(atoi(argv[i+1]));
      strcpy(service_net.udp_port, argv[i+1]);
      upt_acquired = true;
      printf("service: acquired udp port\n");
    }
    // TCP port
    else if (strcmp("-t", argv[i])==0
      && tpt_acquired == false
      && argc > i) {
      // service_net.tcp.sin_port = htons(atoi(argv[i+1]));
      strcpy(service_net.tcp_port, argv[i+1]);
      tpt_acquired = true;
      printf("service: acquired tcp port\n");
    }
  }

  if (!id_acquired
    || !ip_acquired
    || !upt_acquired
    || !tpt_acquired) {
    printf("service: invalid call:\n");
    printf("./build/service –n id –j ip -u upt –t tpt [-i csip] [-p cspt]\n");
    return 1;
  }

  state service_state = disconnected;
  char kb_buffer[128];

  while (service_state != exiting) {
    printf("user: ");
    fgets(kb_buffer, BUFFER_SIZE, stdin);
    kb_buffer[strlen(kb_buffer)-1]='\0';
    char * splitted_buffer = strtok(kb_buffer, " ");
    // 
    if (strcmp(splitted_buffer, "join")==0
      && service_state == disconnected) {
      splitted_buffer = strtok(NULL, " ");
      if (splitted_buffer==NULL) {
        goto invalid;
      }
      strcpy(service_net.service_id, splitted_buffer);
      switch (GetStart(&cs_fd,
        &service_net,
        &cs_addr)) {
        case start :
          if (SetStart(&cs_fd,
            &service_net,
            &cs_addr) == success
            && SetDespatch(&cs_fd,
            &service_net,
            &cs_addr) == success) {
            service_net.is_start = true;
            service_net.is_despatch = true;
            printf("service: started ring and ds\n");
          } else {
            printf("service: error starting ring or ds\n");
          }
          break;
        case join :
          printf("service: join ring\n");
          break;
        case error :
          printf("service: error in GetStart()\n");
          continue;
      }
      service_state = connected;
    }
    else if (strcmp(splitted_buffer, "show_state")==0) {
      printf("service: state\n");
    }
    else if (strcmp(splitted_buffer, "leave")==0
      && service_state == connected) {
      if (service_net.is_despatch 
        && WithdrawDespatch(&cs_fd,
          &service_net,
          &cs_addr) == success){
        service_net.is_despatch = false;
      } else {printf("error in despatch\n");}
      if (service_net.is_start
        && WithdrawStart(&cs_fd,
          &service_net,
          &cs_addr) == success) {
        service_net.is_start = false;
      } else {printf("error in start\n");}
      service_state = disconnected;
      printf("service: leaving\n");
    }
    else if (strcmp(splitted_buffer, "exit")==0
      && service_state == disconnected) {
      service_state = exiting;
      printf("service: exiting\n");
    }
    else {
      invalid:
      printf("service: invalid command\n");
    }
  }

  return 0;
}