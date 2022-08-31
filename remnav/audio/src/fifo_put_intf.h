#ifndef AUDIO_FIFO_PUT_INTF_H_
#define AUDIO_FIFO_PUT_INTF_H_

namespace audio {

template <class T>
class FifoPutIntf {
  public:
    virtual void put(T t) = 0;
};

}

#endif