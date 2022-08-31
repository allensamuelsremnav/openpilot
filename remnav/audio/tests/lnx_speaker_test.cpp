#include <iostream>
#include <thread>
#include <unistd.h>
#include <stdlib.h>

#include "pcm_config.h"
#include "ts_fifo.h"
#include "lnx_speaker.h"
#include "raw_buffer.h"

using namespace std;
using namespace audio;

int main(int argc, char** argv) {
  char* fname;
  int time_sec;
  char* end_ptr;

  if(argc != 2) {
    fprintf(stderr, "Invalid Usage: %s in_filename\n", argv[0]);
    exit(-1);
  }

  fname = argv[1];
  

  FILE* fptr = fopen(fname, "rb");
  if(!fptr) {
    fprintf(stderr, "Could not open '%s':%s\n", fname, strerror(errno));
    exit(-1);
  }

  // Chunkify the file into period sized buffers. Ignore last few bytes.
  fseek(fptr, 0L, SEEK_END);
  int file_size = ftell(fptr);
  rewind(fptr);

  PcmConfig cfg;
  TsFifo<RawBufferSP> fifo;
  LnxSpeaker speaker(cfg, fifo);

  int num_periods = file_size / speaker.get_alsa_period_size();

  for(int i = 0; i < num_periods; i++) {
    RawBufferSP bufsp = std::make_shared<RawBuffer>(speaker.get_alsa_period_size());
    fread(bufsp->ptr(), bufsp->size(), 1, fptr);
    fifo.put(bufsp);
  }

  
  std::thread thread(&LnxSpeaker::run, &speaker, num_periods);

  thread.join();

  fclose(fptr);

  return 0;
}