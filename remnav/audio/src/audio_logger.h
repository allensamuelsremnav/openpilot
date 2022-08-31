#ifndef INCLUDED_AUDIO_LOGGER_H
#define INCLUDED_AUDIO_LOGGER_H

#include <sys/types.h>
#include "pcm_config.h"

namespace audio {

class AudioLogger {
public:
  static void log_pcm_config(PcmConfig& cfg);
  static void log_server_config(const char* server_ip, int udp_port, int buf_size);
};

}
#endif
