#ifndef MY_AUDIO_DBG_UTILS_H_
#define MY_AUDIO_DBG_UTILS_H_

#include <string.h>

namespace audio {

class DbgUtils {
 public:
  static void insert_timestamp(char* buf) {
    DWORD tstamp = (DWORD) GetTickCount64();
    memcpy(buf, &tstamp, sizeof(DWORD));
  }

  static DWORD get_timestamp(char* buf) {
    DWORD tstamp = *(DWORD*)buf;
    return tstamp;
  }

};

}

#endif
