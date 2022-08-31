#ifndef AUDIO_LNX_BASE_DEVICE_H_
#define AUDIO_LNX_BASE_DEVICE_H_

#include "pcm_config.h"
#include <alsa/asoundlib.h>

namespace audio {

class LnxBaseDevice {
public:
  LnxBaseDevice(audio::PcmConfig cfg, int dir);
  int get_alsa_period_size() { return alsa_period_size; } // For testing purposes
  virtual ~LnxBaseDevice();

protected:
  int dir;
  snd_pcm_t* handle;
  snd_pcm_hw_params_t *hw_params;

  // ALSA terminology
  // A frame is the size of 1 sample. It is independent of sampling freq
  // A period is the number of frames between each hardware interrupt.
      
  // For 8b sample and mono audio it would be 1 * 1 = 1 byte
  int alsa_frame_size;
  snd_pcm_uframes_t alsa_period_size;
  unsigned int alsa_period_time;
};

}

#endif