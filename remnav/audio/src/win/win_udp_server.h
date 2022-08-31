#ifndef INCLUDED_win_udp_server_h
#define INCLUDED_win_udp_server_h

#include <winsock2.h>
#include "udp_server_exception.h"

namespace audio {

class UdpServer {
public:
  UdpServer(u_short port);
  ~UdpServer();

  int recv(char *msg, int max_size);
  int send(char* msg, int msg_size);

private:
  SOCKET              socket_desc;
  SOCKADDR_IN         server_addr;
  SOCKADDR_IN         client_addr;
};

}
#endif
