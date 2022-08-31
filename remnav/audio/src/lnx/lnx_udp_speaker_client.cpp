//
// Speaker Client
// This is a UDP server that listens for traffic from the remote microphone.

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <thread>
#include <stdlib.h>

#include "pcm_config.h"
#include "ts_fifo.h"
#include "arg_parser.h"

#include "raw_buffer.h"
#include "lnx_speaker.h"
#include "lnx_udp_server.h"

using namespace audio;

// Windows did not have getopt and so I created my own. Reusing it for lnx.
void print_usage(const char* msg=NULL) {
  fprintf(stderr, "This program reads data from udp and sends it out to default speaker.\n\n");
  fprintf(stderr, "Usage: program -p <udp_port> [-t] [-d delay_size]\n");
  fprintf(stderr, "  -p [port] is the udp port to connect to.\n");
  fprintf(stderr, "  -t enables the timestamp debug mode.\n");
  fprintf(stderr, "  -d number of entries to delay the reader from writer.\n");

  if (msg) {
    fprintf(stderr, "\n\n");
    fprintf(stderr, "ERROR: %s", msg);
    fprintf(stderr, "\n\n");
  }
}

int main(int argc, const char* argv[]) {

  ArgParser parser(argc, argv, &print_usage);
  u_short udp_port = (u_short)parser.getIntOption("-p", 1);
  int timestamp_mode = parser.isBoolOption("-t");
  int delay_num_entries = parser.getIntOption("-d", 0);
  if(delay_num_entries == 0) delay_num_entries = 4;

  UdpServer udp_server(udp_port);
  
  PcmConfig cfg;
  TsFifo<RawBufferSP> fifo;
  LnxSpeaker speaker(cfg, fifo);
  speaker.set_reader_delay_start(delay_num_entries);
  std::thread thread(&LnxSpeaker::run, &speaker, 0);

  int pkt_cnt = 0;
  int nbytes;

  while(1) {
    
    // UDP protocol requires that we know up front the max size of the rx udp datagram.
    // If the incoming packet exceeds this size then the extra bytes will be dropped.
    RawBufferSP bufsp = std::make_shared<RawBuffer>(8192);
    nbytes = udp_server.recv((char*) bufsp->ptr(), bufsp->size());
    bufsp->resize(nbytes);
    
    fifo.put(bufsp);
    
    pkt_cnt += 1;
    if(pkt_cnt % 50 == 0) {
      printf(">>Rcvd %d UDP packets from microhone\n", pkt_cnt);
    }
   
  }

  return 0;
}
