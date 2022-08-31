#include "lnx_microphone.h"

#include <unistd.h>
#include <iostream>

using namespace std;
using namespace audio;

void LnxMicrophone::run(int* done) {
  int rc;

  int num_frames = alsa_period_size / alsa_frame_size;
  printf(">>Running microphone: %s=%d byte, %s=%d usec, %s=%ld frames\n",
          "frame_size(Analog sample size)", alsa_frame_size,
          "period_time", alsa_period_time,
          "period_size", alsa_period_size);

  while(*done != 1) {
    
    RawBufferSP buf = std::make_shared<RawBuffer>(alsa_period_size);

    rc = snd_pcm_readi(handle, buf->ptr(), num_frames);
    if(rc == -EPIPE) {
      fprintf(stderr, "Overrun occured\n");
      snd_pcm_prepare(handle);
    }
    else if(rc < 0) {
      fprintf(stderr, "Read error from microphone.%s\n",
              snd_strerror(rc));
    }
    else if(rc != num_frames) {
      fprintf(stderr, "Short read %d frames\n", rc);
    }

    buf->resize(rc * alsa_frame_size);
    fifo.put(buf);
  }


}