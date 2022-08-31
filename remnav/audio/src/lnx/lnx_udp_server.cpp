#include "lnx_udp_server.h"
#include <string>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

using namespace audio;

UdpServer::UdpServer(int port) {

  struct sockaddr_in serveraddr;
  
  bzero((char*) &serveraddr, sizeof(serveraddr));

  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short) port);

  socket_desc = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP);
  if(socket_desc == -1) {
      throw UdpServerException("Could not create UDP socket for speaker");  
  }

  int r = bind(socket_desc, (struct sockaddr*) &serveraddr, sizeof(serveraddr));
  if(r != 0) {
      close(socket_desc);
      throw UdpServerException("could not bind UDP socket");
  }

}

UdpServer::~UdpServer() {
  close(socket_desc);
}

int UdpServer::recv(char *msg, size_t max_size) {
  int client_struct_len = sizeof(client_addr);
  return ::recvfrom(socket_desc, msg, max_size, 0, 
                    (struct sockaddr*) &client_addr, (socklen_t*) &client_struct_len);
}


int UdpServer::send(char* msg, size_t msg_size) {
  return ::sendto(socket_desc, msg, msg_size, 0, (const sockaddr*)&client_addr, sizeof(client_addr));
}
