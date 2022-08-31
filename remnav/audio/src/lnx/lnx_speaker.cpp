#include "lnx_speaker.h"
#include <unistd.h>
#include <iostream>

using namespace std;
using namespace audio;

void LnxSpeaker::run(int ntimes) {
  int rc;

  printf("Running speaker: %s=%d bytes, %s=%d usec, %s=%ld bytes\n",
          "frame_size", alsa_frame_size, "period_time", alsa_period_time, 
           "period_size", alsa_period_size);

  int num_entries = fifo.size();
  if(ntimes == 0 and num_entries > 0) {
    printf("Speaker flushing %d initial entries in fifo\n", num_entries);
    for(int i = 0; i < num_entries; i++) fifo.get();
  }

  int num_pkts =0;
  int pcm_frames;
  int init_buf_inserted = 0;

  while(1) {

    if (fifo.size() > (this->delay_start + 8)) {
      std::cout << "Warning: High latency. Depth=" << fifo.size()
                << " entries." << std::endl;
    }
    
    if(init_buf_inserted == 0) {
      // Allow the reader to get a head start as compared to the writer. This prevents
      // underflow.
      while(fifo.size() < this->delay_start) {
        sleep(1);
      }

      std::cout << "Restarting with fifo size=" << fifo.size() << std::endl;
      init_buf_inserted = 1;
    }

    RawBufferSP buf = fifo.get();

    pcm_frames = buf->size() / alsa_frame_size;
    rc = snd_pcm_writei(handle, buf->ptr(), pcm_frames);

    if(rc == -EPIPE) {
      fprintf(stderr, "Underrun occured\n");
      snd_pcm_recover(handle, rc, 1);
      snd_pcm_wait(handle, -1);
      fprintf(stdout, "Device is re-ready with handle=%s, state=%s\n", 
          snd_pcm_name(handle), snd_pcm_state_name(snd_pcm_state(handle)));
      init_buf_inserted = 0;
    }
    else if(rc < 0) {
      fprintf(stderr, "Read error from speaker.%s\n",
              snd_strerror(rc));
    }
    else if(rc != pcm_frames) {
      fprintf(stderr, "Short write %d frames\n", rc);
    }

    num_pkts += 1;
    if (ntimes > 0) {
      if (num_pkts >= ntimes) break;
    }
  }

  std::cout << "Finished playing " << num_pkts << " packets on the speaker. Exit." << std::endl;
}