#ifndef AUDIO_RAW_BUFFER_H_
#define AUDIO_RAW_BUFFER_H_

#include <stdexcept>
#include <memory>

namespace audio {

class RawBuffer {
public:
  RawBuffer(int len) {
    buf = malloc(len);
    this->len = len;
  }

  void* ptr() {
    return this->buf;
  }

  int size() {
    return len;
  }

  void resize(int n) {
    if(n <= len) {
      this->len = n;
    }
    else {
      char* tmp = (char*) realloc(buf, n);
      if (tmp != NULL) {
        buf = tmp;
      }
      else {
        throw std::runtime_error("could not allocate memory");
      }
    } 
  }

  virtual ~RawBuffer() {
    free(buf);
    len = 0;
  }

protected:
  int len;
  void* buf;
};

typedef std::shared_ptr<RawBuffer> RawBufferSP;

}
#endif