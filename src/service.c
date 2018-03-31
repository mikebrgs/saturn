// C includes
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>

// UNIX
#include <unistd.h>

// Networking includes
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// System
#include <sys/socket.h>
#include <sys/time.h>


typedef int bool;
#define true 1
#define false 0

typedef int state;
// Service/Ring state
#define disconnected 1  // &Ring
#define available 2  // &Ring
#define busy 3  // &Ring
#define exiting 4
#define leaving 6
#define joinning 7
// Function interaction
#define start 101
#define join 102
#define error 103
#define success 101
#define pass 104
#define handle 105
#define closecom 106
#define ignore 107
#define closeservercom 108
#define servicebusy
// Client interaction
#define start_service 201
#define terminate_service 202
#define invalid_service 203

typedef int token;
#define S 301
#define T 302
#define I 303
#define D 304
#define N 305 // New server in the ring
#define O 306 // Leaving the ring
#define NS 307 // New start
#define NW 308 // New server


#define BUFFER_SIZE 128

// Structures
typedef struct Server {
  char id[BUFFER_SIZE];
  char ip[BUFFER_SIZE];
  char port[BUFFER_SIZE];
} Server;

typedef struct ServerNet {
  char id[BUFFER_SIZE];
  char ip[BUFFER_SIZE];
  char udp_port[BUFFER_SIZE];
  char tcp_port[BUFFER_SIZE];
  char service_id[BUFFER_SIZE];
  Server next_server;
} ServerNet;

typedef struct Connection {
  int fd;
  struct sockaddr_in addr;
} Connection;

typedef struct String {
  char string[BUFFER_SIZE];
} String;

// Global variables
state service_state = disconnected;
state ring_state = disconnected;
bool despatch_state = false;
bool start_state = false;
bool waiting_to_leave = false;
bool joinning_ring = false;
bool verbose_option = false;

struct timeval timeout_udp;
struct timeval timeout_client;

int Max(int a, int b) {
  if (a > b) {
    return a;
  }
  return b;
}

bool BiggerID(ServerNet * service_net,
  Server * server) {
  if (verbose_option)
    printf("service.BiggerID: %d, %d\n", atoi(service_net->id), atoi(server->id));
  if (atoi(service_net->id) > atoi(server->id))
    return true;
  return false;
}

