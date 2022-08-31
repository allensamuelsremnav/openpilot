#include <iostream>
#include <thread>
#include <stdlib.h>

#include "pcm_config.h"
#include "ts_fifo.h"

#include "raw_buffer.h"
#include "win_microphone.h"

using namespace std;
using namespace audio;

int main(int argc, char** argv) {
  char* fname;
  int time_sec;
  char* end_ptr;
  int done = 0;

  if (argc != 3) {
      fprintf(stderr, "Invalid Usage: %s out_filename time_sec\n", argv[0]);
      exit(-1);
  }

  fname = argv[1];
  time_sec = strtol(argv[2], &end_ptr, 0);
  errno_t err;

  FILE* fptr;
  err = fopen_s(&fptr, fname, "wb");
  if (err) {
    char buf[256];
    strerror_s(buf, 256, errno);
    fprintf(stderr, "Could not open '%s':%s\n", fname, buf);
    exit(-1);
  }

  PcmConfig cfg;
  TsFifo<RawBufferSP> fifo;
  WinMicrophone microphone(cfg, fifo);

  std::thread thread(&WinMicrophone::run, &microphone, &done);

  Sleep(time_sec * 1000);

  done = 1;
  thread.join();

  int num_entries = fifo.size();
  printf("Recieved %d entries from the microphone\n", num_entries);

  RawBufferSP item;
  for (int i = 0; i < num_entries; i++) {
    item = fifo.get();
    fwrite(item->ptr(), 1, item->size(), fptr);
  }

  fclose(fptr);

  return 0;
}