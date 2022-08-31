#include <iostream>
#include <thread>
#include <stdlib.h>

#include "pcm_config.h"
#include "ts_fifo.h"
#include "raw_buffer.h"
#include "win_speaker.h"

using namespace std;
using namespace audio;

int main(int argc, char** argv) {
  char* fname;
  
  if(argc != 2) {
    fprintf(stderr, "Invalid Usage: %s in_filename\n", argv[0]);
    exit(-1);
  }

  fname = argv[1];
  
  FILE* fptr;
  errno_t err;

  err = fopen_s(&fptr, fname, "rb");
  if (err) {
    char buf[256];
    strerror_s(buf, 256, errno);
    fprintf(stderr, "Could not open '%s':%s\n", fname, buf);
    exit(-1);
  }

  // Chunkify the file into period sized buffers. Ignore last few bytes.
  fseek(fptr, 0L, SEEK_END);
  int file_size = ftell(fptr);
  rewind(fptr);

  PcmConfig cfg;
  TsFifo<WinAudioBufferSP> fifo;
  WinSpeaker speaker(cfg, fifo);

  int num_periods = file_size / 1024;

  for(int i = 0; i < num_periods; i++) {
    WinAudioBufferSP bufsp = std::make_shared<WinAudioBuffer>(1024);
    fread(bufsp->ptr(), bufsp->size(), 1, fptr);
    fifo.put(bufsp);
  }

  std::thread thread(&WinSpeaker::run, &speaker, num_periods);

  thread.join();

  fclose(fptr);

  return 0;
}