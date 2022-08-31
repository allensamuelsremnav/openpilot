#ifndef AUDIO_UDP_CLIENT_H
#define AUDIO_UDP_CLIENT_H
#include <winsock2.h>

#include "udp_client_exception.h"

namespace audio {

class UdpClient {
public:
  UdpClient(const std::string& addr, u_short port);
  ~UdpClient();

  int recv(char *msg, size_t max_size);
  int send(char* msg, size_t msg_size);

private:
  SOCKET socket_desc;
  SOCKADDR_IN server_addr;
};

}

#endif