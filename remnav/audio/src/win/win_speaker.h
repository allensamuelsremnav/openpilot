#ifndef AUDIO_WIN_SPEAKER_H
#define AUDIO_WIN_SPEAKER_H

#include <windows.h>
#include "fifo_get_intf.h"
#include "win_audio_buffer.h"
#include "win_utils.h"
#include <stdexcept>
#include <deque>

namespace audio {

class WinSpeaker {
public:
  WinSpeaker(PcmConfig cfg, FifoGetIntf<WinAudioBufferSP>& fifo) :
    fifo(fifo) {
    wave_format = Utils::sound_format(cfg);
    timestamp_mode = 0;
    total_latency = 0;
    
    // Open the audio device
    if (waveOutOpen(&handle, WAVE_MAPPER, &wave_format, 0, 0, 
                      CALLBACK_NULL) != MMSYSERR_NOERROR) {
      throw std::runtime_error("Could not open the speaker");
    }
  }

  void run(int ntimes=0);  // 0 means run forever

  void set_timestamp_mode(int tstamp_mode) {
    this->timestamp_mode = tstamp_mode;
  }

  float average_latency(int num_pkts) {
    if (num_pkts == 0) return 0.0;
    float lat = (float) total_latency / num_pkts;
    return lat;
  }

  virtual ~WinSpeaker();

private:
  audio::FifoGetIntf<WinAudioBufferSP>& fifo;
  HWAVEOUT handle; // Handle to sound card output
  WAVEFORMATEX wave_format; // The sound format
  std::deque<WinAudioBufferSP> inuse_fifo;

  int timestamp_mode;
  DWORD total_latency;
};

}

#endif