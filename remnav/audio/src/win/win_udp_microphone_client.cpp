//
// Microphone Client
//

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <thread>
#include <stdlib.h>
#include <winsock2.h>

#include "pcm_config.h"
#include "ts_fifo.h"
#include "arg_parser.h"

#include "raw_buffer.h"
#include "win_microphone.h"
#include "win_udp_client.h"
#include "audio_logger.h"

#include "log.capnp.h"
#include <capnp/message.h>
#include <capnp/serialize.h>

using namespace audio;

void print_usage(const char* msg=NULL) {
  fprintf(stderr, "This program reads data from microphone and sends it out on udp port.\n\n");
  fprintf(stderr, "Usage: program -s <server_ip_dot> -p <udp_port> [-t] [-b buffer_size]\n");
  fprintf(stderr, "  -s [ip_addr] is the destination udp address.\n");
  fprintf(stderr, "  -p [port] is the udp port to connect to.\n");
  fprintf(stderr, "  -t enables the timestamp debug mode.\n");
  fprintf(stderr, "  -b size of the audio buffer to use.\n");

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
  std::string tmp = parser.getStringOption("-s", 1).c_str();
  const char* server_ip = tmp.c_str();
  u_short udp_port = (u_short)parser.getIntOption("-p", 1);
  int audio_buf_size = parser.getIntOption("-b", 0);
  int timestamp_mode = parser.isBoolOption("-t");

  PcmConfig cfg;
  AudioLogger::log_pcm_config(cfg);
  
  UdpClient udp_client(server_ip, udp_port);
  AudioLogger::log_server_config(server_ip, udp_port, audio_buf_size);

  TsFifo<RawBufferSP> fifo;
  WinMicrophone microphone(cfg, fifo);
  microphone.set_timestamp_mode(timestamp_mode);
  if (audio_buf_size > 0) {
    microphone.set_audio_buf_size(audio_buf_size);
  }
  else {
    microphone.set_audio_buf_size(1024);  // Default value
  }

  int done = 0;
  std::thread thread(&WinMicrophone::run, &microphone, &done);

  int num_pkts = 0;

  RawBufferSP item;
  while (1) {
    item = fifo.get();
    udp_client.send((char*) item->ptr(), item->size());
    num_pkts += 1;
  }

  printf(">>Stopping microphone thread. Waiting for thread to be done\n");
  done = 1;
  thread.join();

  printf(">>Exiting microphone client with %d udp packets sent\n", num_pkts);

  return 0;
}
