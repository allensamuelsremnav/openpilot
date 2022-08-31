#ifndef AUDIO_WIN_MICROPHONE_H_
#define AUDIO_WIN_MICROPHONE_H_

#include <windows.h>
#include "fifo_put_intf.h"
#include "raw_buffer.h"
#include "win_utils.h"
#include <stdexcept>


namespace audio {

class WinMicrophone {
public:
  WinMicrophone(PcmConfig cfg, FifoPutIntf<RawBufferSP>& fifo) :
    fifo(fifo) {
    wave_format = Utils::sound_format(cfg);
    timestamp_mode = 0;
    // Open default microphone with no callback.
    if (waveInOpen(&handle, WAVE_MAPPER, &wave_format, NULL, NULL,
      CALLBACK_NULL | WAVE_FORMAT_DIRECT) != MMSYSERR_NOERROR) {

      throw std::runtime_error("Could not open the microphone");
    }
     
    // Using a 1/2 sec buffer size by default. Can be changed from cmd line
    audio_buf_size = (cfg.sampling_rate_hz * cfg.sample_size_bits / 8 
                    * cfg.num_channels) / 2;
  }

  void set_audio_buf_size(int size) {
    this->audio_buf_size = size;
  }

  void run(int* done);

  void set_timestamp_mode(int tstamp_mode) {
    this->timestamp_mode = tstamp_mode;
  }
  
  virtual ~WinMicrophone();
  
private:
  audio::FifoPutIntf<RawBufferSP>& fifo;
  HWAVEIN handle;   // handle to the microphone
  WAVEFORMATEX wave_format; // The sound format

  int audio_buf_size;
  int timestamp_mode;   
};

}
#endif
