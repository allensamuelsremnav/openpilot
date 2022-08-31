#ifndef AUDIO_UDP_CLIENT_H
#define AUDIO_UDP_CLIENT_H
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "udp_client_exception.h"

namespace audio {

class UdpClient {
public:
  UdpClient(const std::string& addr, int port);
  ~UdpClient();

  int recv(char *msg, size_t max_size);
  int send(char* msg, size_t msg_size);

private:
  int                 socket_desc;
  struct addrinfo *   addrinfo;
  struct sockaddr_in  server_addr;
};

}

#endif