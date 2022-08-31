#include "lnx_udp_client.h"
#include <string>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

#include "udp_client_exception.h"

using namespace audio;

UdpClient::UdpClient(const std::string& addr, int port) {
  char decimal_port[16];
  snprintf(decimal_port, sizeof(decimal_port), "%d", port);
  decimal_port[sizeof(decimal_port) / sizeof(decimal_port[0]) - 1] = '\0';

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;

  int r = getaddrinfo(addr.c_str(), decimal_port, &hints, &addrinfo);
  if(r != 0 || addrinfo == NULL) {
      throw UdpClientException(("invalid address or port for UDP socket: \"" 
                                      + addr + ":" + decimal_port + "\"").c_str());
  }

  socket_desc = socket(addrinfo->ai_family, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP);
  if(socket_desc == -1) {
      freeaddrinfo(addrinfo);
      throw UdpClientException(("could not create UDP socket for: \"" 
                                     + addr + ":" + decimal_port + "\"").c_str());
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = inet_addr(addr.c_str());

  // connect to server
  if(connect(socket_desc, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    throw UdpClientException("Could not connect to server");    
  }

  printf(">>Connected to server '%s' at port %d\n", addr.c_str(), port);

}

UdpClient::~UdpClient() {
  freeaddrinfo(addrinfo);
  close(socket_desc);
}

int UdpClient::recv(char *msg, size_t max_size) {
  return ::recvfrom(socket_desc, msg, max_size, 0, (struct sockaddr*) NULL, NULL);
}


int UdpClient::send(char* msg, size_t msg_size) {
  return ::sendto(socket_desc, msg, msg_size, 0, (const sockaddr*)NULL, sizeof(server_addr));
}
