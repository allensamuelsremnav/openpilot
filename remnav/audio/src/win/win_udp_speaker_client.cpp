//
// Speaker Client
// This is a UDP server that listens to traffic from the remote microphone (UDP client)

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <thread>
#include <stdlib.h>
#include <winsock2.h>

#include "pcm_config.h"
#include "ts_fifo.h"
#include "arg_parser.h"

#include "win_audio_buffer.h"
#include "win_speaker.h"
#include "win_udp_server.h"

using namespace audio;

void print_usage(const char* msg = NULL) {
  fprintf(stderr, "This program receives data from udp port and plays it on the default speaker.\n\n");
  fprintf(stderr, "Usage: program -p <udp_port> [-t] [-b max_buf_size]\n");
  fprintf(stderr, "  -p [port] is the udp port to connect to.\n");
  fprintf(stderr, "  -t enables the timestamp debug mode.\n");
  fprintf(stderr, "  -b max size of the audio buffer to use.\n");

  if (msg) {
    fprintf(stderr, "\n\n");
    fprintf(stderr, msg);
    fprintf(stderr, "\n\n");
  }
}

int main(int argc, const char* argv[]) {
  WSADATA wsaData;

  // Initialize Winsock version 2.2
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    printf("Server: WSAStartup failed with error: %ld\n", WSAGetLastError());
    return -1;
  }
  
  ArgParser parser(argc, argv, &print_usage);
  u_short udp_port = (u_short)parser.getIntOption("-p", 1);
  int timestamp_mode = parser.isBoolOption("-t");
  int max_buf_size = parser.getIntOption("-b", 0);
  if (max_buf_size == 0) {
    // If the size of the UDP message exceeds this, then the rest of the packet will be lost !
    // Hence must make sure that the microphone client does not exceed this size.
    max_buf_size = 8192;
  }

  UdpServer udp_server(udp_port);

  PcmConfig cfg;
  TsFifo<WinAudioBufferSP> fifo;
  WinSpeaker speaker(cfg, fifo);
  speaker.set_timestamp_mode(timestamp_mode);

  std::thread thread(&WinSpeaker::run, &speaker, 0); // Run forever

  int pkt_cnt = 0;
  int nbytes;

  while(1) {
    
    // UDP protocol requires that we know up front the max size of the rx udp datagram.
    // If the incoming packet exceeds this size then the extra bytes will be dropped.
    WinAudioBufferSP bufsp = std::make_shared<WinAudioBuffer>(8192);
    nbytes = udp_server.recv((char*) bufsp->ptr(), bufsp->size());
    
    bufsp->resize(nbytes);

    fifo.put(bufsp);
    pkt_cnt += 1;
  }

  

  return 0;
}
