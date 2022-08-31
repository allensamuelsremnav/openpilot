#include <windows.h>
#include <stdexcept>
#include <cassert>
#include <mutex>
#include <iostream>
#include <fileapi.h>


#include "msg_logger.h"
#include <capnp/serialize.h>

// Log file basic api
class LogFile {
public:
  LogFile(const char* fname) {
     handle = CreateFileA(fname, 
						   GENERIC_WRITE, 
		                   0, 
		                   NULL, 
		                   CREATE_ALWAYS,
		                   FILE_ATTRIBUTE_NORMAL, 
		                   NULL);

	if (handle == INVALID_HANDLE_VALUE) {
		throw std::invalid_argument("Could not open log file.");
	}

  }

  ~LogFile() {
      CloseHandle(handle);
  }

  inline void write_bytes(void* data, int size) {
  	DWORD bytes_written;
    bool status = WriteFile(handle, data, size, &bytes_written, NULL);
	

	if (FALSE == status) {
		throw std::invalid_argument("Could not write to log file.");
	} 
	else {
		if(bytes_written != size) {
			throw std::underflow_error("Could not write all the bytes to the log file.");
		}
	}
  }

private:
	HANDLE handle = INVALID_HANDLE_VALUE;

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