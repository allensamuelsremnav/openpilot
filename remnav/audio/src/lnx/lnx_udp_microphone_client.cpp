//
// Microphone Client
//

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <thread>
#include <stdlib.h>

#include "pcm_config.h"
#include "ts_fifo.h"
#include "arg_parser.h"

#include "raw_buffer.h"
#include "lnx_microphone.h"
#include "lnx_udp_client.h"

#include "audio_logger.h"

using namespace audio;

// Windows did not have getopt and so I created my own. Reusing it for lnx.
void print_usage(const char* msg=NULL) {
  fprintf(stderr, "This program reads data from microphone and sends it out on udp port.\n\n");
  fprintf(stderr, "Usage: program -s <server_ip_dot> -p <udp_port> [-t] [-b buffer_size]\n");
  fprintf(stderr, "  -s [ip_addr] is the destination udp address.\n");
  fprintf(stderr, "  -p [port] is the udp port to connect to.\n");
  fprintf(stderr, "  -t enables the timestamp debug mode.\n");
  fprintf(stderr, "  -b size of the audio buffer to use.\n");

  if (msg) {
    fprintf(stderr, "\n\n");
    fprintf(stderr, "ERROR: %s", msg);
    fprintf(stderr, "\n\n");
  }
}

int main(int argc, const char* argv[]) {

  ArgParser parser(argc, argv, &print_usage);
  std::string tmp = parser.getStringOption("-s", 1).c_str();
  const char* server_ip = tmp.c_str();
  u_short udp_port = (u_short)parser.getIntOption("-p", 1);
  int audio_buf_size = parser.getIntOption("-b", 0);
  int timestamp_mode = parser.isBoolOption("-t");

  PcmConfig cfg;
  AudioLogger::log_pcm_config(cfg);

  if(audio_buf_size > 0) {
    cfg.input_buf_size = audio_buf_size;
  }
  
  TsFifo<RawBufferSP> fifo;
  LnxMicrophone microphone(cfg, fifo);
  UdpClient udp_client(server_ip, udp_port);
  AudioLogger::log_server_config(server_ip, udp_port, audio_buf_size);

  int done = 0;
  std::thread thread(&LnxMicrophone::run, &microphone, &done);

  int num_pkts = 0;

  RawBufferSP item;
  while (1) {
    item = fifo.get();
    udp_client.send((char*) item->ptr(), item->size());
    num_pkts += 1;
    if(num_pkts % 50 == 0) {
      printf(">>Sent %d UDP samples\n", num_pkts);
    }
  }

  printf(">>Stopping microphone thread. Waiting for thread to be done\n");
  done = 1;
  thread.join();

  printf(">>Exiting microphone client with %d udp packets sent\n", num_pkts);

  return 0;
}
