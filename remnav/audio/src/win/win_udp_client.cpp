#include "win_udp_client.h"
#include <string>

#include "udp_client_exception.h"

#include <Ws2tcpip.h>
using namespace audio;

UdpClient::UdpClient(const std::string& addr, u_short port) {
  //create socket
  if ( (socket_desc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR) {
    printf("socket() failed with error code : %d" , WSAGetLastError());
    throw UdpClientException("socket failed"); 
  }
  
  //setup address structure
  memset((char *) &server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = (unsigned short) htons(port);
  InetPton(AF_INET, addr.c_str(), &server_addr.sin_addr.s_addr);
  //server_addr.sin_addr.S_un.S_addr = inet_pton(AF_INET, addr.c_str());
  
}

UdpClient::~UdpClient() {
  closesocket(socket_desc);
  WSACleanup();
}

int UdpClient::recv(char *msg, size_t max_size) {
  int server_addr_len = (int) sizeof(server_addr);
  return ::recvfrom(socket_desc, msg, (int) max_size, 0, (struct sockaddr*) &server_addr,
                    &server_addr_len);
}


int UdpClient::send(char* msg, size_t msg_size) {
  int n = ::sendto(socket_desc, msg, (int) msg_size, 0, (const sockaddr*)&server_addr, 
                  sizeof(server_addr));
  if (n == SOCKET_ERROR) {
    printf("ERROR: sendto error with %d\n", WSAGetLastError());
    throw UdpClientException("sendto failed");
  }
  return n;
}
