#ifndef AUDIO_UDP_CLIENT_EXCEPTION_H_
#define AUDIO_UDP_CLIENT_EXCEPTION_H_

#include <stdexcept>

namespace audio {

class UdpClientException : public std::runtime_error {
public:
  UdpClientException(const char* w) : std::runtime_error(w) {}

};

}

#endif