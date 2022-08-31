#include "win_udp_server.h"
#include <ws2tcpip.h>
#include <string>

using namespace audio;

UdpServer::UdpServer(u_short port) {
  socket_desc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  memset(&client_addr, 0, sizeof(client_addr));

  if (socket_desc == INVALID_SOCKET){
      //Print error message
      printf("Server: Error at socket(): %ld\n", WSAGetLastError());
      // Clean up
      WSACleanup();
      // Exit with error
      throw UdpServerException("socket failed");
  }
 
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(socket_desc, (SOCKADDR *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
    // Print error message
    printf("Server: Error! bind() failed!\n");

    // Close the socket
    closesocket(socket_desc);

    // Do the clean up
    WSACleanup();
    throw UdpServerException("bind failed");
  }
}

UdpServer::~UdpServer() {
  if (closesocket(socket_desc) != 0) {
        printf("Server: closesocket() failed! Error code: %ld\n", WSAGetLastError());
  }
   else{
        printf("Server: closesocket() is OK\n");
  }
 if(WSACleanup() != 0){
    printf("Server: WSACleanup() failed! Error code: %ld\n", WSAGetLastError());
  }
  else{
    printf("Server: WSACleanup() is OK\n");
  }
}

int UdpServer::recv(char *msg, int max_size) {
  int client_addr_size = sizeof(client_addr);
  int nbytes = recvfrom(socket_desc, msg, max_size, 0, (SOCKADDR *)&client_addr, 
                        &client_addr_size);

  if (nbytes <= 0){ //If the buffer is empty
    //Print error message
    fprintf(stdout, "Server: Connection closed with error code: %ld\n", WSAGetLastError());
  }
  
  return nbytes;
}

int UdpServer::send(char* msg, int msg_size) {
  return ::sendto(socket_desc, msg, msg_size, 0, 
                  (const sockaddr*)&client_addr, sizeof(client_addr));
}
