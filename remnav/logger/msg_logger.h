#pragma once

#include <cstdint>
#include <cstddef>
#include <capnp/message.h>


class MsgLogger {
public:
  static MsgLogger& get_instance() {
    static MsgLogger instance("msg_log.bin");
    return instance;
  }

  // Write capnp message
  void msg(::capnp::MallocMessageBuilder& msg);

  // Write raw bytes
  void raw_bytes(uint8_t* data, size_t data_size);

  ~MsgLogger();

private:
  MsgLogger(const char* fname);
  MsgLogger(MsgLogger const&);      // Dont implement
  void operator=(MsgLogger const&); // Done implement
};