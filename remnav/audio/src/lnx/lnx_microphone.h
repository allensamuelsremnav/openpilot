#ifndef AUDIO_LNX_MICROPHONE_H_
#define AUDIO_LNX_MICROPHONE_H_

#include "lnx_base_device.h"
#include "fifo_put_intf.h"
#include "raw_buffer.h"

namespace audio {

class LnxMicrophone : public LnxBaseDevice {
public:
  LnxMicrophone(PcmConfig cfg, FifoPutIntf<RawBufferSP>& fifo) : 
    LnxBaseDevice(cfg, 0),
    fifo(fifo) {}


  void run(int* done);

  void set_timestamp_mode(int tstamp_mode) {
    this->timestamp_mode = tstamp_mode;
  }


private:
  audio::FifoPutIntf<RawBufferSP>& fifo;
  unsigned int period_time;

  int timestamp_mode;   
};

}

#endif