state GetStart(ServerNet * service_net,
  Connection * central_server) {
  char cs_buffer[BUFFER_SIZE];
  int tmp;

  memset((void*)&cs_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
  sprintf(cs_buffer, "GET_START %s;%s", service_net->service_id, service_net->id);
  if (verbose_option)
    printf("service.GetStart.request: %s\n", cs_buffer);
  // Enquiring the central server
  int n = sendto(central_server->fd, cs_buffer, sizeof(char)*strlen(cs_buffer),
    0, (struct sockaddr*)&(central_server->addr), sizeof(central_server->addr));
  if (n == -1) {
    char error_buffer[BUFFER_SIZE];
    perror(error_buffer);
    printf("service: sendto() error\n");
    return error;
  }
  // printf("SENT\n");
  // Waiting for response
  memset((void*)&cs_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
  n=recvfrom(central_server->fd,
    cs_buffer,
    BUFFER_SIZE,
    0,(struct sockaddr*)&(central_server->addr), (socklen_t*)&tmp);
  if (n==-1) {
    char error_buffer[BUFFER_SIZE];
    perror(error_buffer);
    printf("service: recvfrom() error\n");
    return error;
  }
  if (verbose_option)
    printf("service.GetStart.response: %s\n", cs_buffer);
  // Check if there was an error
  char join_response[BUFFER_SIZE];
  memset((void*)&join_response, (int)'\0', sizeof(char)*BUFFER_SIZE);
  sprintf(join_response, "OK 0;0;0.0.0.0;0");
  if (strcmp(cs_buffer, join_response)==0) {
    return error;
  }
  // Check if this should start new service
  memset((void*)&join_response, (int)'\0', sizeof(char)*BUFFER_SIZE);
  sprintf(join_response, "OK %s;0;0.0.0.0;0", service_net->id);
  if (strcmp(cs_buffer, join_response)==0) {
    return start;
  }
  // Then we should join a service
  char tmp_token[BUFFER_SIZE], tmp_this_id[BUFFER_SIZE];
  if (sscanf(cs_buffer, "%s %[^;];%[^;];%[^;];%s", tmp_token, tmp_this_id,
    service_net->next_server.id, service_net->next_server.ip,
    service_net->next_server.port) != 5) {
    return error;
  }
  return join;
}

state GetDespatch(ServerNet * service_net,
  Connection * central_server) {
  char cs_buffer[BUFFER_SIZE];
  int tmp;

  memset((void*)&cs_buffer, (int)'\0', BUFFER_SIZE*sizeof(char));
  sprintf(cs_buffer, "GET_DS_SERVER %s", service_net->service_id);
  if (verbose_option)
    printf("service.GetDespatch.request: %s\n", cs_buffer);
  int n = sendto(central_server->fd, cs_buffer, sizeof(char)*strlen(cs_buffer),
    0, (struct sockaddr*)&(central_server->addr), sizeof(central_server->addr));
  if (n == -1) {
    return error;
  }
  memset((void*)&cs_buffer, (int)'\0', BUFFER_SIZE*sizeof(char));
  n=recvfrom(central_server->fd,
    cs_buffer,
    BUFFER_SIZE,
    0,(struct sockaddr*)&(central_server->addr),(socklen_t*)&tmp);
  if (n==-1) {
    return error;
  }
  if (verbose_option)
    printf("service.GetDespatch.response: %s\n", cs_buffer);
  if (strcmp(cs_buffer, "OK 0;0.0.0.0;0") == 0) {
    return busy;
  }
  return available;
}

state SetStart(ServerNet * service_net,
  Connection * central_server) {
  char cs_buffer[BUFFER_SIZE];
  int tmp;

  memset((void*)&cs_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
  sprintf(cs_buffer, "SET_START %s;%s;%s;%s", service_net->service_id,
    service_net->id, service_net->ip, service_net->tcp_port);
  if (verbose_option)
    printf("service.SetStart.request: %s\n", cs_buffer);
  // Informing the central server
  int n_send = sendto(central_server->fd, cs_buffer, sizeof(char)*strlen(cs_buffer),
    0, (struct sockaddr*)&(central_server->addr), sizeof(central_server->addr));
  if (n_send == -1) {
    char error_buffer[1024];
    // memset((void*)&error_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
    perror(error_buffer);
    printf("service.SetStart.sendto error - %s\n", error_buffer);
    return error;
  }
  // printf("SENT\n");
  // Waiting for response
  memset((void*)&cs_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
  int n_recv = recvfrom(central_server->fd, cs_buffer, BUFFER_SIZE*sizeof(char),
    0,(struct sockaddr*)&(central_server->addr), (socklen_t*)&tmp);
  if (n_recv==-1) {
    char error_buffer[1024];
    // memset((void*)&error_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
    perror(error_buffer);
    printf("service.SetStart.recvfrom error - %s\n", error_buffer);
    printf("%s\n", cs_buffer);
    return error;
  }
  if (verbose_option)
    printf("service.SetStart.response: %s\n", cs_buffer);
  char join_response[BUFFER_SIZE];
  memset((void*)&join_response, (int)'\0', sizeof(char)*BUFFER_SIZE);
  sprintf(join_response, "OK %s;0;0.0.0.0;0", service_net->id);
  if (strcmp(cs_buffer, join_response)!=0) {
    printf("service: error with central service\n");
    return error;
  }
  start_state = true;
  return success;
}

state SetDespatch(ServerNet * service_net,
  Connection * central_server) {
  char cs_buffer[128];
  int tmp;

  memset((void*)&cs_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
  sprintf(cs_buffer, "SET_DS %s;%s;%s;%s", service_net->service_id,
    service_net->id, service_net->ip, service_net->udp_port);
  if (verbose_option)
    printf("service.SetDespatch.request: %s\n", cs_buffer);

  // Informing the central server
  int n = sendto(central_server->fd,
    cs_buffer,
    sizeof(char)*strlen(cs_buffer),
    0, (struct sockaddr*)&(central_server->addr),
    sizeof(central_server->addr));
  if (n == -1) {
    printf("service.SetDespatch.sendto error\n");
    return error;
  }
  // printf("SENT\n");
  // Waiting for response
  memset((void*)&cs_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
  n=recvfrom(central_server->fd, cs_buffer, BUFFER_SIZE,
    0,(struct sockaddr*)&(central_server->addr), (socklen_t*)&tmp);
  if (n==-1) {
    char error_buffer[1024];
    perror(error_buffer);
    printf("service.SetDespatch.recvfrom error - %s\n", error_buffer);
    return error;
  }
  if (verbose_option)
    printf("service.SetDespatch.response: %s\n", cs_buffer);
  char join_response[BUFFER_SIZE];
  memset((void*)&join_response, (int)'\0', sizeof(char)*BUFFER_SIZE);
  sprintf(join_response, "OK %s;0;0.0.0.0;0", service_net->id);
  if (strcmp(cs_buffer, join_response)!=0) {
    printf("service: error with central service\n");
    return error;
  }
  despatch_state = true;
  return success;
}

state OpenServiceServer(ServerNet *service_net,
  Connection *listen_server) {
  if (verbose_option)
    printf("service.OpenServiceServer\n");
  memset((void*)&(listen_server->addr), (int)'\0', sizeof(listen_server->addr));
  listen_server->addr.sin_family = AF_INET;
  listen_server->addr.sin_addr.s_addr = htonl(INADDR_ANY);
  listen_server->addr.sin_port = htons(atoi(service_net->tcp_port));
  if (bind(listen_server->fd, (struct sockaddr*)(&listen_server->addr),
    sizeof(listen_server->addr)) == -1) {
    return error;
  }
  if (listen(listen_server->fd, 5) == -1) {
    return error;
  }
  return success;
}

state AcceptServer(ServerNet *service_net,
  Connection *listen_server,
  Connection *prev_server) {
  if (verbose_option)
    printf("service.AcceptServer\n");
  int addrlen = sizeof(listen_server->addr);
  int old_fd = prev_server->fd;
  prev_server->fd = accept(listen_server->fd,
    (struct sockaddr*)&(listen_server->addr), (socklen_t*)&addrlen);
  if (prev_server->fd == -1) {
    prev_server->fd = old_fd;
    return error;
  }

  prev_server->addr = listen_server->addr;
  return success;
}

state ConnectServer(ServerNet * service_net,
  Connection * next_server,
  Server * new_server) {
  if (verbose_option)
    printf("service.ConnectServer\n");
  if (strcmp(new_server->id, service_net->id) == 0) {
    return error;
  }
  if (next_server->fd != -1) {
    close(next_server->fd);
  }
  next_server->fd = socket(AF_INET, SOCK_STREAM, 0);
  if (next_server->fd == -1) {
    return error;
  }
  memset((void*)&(next_server->addr), (int)'\0', sizeof(next_server->addr));
  next_server->addr.sin_family = AF_INET;
  if (inet_aton(new_server->ip, &(next_server->addr.sin_addr)) == 0) {
    return error;
  }
  next_server->addr.sin_port = htons(atoi(new_server->port));
  if (connect(next_server->fd, (struct sockaddr*)&(next_server->addr),
    sizeof(next_server->addr)) == -1) {
    return error;
  }
  strcpy(service_net->next_server.id, new_server->id);
  strcpy(service_net->next_server.ip, new_server->ip);
  strcpy(service_net->next_server.port, new_server->port);
  return success;
}

state ConnectServerfromTokenO(ServerNet * service_net,
  Connection * next_server,
  String * token_buffer) {
  Server new_server;
  if (verbose_option)
    printf("service.ConnectServerfromTokenO: %s\n", token_buffer->string);
  char token_tmp[BUFFER_SIZE], type_tmp[BUFFER_SIZE], leaver_tmp[BUFFER_SIZE];
  if (sscanf(token_buffer->string, "%s %[^;];%[^;];%[^;];%[^;];%[^;]\n", token_tmp,
    leaver_tmp, type_tmp, new_server.id, new_server.ip, new_server.port) != 6) {
    return error;
  }
  if (strcmp(new_server.id, service_net->id) == 0) {
    close(next_server->fd);
    next_server->fd = -1;
    return success;
  }
  next_server->fd = socket(AF_INET, SOCK_STREAM, 0);
  if (next_server->fd == -1) {
    return error;
  }
  memset((void*)&(next_server->addr), (int)'\0', sizeof(next_server->addr));
  next_server->addr.sin_family = AF_INET;
  if (inet_aton(new_server.ip, &(next_server->addr.sin_addr)) == 0) {
    return error;
  }
  next_server->addr.sin_port = htons(atoi(new_server.port));
  // printf("H4\n");
  if (connect(next_server->fd, (struct sockaddr*)&(next_server->addr),
    sizeof(next_server->addr)) == -1) {
    return error;
  }
  strcpy(service_net->next_server.id, new_server.id);
  return success;
}

state ConnectRing(ServerNet * service_net,
  Connection *next_server,
  Connection *listen_server) {
  next_server->fd = socket(AF_INET, SOCK_STREAM, 0);
  // Convert strings to net format and prepare address
  if (verbose_option)
    printf("service.ConnectRing\n");
  memset((void*)&(next_server->addr),
    (int)'\0', sizeof(next_server->addr));
  next_server->addr.sin_family = AF_INET;
  if (inet_aton(service_net->next_server.ip, &next_server->addr.sin_addr)==0) {
    char error_buffer[BUFFER_SIZE];
    perror(error_buffer);
    printf("service.ConnectRing: error with inet_aton\n");
    return error;
  }
  next_server->addr.sin_port = htons(atoi(service_net->next_server.port));
  // Prepare connection
  if (connect(next_server->fd, (struct sockaddr*)&(next_server->addr),
    sizeof(next_server->addr))==-1) {
    char error_buffer[BUFFER_SIZE];
    perror(error_buffer);
    printf("service.ConnectRing: error with connect\n");
    return error;
  }
  return success;
}

state AcceptRing (ServerNet * service_net,
  Connection * listen_server,
  Connection * prev_server) {
  // Connect to new prev server
  int addrlen;
  if (verbose_option)
    printf("service.AcceptRing\n");
  addrlen = sizeof(listen_server->addr);
    if ((prev_server->fd = accept(listen_server->fd,
      (struct sockaddr*)&listen_server->addr, (socklen_t*)&addrlen))==-1) {
      return error;
    }
  prev_server->addr = listen_server->addr;
  return success;
}

state CloseConnections(Connection * prev_server,
  Connection * next_server,
  Connection * client) {
  if (verbose_option)
    printf("service.CloseConnections\n");
  if (next_server->fd != -1) {
    close(next_server->fd);
  }
  next_server->fd = -1;
  if (prev_server->fd != -1) {
    close(prev_server->fd);
  }
  prev_server->fd = -1;
  if (client->fd != -1) {
    close(client->fd);
  }
  client->fd = -1;
  return success;
}

state CloseServers(Connection * prev_server,
  Connection * next_server) {
  if (verbose_option)
    printf("service.CloseServers\n");
  if (prev_server->fd != -1) {
    close(prev_server->fd);
    prev_server->fd = -1;
  }
  if (next_server->fd != -1) {
    close(next_server->fd);
    next_server->fd = -1;
  }
  return success;
}

state WithdrawDespatch(ServerNet * service_net,
  Connection * central_server) {
  char cs_buffer[128];
  int tmp;
  if (despatch_state == false) {
    return success;
  }
  memset((void*)&cs_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
  sprintf(cs_buffer, "WITHDRAW_DS %s;%s", service_net->service_id, service_net->id);
  if (verbose_option)
    printf("service.WithdrawDespatch.request: %s\n", cs_buffer);

  // Informing the central server
  int n_send = sendto(central_server->fd, cs_buffer, sizeof(char)*strlen(cs_buffer),
    0, (struct sockaddr*)&(central_server->addr), sizeof(central_server->addr));
  if (n_send == -1) {
    printf("service.WithdrawDespatch.sendto: error\n");
    return error;
  }
  // printf("SENT\n");
  // Waiting for response
  memset((void*)&cs_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
  int n_recv=recvfrom(central_server->fd, cs_buffer, BUFFER_SIZE,
    0,(struct sockaddr*)&(central_server->addr), (socklen_t*)&tmp);
  if (n_recv==-1) {
    printf("service.WithdrawDespatch.recvfrom: error\n");
    return error;
  }
  if (verbose_option)
    printf("service.WithdrawDespatch.response: %s\n", cs_buffer);
  char join_response[BUFFER_SIZE];
  memset((void*)&join_response, (int)'\0', sizeof(char)*BUFFER_SIZE);
  sprintf(join_response, "OK %s;0;0.0.0.0;0", service_net->id);
  if (strcmp(cs_buffer, join_response)!=0) {
    printf("service.WithdrawDespatch: error with central service\n");
    return error;
  }
  despatch_state = false;
  return success;
}

state WithdrawStart(ServerNet * service_net,
  Connection * central_server) {
  char cs_buffer[128];
  int tmp;

  memset((void*)&cs_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
  sprintf(cs_buffer, "WITHDRAW_START %s;%s", service_net->service_id,
    service_net->id);
  if (verbose_option)
    printf("service.WithdrawStart.request: %s\n", cs_buffer);

  // Informing the central server
  int n_send = sendto(central_server->fd, cs_buffer, sizeof(char)*strlen(cs_buffer),
    0, (struct sockaddr*)&(central_server->addr), sizeof(central_server->addr));
  if (n_send == -1) {
    char error_buffer[1024];
    perror(error_buffer);
    printf("service: sendto() error - %s\n", error_buffer);
    return error;
  }
  // printf("SENT\n");
  // Waiting for response
  memset((void*)&cs_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
  int n_recv=recvfrom(central_server->fd, cs_buffer, BUFFER_SIZE,
    0,(struct sockaddr*)&(central_server->addr), (socklen_t*)&tmp);
  if (n_recv==-1) {
    char error_buffer[1024];
    perror(error_buffer);
    printf("service: recvfrom() error - %s\n", error_buffer);
    return error;
  }
  if (verbose_option)
    printf("service.WithdrawStart.response: %s\n", cs_buffer);
  char join_response[BUFFER_SIZE];
  memset((void*)&join_response, (int)'\0', sizeof(char)*BUFFER_SIZE);
  sprintf(join_response, "OK %s;0;0.0.0.0;0", service_net->id);
  if (strcmp(cs_buffer, join_response)!=0) {
    printf("service: error with central service\n");
    return error;
  }
  start_state = false;
  return success;
}

token HandleToken(ServerNet *service_net,
  Connection *prev_server,
  String * token_buffer) {
  memset((void*)&(token_buffer->string), (int)'\0', sizeof(char)*BUFFER_SIZE);
  int token_read;
  char * token_pointer = &(token_buffer->string)[0];

  while (1) {
    token_read = read(prev_server->fd, token_pointer, BUFFER_SIZE);
    if (token_read <= 0) {
      return error;
    }
    token_pointer += token_read;
    if (strchr(token_buffer->string, '\n') != NULL) {
      break;
    }
  }
  if (verbose_option)
    printf("service.HandleToken: %s", token_buffer->string);
  if (strcmp(token_buffer->string, "NEW_START\n") == 0) {
    return NS;
  }
  char token[BUFFER_SIZE], master_id[BUFFER_SIZE], type[BUFFER_SIZE],
  new_id[BUFFER_SIZE], new_ip[BUFFER_SIZE], new_port[BUFFER_SIZE];
  if (sscanf(token_buffer->string, "%s %[^;];%[^;];%[^;];%[^;];%[^\n]\n", token,
    master_id, type, new_id, new_ip, new_port) == 6) {
    if (strcmp(token, "TOKEN") != 0) {
      return error;
    }
    if (strcmp(type, "O") == 0) {
      return O;
    } else if (strcmp(type, "N") == 0) {
      return N;
    }
    return error;
  } else if (sscanf(token_buffer->string, "%s %[^;];%[^;];%[^\n]\n", token,
    new_id, new_ip, new_port) == 4) {
    if (strcmp(token, "NEW") != 0) {
      return error;
    }
    return NW;
  } else if (sscanf(token_buffer->string, "%s %[^;];%[^\n]\n", token,
    master_id, type) == 3) {
    if (strcmp(token, "TOKEN") != 0) {
      return error;
    }
    if (strcmp(type, "S") == 0) {
      return S;
    } else if (strcmp(type, "T") == 0) {
      return T;
    } else if (strcmp(type, "D") == 0) {
      return D;
    } else if (strcmp(type, "I") == 0) {
      return I;
    } else {
      return error;
    }
  }
  return error;
}

state HandleTokenS(ServerNet * service_net,
  String * token_buffer,
  Server * server) {
  char token_tmp[BUFFER_SIZE], type_tmp[BUFFER_SIZE];
  if (verbose_option)
    printf("service.HandleTokenS: %s\n", token_buffer->string);
  if (sscanf(token_buffer->string, "%s %[^;];%[^\n]\n", token_tmp,
    server->id, type_tmp) != 3) {
    return error;
  }
  if (strcmp(token_tmp, "TOKEN") != 0
    || strcmp(type_tmp, "S") != 0) {
    return error;
  }
  if (strcmp(service_net->id, server->id) == 0
    && (service_state == busy || service_state == leaving)) {
    return closecom;
  } else if (strcmp(service_net->id, server->id) == 0) {
    return error;
  } else if (service_state == available) {
    return handle;
  }
  return pass;
}

state HandleTokenT(ServerNet * service_net,
  String * token_buffer) {
  char token_tmp[BUFFER_SIZE], type_tmp[BUFFER_SIZE], sender_tmp[BUFFER_SIZE];
  if (verbose_option)
    printf("service.HandleTokenT: %s\n", token_buffer->string);
  if (sscanf(token_buffer->string, "%s %[^;];%[^\n]\n", token_tmp,
    sender_tmp, type_tmp) != 3) {
    return error;
  }
  if (strcmp(token_tmp, "TOKEN") != 0
    || strcmp(type_tmp, "T") != 0) {
    return error;
  }
  if (strcmp(sender_tmp, service_net->id) == 0) {
    return handle;
  }
  return pass;
}

state HandleTokenI(ServerNet * service_net,
  String * token_buffer) {
  char token_tmp[BUFFER_SIZE], type_tmp[BUFFER_SIZE], sender_tmp[BUFFER_SIZE];
  if (verbose_option)
    printf("service.HandleTokenI: %s\n", token_buffer->string);
  if (sscanf(token_buffer->string, "%s %[^;];%[^\n]\n", token_tmp,
    sender_tmp, type_tmp) != 3) {
    return error;
  }
  if (strcmp(token_tmp, "TOKEN") != 0
    || strcmp(type_tmp, "I") != 0) {
    return error;
  }
  ring_state = busy;
  if (strcmp(sender_tmp, service_net->id) == 0) {
    return handle;
  }
  return pass;
}

state HandleTokenD(ServerNet * service_net,
  String * token_buffer) {
  Server server;
  char token_tmp[BUFFER_SIZE], type_tmp[BUFFER_SIZE];
  if (verbose_option)
    printf("service.HandleTokenD: %s\n", token_buffer->string);
  if (sscanf(token_buffer->string, "%s %[^;];%[^\n]\n", token_tmp,
    server.id, type_tmp) != 3) {
    return error;
  }
  if (strcmp(token_tmp, "TOKEN") != 0
    || strcmp(type_tmp, "D") != 0) {
    return error;
  }
  ring_state = available;
  if (strcmp(server.id, service_net->id) == 0) {
    if (verbose_option)
      printf("service.HandleTokenD: handling\n");
    return handle;
  } else if (joinning_ring == true
    && BiggerID(service_net, &server)) {
    if (verbose_option)
      printf("service.HandleTokenD: ignoring\n");
    return ignore;
  }
  if (verbose_option)
    printf("service.HandleTokenD: passing\n");
  return pass;
}

state HandleTokenNW(ServerNet * service_net,
  Connection * next_server,
  String * token_buffer) {
  char token_tmp[BUFFER_SIZE], id_tmp[BUFFER_SIZE], ip_tmp[BUFFER_SIZE],
    port_tmp[BUFFER_SIZE];
  if (verbose_option)
    printf("service.HandleTokenNW: %s", token_buffer->string);
  sscanf(token_buffer->string, "%s %[^;];%[^;];%[^\n]\n", token_tmp,
    id_tmp, ip_tmp, port_tmp);
  if (strcmp(id_tmp, service_net->id) == 0) {
    return error;
  }
  if (next_server->fd == -1) {
    return handle;
  }
  return pass;
}

state HandleTokenN(ServerNet * service_net,
  Connection * next_server,
  String * token_buffer) {
  Server new_server;
  char token_tmp[BUFFER_SIZE], type_tmp[BUFFER_SIZE], master_tmp[BUFFER_SIZE];
  if (verbose_option)
    printf("service.HandleTokenN: %s\n", token_buffer->string);
  if (sscanf(token_buffer->string, "%s %[^;];%[^;];%[^;];%[^;];%[^\n]\n", token_tmp,
    master_tmp, type_tmp, new_server.id, new_server.ip, new_server.port) != 6) {
    return error;
  }
  if (strcmp(token_tmp, "TOKEN") != 0
    || strcmp(type_tmp, "N") != 0) {
    return error;
  }
  if (strcmp(service_net->id, master_tmp) == 0) {
    printf("service.HandleTokenN: repeated ID\n");
    return error;
  }
  if (strcmp(service_net->next_server.id, master_tmp) == 0) {
    return handle;
  }
  return pass;
}

state HandleTokenO(ServerNet * service_net,
  Connection * next_server,
  String * token_buffer) {
  Server new_server;
  char token_tmp[BUFFER_SIZE], type_tmp[BUFFER_SIZE], leaver[BUFFER_SIZE];
  if (verbose_option)
    printf("service.HandleTokenO: %s\n", token_buffer->string);
  if (sscanf(token_buffer->string, "%s %[^;];%[^;];%[^;];%[^;];%[^\n]\n", token_tmp,
    leaver, type_tmp, new_server.id, new_server.ip, new_server.port) != 6) {
    return error;
  }
  if (strcmp(token_tmp, "TOKEN") != 0) {
    return error;
  }
  // Server ID
  if (strcmp(leaver, service_net->id) == 0) {
    return closecom;
  } else if (strcmp(leaver, service_net->next_server.id) == 0) {
    if (strcmp(service_net->id, new_server.id) == 0) {
      return closeservercom;
    } else {
      return handle;
    }
  }
  return pass;
}

state HandleTokenNS() {
  if (verbose_option)
    printf("service.HandleTokenNS\n");
  return handle;
}

state SendTokenS(ServerNet * service_net,
  Connection * next_server) {
  if (next_server->fd == -1) {
    return error;
  } else {
    char token_buffer[BUFFER_SIZE];
    memset((void*)&token_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
    sprintf(token_buffer, "TOKEN %s;S\n", service_net->id);
    int token_left = strlen(token_buffer) * sizeof(char);
    if (verbose_option)
      printf("service.SendTokenS: %s\n", token_buffer);
    int token_written;
    char *token_pointer = &token_buffer[0];
    while (token_left > 0) {
      token_written = write(next_server->fd, token_pointer, token_left);
      if (token_written <= 0) {
        return error;
      }
      token_left -= token_written;
      token_pointer += token_written;
    }
  }
  return success;
}

state SendTokenT(ServerNet * service_net,
  Connection * next_server,
  Server * server) {
  if (next_server->fd == -1) {
    return error;
  } else {
    char token_buffer[BUFFER_SIZE];
    memset((void*)&token_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
    sprintf(token_buffer, "TOKEN %s;T\n", server->id);
      if (verbose_option)
    printf("service.SendTokenT: %s\n", token_buffer);
    int token_left = strlen(token_buffer) * sizeof(char);
    int token_written;
    char *token_pointer = &token_buffer[0];
    while (token_left > 0) {
      token_written = write(next_server->fd, token_pointer, token_left);
      if (token_written <= 0) {
        return error;
      }
      token_left -= token_written;
      token_pointer += token_written;
    }
  }
  return success;
}

state SendTokenI(ServerNet * service_net,
  Connection * next_server) {
  if (next_server->fd == -1) {
    return success;
  } else {
    char token_buffer[BUFFER_SIZE];
    memset((void*)&token_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
    sprintf(token_buffer, "TOKEN %s;I\n", service_net->id);
    if (verbose_option)
      printf("service.SendTokenI: %s\n", token_buffer);
    int token_left = strlen(token_buffer) * sizeof(char);
    int token_written;
    char *token_pointer = &token_buffer[0];
    while (token_left > 0) {
      token_written = write(next_server->fd, token_pointer, token_left);
      if (token_written <= 0) {
        return error;
      }
      token_left -= token_written;
      token_pointer += token_written;
    }
  }
  return success;
}

state SendTokenD(ServerNet * service_net,
  Connection * next_server) {
  if (next_server->fd == -1) {
    return error;
  } else {
    char token_buffer[BUFFER_SIZE];
    memset((void*)&token_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
    sprintf(token_buffer, "TOKEN %s;D\n", service_net->id);
    if (verbose_option)
      printf("service.SendTokenD: %s\n", token_buffer);
    int token_left = strlen(token_buffer) * sizeof(char);
    int token_written;
    char *token_pointer = &token_buffer[0];
    while (token_left > 0) {
      token_written = write(next_server->fd, token_pointer, token_left);
      if (token_written <= 0) {
        return error;
      }
      token_left -= token_written;
      token_pointer += token_written;
    }
  }
  return success;
}

state SendTokenNW(ServerNet * service_net,
  Connection * next_server) {
  char token_buffer[BUFFER_SIZE];
  memset((void*)&token_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
  sprintf(token_buffer, "NEW %s;%s;%s\n", service_net->id, service_net->ip,
    service_net->tcp_port);
  if (verbose_option)
    printf("service.SendTokenNW: %s\n", token_buffer);
  int token_left = strlen(token_buffer) * sizeof(char);
  int token_written;
  char *token_pointer = &token_buffer[0];
  while (token_left > 0) {
    token_written = write(next_server->fd, token_pointer, token_left);
    if (token_written <= 0) {
      return error;
    }
    token_left -= token_written;
    token_pointer += token_written;
  }
  return success;
}

state SendTokenN(ServerNet * service_net,
  Connection * next_server,
  Server * server) {

  char token_buffer[BUFFER_SIZE];
  memset((void*)&token_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
  sprintf(token_buffer, "TOKEN %s;N;%s;%s;%s\n", service_net->id, server->id,
    server->ip, server->port);
  if (verbose_option)
    printf("service.SendTokenN: %s\n", token_buffer);
  int token_left = strlen(token_buffer) * sizeof(char);
  int token_written;
  char *token_pointer = &(token_buffer)[0];
  while (token_left > 0) {
    token_written = write(next_server->fd, token_pointer, token_left);
    if (token_written <= 0) {
      return error;
    }
    token_left -= token_written;
    token_pointer += token_written;
  }
  return success;
}

state SendTokenO(ServerNet * service_net,
  Connection * next_server) {
  if (next_server->fd == -1) {
    waiting_to_leave = false;
    return handle;
  }
  char new_token_buffer[BUFFER_SIZE];
  memset((void*)&new_token_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
  sprintf(new_token_buffer, "TOKEN %s;O;%s;%s;%s\n",
    service_net->id, service_net->next_server.id, service_net->next_server.ip,
    service_net->next_server.port);
  int token_left = strlen(new_token_buffer) * sizeof(char);
  int token_written;
  char *token_pointer = &new_token_buffer[0];
  if (verbose_option)
    printf("service.SendTokenO: %s\n", new_token_buffer);
  while (token_left > 0) {
    token_written = write(next_server->fd, token_pointer, token_left);
    if (token_written <= 0) {
      return error;
    }
    token_left -= token_written;
    token_pointer += token_written;
  }
  waiting_to_leave = true;
  return success;
}

state SendTokenNS(ServerNet * service_net,
  Connection * next_server) {
  if (next_server->fd == -1) {
    return handle;
  } else {
    char token_buffer[BUFFER_SIZE];
    memset((void*)&token_buffer, (int)'\0', sizeof(char)*BUFFER_SIZE);
    strcpy(token_buffer, "NEW_START\n");
    if (verbose_option)
      printf("service.SendTokenNS: %s\n", token_buffer);
    int token_left = strlen(token_buffer) * sizeof(char);
    int token_written;
    char *token_pointer = &token_buffer[0];
    while (token_left > 0) {
      token_written = write(next_server->fd, token_pointer, token_left);
      if (token_written <= 0) {
        return error;
      }
      token_left -= token_written;
      token_pointer += token_written;
    }
  }
  return success;
}

state PassToken(Connection * next_server,
  String * token_buffer) {
  if (next_server->fd == -1) {
    return success;
  }
  int token_left = strlen(token_buffer->string) * sizeof(char);
  int token_written;
  char *token_pointer = &(token_buffer->string)[0];
  if (verbose_option)
    printf("service.PassToken: %s\n", token_buffer->string);
  while (token_left > 0) {
    token_written = write(next_server->fd, token_pointer, token_left);
    if (token_written <= 0) {
      return error;
    }
    token_left -= token_written;
    token_pointer += token_written;
  }
  return success;
}

state ConvertTokenO(String * token_buffer,
  Server * server) {
  if (verbose_option)
    printf("service.ConvertTokenO: %s\n", token_buffer->string);
  char token_tmp[BUFFER_SIZE], type_tmp[BUFFER_SIZE], leaver_tmp[BUFFER_SIZE];
  if (sscanf(token_buffer->string, "%s %[^;];%[^;];%[^;];%[^;];%[^;]\n", token_tmp,
    leaver_tmp, type_tmp, server->id, server->ip, server->port) != 6) {
    return error;
  }
  return success;
}

state ConvertTokenN(String * token_buffer,
  Server * server) {
  if (verbose_option)
    printf("service.ConvertTokenN: %s\n", token_buffer->string);
  char token_tmp[BUFFER_SIZE], type_tmp[BUFFER_SIZE], leaver_tmp[BUFFER_SIZE];
  if (sscanf(token_buffer->string, "%s %[^;];%[^;];%[^;];%[^;];%[^\n]\n", token_tmp,
    leaver_tmp, type_tmp, server->id, server->ip, server->port) != 6) {
    return error;
  }
  return success;
}

state ConvertTokenNW(String * token_buffer,
  Server * server) {
  char token_tmp[BUFFER_SIZE];
  if (verbose_option)
    printf("service.ConvertTokenNW: %s\n", token_buffer->string);
  if (sscanf(token_buffer->string, "%s %[^;];%[^;];%[^\n]\n", token_tmp,
    server->id,  server->ip, server->port) != 4) {
    return error;
  }
  return success;
}

state OpenClientServer(ServerNet *service_net,
  Connection *client) {
  if (verbose_option)
    printf("service.OpenClientServer\n");
  client->fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (client->fd == -1) {
    return error;
  }
  if (setsockopt(client->fd, SOL_SOCKET, SO_RCVTIMEO,
    &timeout_udp, sizeof(timeout_udp)) < 0) {
    printf("service.OpenClientServer: socket options error\n");
  }
  memset((void*)&(client->addr), (int)'\0', sizeof(client->addr));
  client->addr.sin_family = AF_INET;
  client->addr.sin_addr.s_addr = htonl(INADDR_ANY);
  client->addr.sin_port = htons(atoi(service_net->udp_port));
  if (bind(client->fd, (struct sockaddr*)&(client->addr), sizeof(client->addr)) == -1) {
    char error_buffer[BUFFER_SIZE];
    perror(error_buffer);
    printf("service.OpenClientServer: error binding\n");
    return error;
  }
  return success;
}

state HandleClient(Connection *client) {
  int tmp = sizeof(client->addr);
  char client_buffer[BUFFER_SIZE];
  memset((void*)&(client_buffer), (int)'\0', sizeof(char)*BUFFER_SIZE);
  if (recvfrom(client->fd,
    client_buffer,
    BUFFER_SIZE*sizeof(char), 0,
    (struct sockaddr*)&(client->addr), (socklen_t*)&tmp) == -1) {
    return error;
  }
  if (verbose_option)
    printf("service.HandleClient.request: %s\n", client_buffer);
  if (strcmp(client_buffer, "MY_SERVICE ON") == 0
    && service_state == available) {
    memset((void*)&(client_buffer), (int)'\0', sizeof(char)*BUFFER_SIZE);
    strcpy(client_buffer, "YOUR_SERVICE ON");
    if (verbose_option)
      printf("service: response: %s\n", client_buffer);
    if (sendto(client->fd, client_buffer,
      sizeof(char)*strlen(client_buffer),
      0, (struct sockaddr*)&(client->addr), tmp) == -1) {
      return error;
    }
    return start_service;
  } else if (strcmp(client_buffer, "MY_SERVICE OFF") == 0
    && service_state == busy) {
    memset((void*)&(client_buffer), (int)'\0', sizeof(char)*BUFFER_SIZE);
    strcpy(client_buffer, "YOUR_SERVICE OFF");
    if (verbose_option)
      printf("service.HandleClient.response: %s\n", client_buffer);
    if (sendto(client->fd, client_buffer,
      sizeof(char)*strlen(client_buffer),
      0, (struct sockaddr*)&(client->addr), tmp) == -1) {
      return error;
    }
    return terminate_service;
  }
  return invalid_service;
}

state CloseClient(Connection *client) {
  if (verbose_option)
    printf("service.CloseClient\n");
  if (client->fd == -1) {
    return success;
  }
  close(client->fd);
  return success;
}

int main(int argc, char const *argv[])
{
  // Struct with all data
  ServerNet service_net;
  
  // UDP timeout times
  timeout_udp.tv_sec = 2;
  timeout_udp.tv_usec = 0;

  // Client timout times
  timeout_client.tv_sec = 300;
  timeout_client.tv_usec = 0;

  // SIGPIPE protection
  void (*sigpipe_handler)(int);
  if ((sigpipe_handler = signal(SIGPIPE, SIG_IGN)) == SIG_ERR) {
    printf("service: pipe assignement error\n");
  }

  // Socket descriptors
  Connection central_server;
  memset((void*)&central_server.addr, (int)'\0', sizeof(central_server.addr));
  central_server.addr.sin_family = AF_INET;
  central_server.fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (central_server.fd == -1) {
    printf("service: socket() error\n");
    exit(1);
  }
  if (setsockopt(central_server.fd, SOL_SOCKET, SO_RCVTIMEO,
    &timeout_udp, sizeof(timeout_udp)) < 0) {
    printf("service: socket options error\n");
  }

  Connection listen_server;
  listen_server.fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_server.fd == -1) {
    printf("service: socket() error\n");
    exit(1);
  }

  Connection next_server, prev_server, client;
  next_server.fd = -1;
  prev_server.fd = -1;
  client.fd = -1;

  // Evaluate arguments
  struct hostent *h;
  bool csip_acquired = false;
  bool cspt_acquired = false;
  bool id_acquired = false;
  bool ip_acquired = false;
  bool upt_acquired = false;
  bool tpt_acquired = false;
  for (size_t i=1; i<argc; i++) {
    // CS IP
    if (strcmp("-i",argv[i])==0 
      && csip_acquired==false
      && argc > i) {
      h = gethostbyname(argv[i+1]);
      if (h==NULL){
        printf("service: not able to connect to %s\n", argv[i+1]);
        continue;
      }
      central_server.addr.sin_addr = *(struct in_addr*)h->h_addr_list[0];
      csip_acquired = true;
      printf("service: acquired ip\n");
      i++;
    }
    // CS port
    else if (strcmp("-p",argv[i])==0
      && cspt_acquired==false
      && argc > i) {
      central_server.addr.sin_port = htons(atoi(argv[i+1]));
      cspt_acquired = true;
      printf("service: acquired pt\n");
      i++;
    }
    // ID
    else if (strcmp("-n", argv[i])==0
      && id_acquired == false
      && argc > i) {
      strcpy(service_net.id, argv[i+1]);
      int tmp_id;
      if (sscanf(service_net.id, "%d", &tmp_id) != 1
        || tmp_id <= 0) {
        printf("service: ID must be positive\n");
        return 1;
      }
      id_acquired = true;
      printf("service: acquired id\n");
      i++;
    }
    // IP
    else if (strcmp("-j", argv[i])==0
      && ip_acquired == false
      && argc > i) {
      strcpy(service_net.ip, argv[i+1]);
      ip_acquired = true;
      printf("service: acquired ip\n");
      i++;
    }
    // UDP port
    else if (strcmp("-u", argv[i])==0
      && upt_acquired == false
      && argc > i) {
      strcpy(service_net.udp_port, argv[i+1]);
      upt_acquired = true;
      printf("service: acquired udp port\n");
      i++;
    }
    // TCP port
    else if (strcmp("-t", argv[i])==0
      && tpt_acquired == false
      && argc > i) {
      strcpy(service_net.tcp_port, argv[i+1]);
      tpt_acquired = true;
      printf("service: acquired tcp port\n");
      i++;
    } else if (strcmp("-v", argv[i]) == 0) {
      printf("service: debug info will be printed :)\n");
      verbose_option = true;
    } else if (strcmp("-h", argv[i]) == 0) {
      printf("-n <service id>\n");
      printf("-j <service ip>\n");
      printf("-u <service udp port>\n");
      printf("-t <service tcp port>\n");
      printf("-i <central server's ip> -- not mandatory\n");
      printf("-p <central server's port> -- not mandatory\n");
      printf("-v -- print debug info during runtime -- not mandatory\n");
      printf("-h -- show info -- not mandatory\n");
      printf("./build/service –n id –j ip -u upt –t tpt [-i csip] [-p cspt] [-v] [-h]\n");
      return 0;
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

  // If the central server arguments were not used as args, use defaults
  if (csip_acquired == false) {
    h = gethostbyname("tejo.tecnico.ulisboa.pt");
    if (h==NULL){
      printf("service: not able to connect to tejo\n");
      return -1;
    }
    central_server.addr.sin_addr = *(struct in_addr*)h->h_addr_list[0];
  }
  if (cspt_acquired == false) {
    central_server.addr.sin_port = htons(59000);
  }

  // Parameters necessary fo running the program
  char kb_buffer[BUFFER_SIZE];
  int max_fd = -1;
  fd_set rfds;
  state tmp_state;
  // Start listening to the TCP port
  OpenServiceServer(&service_net, &listen_server);
  // printf("user: ");
  fflush(stdout);

  while (service_state != exiting) {
    // Preparing multitasking
    FD_ZERO(&rfds);
    FD_SET(listen_server.fd, &rfds);
    max_fd = listen_server.fd;
    FD_SET(fileno(stdin), &rfds);
    max_fd = Max(max_fd, fileno(stdin));
    // If it's available, listen to clients
    if (service_state == available ||
      service_state == busy) {
      FD_SET(client.fd, &rfds);
      max_fd = Max(max_fd, client.fd);
    }
    // If it has a server connected
    if (prev_server.fd != -1) {
      FD_SET(prev_server.fd, &rfds);
      max_fd = Max(max_fd, prev_server.fd);
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
        switch (GetStart(&service_net, &central_server)) {
          case start :
            if (SetStart(&service_net, &central_server) == success
              && SetDespatch(&service_net, &central_server) == success
              && OpenClientServer(&service_net, &client) == success) {
              printf("service: start success\n");
              service_state = available;
              ring_state = available;
              start_state = true;
              despatch_state = true;
            } else {
              goto join_error;
            }
            break;
          case join :
            if (ConnectRing(&service_net, &next_server,
                &listen_server) == success
              && SendTokenNW(&service_net, &next_server) == success
              && AcceptRing(&service_net, &listen_server, &prev_server) == success
              && OpenClientServer(&service_net, &client) == success) {
              state despatch_state = GetDespatch(&service_net, &central_server);
              if (despatch_state == busy
                && SendTokenD(&service_net, &next_server) == success) {
                service_state = available;
                ring_state = busy;
              } else if (despatch_state == available) {
                service_state = available;
                ring_state = available;
              } else {
                goto join_error;
              }
              printf("service: join success\n");
            } else {
              goto join_error;
            }
            break;
          default :
            join_error :
            if (start_state == true) {
              WithdrawStart(&service_net, &central_server);
            }
            if (despatch_state == true) {
              WithdrawDespatch(&service_net, &central_server);
            }
            if (client.fd != -1) {
              CloseClient(&client);
            }
            printf("service: error joining service\n");
            break;
        }
      } else if (strcmp(splitted_buffer, "show_state")==0) {
        printf("service: ");
        // Self info
        switch (service_state){
          case disconnected :
            printf("Disconnected - ");
            break;
          case available :
            printf("Connected & Available - ");
            break;
          case busy :
            printf("Connected & Busy - ");
            break;
        }
        // Ring info
        switch (ring_state) {
          case available :
            printf("Ring Available - ");
            break;
          case busy :
            printf("Ring Busy - ");
            break;
          case disconnected :
            printf("Not connected to Ring - ");
            break;
        }
        // Successor info
        if (next_server.fd == -1) {
          printf("NextServer: Empty\n");
        } else {
          printf("NextServer: %s\n", service_net.next_server.id);
        }
      } else if (strcmp(splitted_buffer, "leave")==0
        && service_state == available) {
        if (next_server.fd == -1) {
          if (WithdrawStart(&service_net, &central_server) != success) {
            goto invalid;
          }
          if (WithdrawDespatch(&service_net, &central_server) != success) {
            goto invalid;
          }
          if (CloseConnections(&prev_server, &next_server, &client) != success) {
            goto invalid;
          }
          service_state = disconnected;
          ring_state = disconnected;
          printf("service: disconnected\n");
        } else {
          service_state = leaving;
          if (start_state == true) {
            if (WithdrawStart(&service_net, &central_server) != success) {
              goto invalid;
            }
            if (SendTokenNS(&service_net, &next_server) != success) {
              goto invalid;
            }
          }
          if (despatch_state == true) {
            if (WithdrawDespatch(&service_net, &central_server) != success) {
              goto invalid;
            }
            if (SendTokenS(&service_net, &next_server) != success) {
              goto invalid;
            } else {
              goto jumpO;
            }
          }
          if (SendTokenO(&service_net, &next_server) != success) {
            goto invalid;
          }
          jumpO:
          if (verbose_option)
            printf("service: leaving\n");
        }
      } else if (strcmp(splitted_buffer, "exit")==0
      && service_state == disconnected) {
      service_state = exiting;
      printf("service: exiting\n");
      } else {
        invalid:
        printf("service: invalid\n");
      }
      // printf("user: ");
      fflush(stdout);
    }

    // Socket action - new server contacting
    if (listen_server.fd != -1
      && FD_ISSET(listen_server.fd, &rfds)) {
      if (verbose_option)
        printf("service: accepting new server\n");
      switch(AcceptServer(&service_net, &listen_server, &prev_server)) {
        case success :
          printf("service: server connected\n");
          break;
        default :
          printf("service: server invalid \n");
          break;
      }
    }
    // Socket action - previous
    if (prev_server.fd != -1
      && FD_ISSET(prev_server.fd, &rfds)) {
      String token;
      Server new_server;
      // printf("service: token detected\n");
      switch(HandleToken(&service_net, &prev_server, &token)) {
        case S :
          tmp_state = HandleTokenS(&service_net, &token, &new_server);
          if (tmp_state == handle) {
            // Set this server as despatch
            if (SendTokenT(&service_net, &next_server, &new_server) != success) {
              goto error_tokenS;
            }
            if (SetDespatch(&service_net, &central_server) != success) {
              goto error_tokenS;
            }
          } else if (tmp_state == closecom) {
            // Warn ring is unavailable
            if (SendTokenI(&service_net, &next_server) != success) {
              goto error_tokenS;
            }
          } else if (tmp_state == pass) {
            // Pass token
            if (PassToken(&next_server, &token) != success) {
              goto error_tokenS;
            }
          } else {
            error_tokenS:
            printf("service: error handling tokenS\n");
          }
          break;
        case T :
          tmp_state = HandleTokenT(&service_net, &token);
          if (tmp_state == handle && service_state == leaving) {
            if (SendTokenO(&service_net, &next_server) != success) {
              goto error_tokenT;
            }
          } else if (tmp_state == handle) {
            // do nothing
          } else if (tmp_state == pass) {
            if (PassToken(&next_server, &token) != success) {
              goto error_tokenT;
            }
          } else {
            error_tokenT:
            printf("service: error handling tokenT\n");
          }
          break;
        case I :
          tmp_state = HandleTokenI(&service_net, &token);
          if (tmp_state == handle && service_state == leaving) {
            if (SendTokenO(&service_net, &next_server) != success) {
              goto error_tokenI;
            }
          } else if (tmp_state == handle && service_state == busy) {
            // Do nothing
          } else if (tmp_state == pass) {
            if (PassToken(&next_server, &token) != success) {
              goto error_tokenI;
            }
          } else {
            error_tokenI:
            printf("service: error handling tokenI\n");
          }
          break;
        case D :
          tmp_state = HandleTokenD(&service_net, &token);
          if (tmp_state == pass) {
            if (PassToken(&next_server, &token) != success) {
              goto error_tokenD;
            }
          } else if (tmp_state == ignore) {
            // Do nothing
          } else if (tmp_state == handle) {
            if (SetDespatch(&service_net, &central_server) != success) {
              goto error_tokenD;
            }
          } else {
            error_tokenD:
            printf("service: error handling tokenD\n");
          }
          break;
        case NW :
          tmp_state = HandleTokenNW(&service_net, &next_server, &token);
          if (tmp_state == handle) {
            if (ConvertTokenNW(&token, &new_server) != success) {
              goto error_tokenNW;
            }
            if (ConnectServer(&service_net, &next_server, &new_server) != success) {
              goto error_tokenNW;
            }
          } else if (tmp_state == pass) {
            if (ConvertTokenNW(&token, &new_server) != success) {
              goto error_tokenNW;
            }
            if (SendTokenN(&service_net, &next_server, &new_server) != success) {
              goto error_tokenNW;
            }
          } else {
            error_tokenNW:
            printf("service: error handling tokenNW\n");
          }
          break;
        case N :
          tmp_state = HandleTokenN(&service_net, &next_server, &token);
          if (tmp_state == handle) {
            if (ConvertTokenN(&token, &new_server) != success) {
              goto error_tokenN;
            }
            if (ConnectServer(&service_net, &next_server, &new_server) != success) {
              goto error_tokenN;
            }
          } else if (tmp_state == pass) {
            if (PassToken(&next_server, &token) != success) {
              goto error_tokenN;
            }
          } else {
            error_tokenN:
            printf("service: error handling tokenN\n");
          }
          break;
        // token O
        case O :
          tmp_state = HandleTokenO(&service_net, &next_server, &token);
          if (tmp_state == closecom) {
            if (CloseConnections(&prev_server, &next_server, &client) != success) {
              goto error_tokenO;
            }
            printf("service: disconnected\n");
            service_state = disconnected;
            ring_state = disconnected;
          }else if (tmp_state == handle) {
            // This server connects to new server
            if (PassToken(&next_server, &token) != success) {
              goto error_tokenO;
            }
            if (ConvertTokenO(&token, &new_server) != success) {
              goto error_tokenO;
            }
            if (ConnectServer(&service_net, &next_server, &new_server) != success) {
              goto error_tokenO;
            }
          } else if (tmp_state == closeservercom) {
            if (PassToken(&next_server, &token) != success) {
              goto error_tokenO;
            }
            if (CloseServers(&prev_server, &next_server) != success) {
              goto error_tokenO;
            }
          } else if (tmp_state == pass) {
            if (PassToken(&next_server, &token) != success) {
              goto error_tokenO;
            }
          } else {
            error_tokenO:
            printf("service: error handling tokenO\n");
          }
          break;
        case NS :
          tmp_state = HandleTokenNS();
          if (tmp_state == handle) {
            if (SetStart(&service_net, &central_server) != success) {
              goto error_tokenNS;
            }
          } else {
            error_tokenNS:
            printf("service: error handling tokenNS");
          }
          break;
      }
    }
    // Socket action - client
    if (client.fd != -1
      && FD_ISSET(client.fd, &rfds)) {
      switch (HandleClient(&client)) {
        case start_service :
          joinning_ring = false;
          if (despatch_state == true && next_server.fd == -1) {
            if (WithdrawDespatch(&service_net, &central_server) == success) {
              ring_state = busy;
            } else {
              goto error_client;
            }
          } else if (despatch_state == true
            && (WithdrawDespatch(&service_net, &central_server) != success
            || SendTokenS(&service_net, &next_server) != success)) {
            goto error_client;
          } else {
            // Do nothing
          }
          service_state = busy;
          break;
        case terminate_service :
          joinning_ring = true;
          if (next_server.fd == -1) {
            if (SetDespatch(&service_net, &central_server) == success) {
              ring_state = available;
            } else {
              goto error_client;
            }
          } else if (ring_state == busy
            && SendTokenD(&service_net, &next_server) != success) {
            goto error_client;
          } else {
            // Do nothing
          }
          service_state = available;
          break;
        default :
          error_client:
          printf("service: error handling client\n");
          break;
      }
    }
  }

  return 0;
}