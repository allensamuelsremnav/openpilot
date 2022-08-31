#ifndef AUDIO_WIN_UTILS_H_
#define AUDIO_WIN_UTILS_H_

#include "pcm_config.h"

namespace audio {

class Utils {
public:
  static WAVEFORMATEX sound_format(PcmConfig cfg) {
    WAVEFORMATEX wave_format = {}; // The sound format
    wave_format.wFormatTag = WAVE_FORMAT_PCM;     // Uncompressed sound format
    wave_format.nChannels = (WORD) cfg.num_channels;     // 1 = Mono, 2 = Stereo
    wave_format.wBitsPerSample = (WORD) cfg.sample_size_bits;
    wave_format.nSamplesPerSec = cfg.sampling_rate_hz;  
    wave_format.nBlockAlign = wave_format.nChannels * wave_format.wBitsPerSample / 8;
    wave_format.nAvgBytesPerSec = wave_format.nSamplesPerSec * wave_format.nBlockAlign;
    wave_format.cbSize = 0;
    return wave_format;
  }
  
};


}

#endif