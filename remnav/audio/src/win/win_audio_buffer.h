#ifndef AUDIO_WIN_AUDIO_BUFFER_H_
#define AUDIO_WIN_AUDIO_BUFFER_H_

#include "raw_buffer.h"
#include <windows.h>

namespace audio {

class WinAudioBuffer : public RawBuffer {
 public:
  WinAudioBuffer(int len) : RawBuffer(len) {
    wave_hdr.lpData = (LPSTR) this->buf;
    wave_hdr.dwBufferLength = len;
    wave_hdr.dwFlags = 0;
    wave_hdr.dwLoops = 0;
  }

  virtual void resize(int n) {
    RawBuffer::resize(n);
    wave_hdr.lpData = (LPSTR) this->buf;
    wave_hdr.dwBufferLength = len;
  }

  WAVEHDR wave_hdr;
};

typedef std::shared_ptr<WinAudioBuffer> WinAudioBufferSP;
}

#endif