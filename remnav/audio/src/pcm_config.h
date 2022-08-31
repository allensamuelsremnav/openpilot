#ifndef AUDIO_PCM_CONFIG_H_
#define AUDIO_PCM_CONFIG_H_

namespace audio {

struct PcmConfig {
  int sample_size_bits;
  int sampling_rate_hz;
  int num_channels;
  int input_buf_size;  

  PcmConfig() {
    sample_size_bits = 8;
    sampling_rate_hz = 8000;
    num_channels = 1;
    input_buf_size = 1024;
  }
  
};

}
#endif
