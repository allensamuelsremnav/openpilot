#ifndef AUDIO_LNX_SPEAKER_H_
#define AUDIO_LNX_SPEAKER_H_

#include "lnx_base_device.h"
#include "fifo_get_intf.h"
#include "raw_buffer.h"

namespace audio {

class LnxSpeaker : public LnxBaseDevice {
public:
  LnxSpeaker(PcmConfig cfg, FifoGetIntf<RawBufferSP>& fifo) : 
    LnxBaseDevice(cfg, 1),
    fifo(fifo) {}

  void set_reader_delay_start(int num_packets) {
    this->delay_start = num_packets;
  }

  void run(int ntimes=0);

private:
  audio::FifoGetIntf<RawBufferSP>& fifo;
  int delay_start;
};

}

#endif