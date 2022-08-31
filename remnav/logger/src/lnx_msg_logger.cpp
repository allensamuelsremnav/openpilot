#include <stdexcept>
#include <cassert>
#include <mutex>
#include <iostream>
#include <stdio.h>
#include <memory>

#include "msg_logger.h"
#include <capnp/serialize.h>

// Log file basic api
class LogFile {
public:
  LogFile(const char* fname) {
    fp = fopen(fname, "wb");
  }

  ~LogFile() {
    fclose(fp);
  }

  inline void write_bytes(void* data, int size) {
    size_t num_bytes = fwrite(data, 1, size, fp);
    assert(num_bytes == size); 
    fflush(fp);
  }

private:
	FILE* fp = nullptr;

};

// Make logging thread safe
typedef struct LogHandle {
  std::mutex mutex;
  std::unique_ptr<LogFile> log;
} LogHandle;

static LogHandle* log_handle;

MsgLogger::MsgLogger(const char* fname) {
  log_handle = new LogHandle;
  log_handle->log = std::make_unique<LogFile>(fname);
}

void MsgLogger::raw_bytes(uint8_t* data, size_t data_size) {
  log_handle->mutex.lock();
  log_handle->log->write_bytes(data, data_size);
  log_handle->mutex.unlock();
}

void MsgLogger::msg(::capnp::MallocMessageBuilder& msg) {
  const auto m = capnp::messageToFlatArray(msg);
  const auto c = m.asChars();
  raw_bytes((uint8_t*)c.begin(), c.size());
}

MsgLogger::~MsgLogger() {
  delete log_handle;  
}