#ifndef INCLUDED_udp_server_h
#define INCLUDED_udp_server_h

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "udp_server_exception.h"

namespace audio {

class UdpServer {
public:
  UdpServer(int port);
  ~UdpServer();

  int recv(char *msg, size_t max_size);
  int send(char* msg, size_t msg_size);

private:
  int                 socket_desc;
  struct addrinfo *   addrinfo;
  struct sockaddr_in  client_addr;
};

}
#endif