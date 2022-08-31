#ifndef AUDIO_UDP_SERVER_EXCEPTION_H_
#define AUDIO_UDP_SERVER_EXCEPTION_H_

#include <stdexcept>

namespace audio {

class UdpServerException : public std::runtime_error {
public:
  UdpServerException(const char* w) : std::runtime_error(w) {}

};

}

#endif