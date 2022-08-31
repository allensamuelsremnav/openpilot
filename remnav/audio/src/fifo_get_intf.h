#ifndef AUDIO_FIFO_GET_INTF_H_
#define AUDIO_FIFO_GET_INTF_H_

namespace audio {

template <class T>
class FifoGetIntf {
  public:
    virtual T get() = 0;
    virtual int size() = 0;
};

}

#endif