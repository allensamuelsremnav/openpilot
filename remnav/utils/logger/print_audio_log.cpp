#include<sys/types.h>
#include<sys/stat.h>
#include <fcntl.h>  

#ifndef WIN32
  #include <fcntl.h>
  #include <unistd.h>
#endif

#include "log.capnp.h"
#include <capnp/message.h>
#include <capnp/serialize.h>
#include <iostream>

//
// Uses capnproto to print the audio message.
//

int main(int argc, char** argv) {

  if (argc != 2) {
    fprintf(stderr, "Wrong number of arguments specified. %d\n", argc);
    return -1;
  }

  char* logfile = argv[1];
#ifdef WIN32
  int fd = _open(logfile, O_RDONLY);
#else
  int fd = open(logfile, O_RDONLY);
#endif
  ::capnp::StreamFdMessageReader message(fd);


  Message::Reader msg = message.getRoot<Message>();
  std::cout << "Valid=" << msg.getValid() << std::endl;
  std::cout << "Tstamp=" << msg.getLogMonoTime() << std::endl;
  if (msg.isAudioPcmConfig()) {
    std::cout << "SampleSize=" << msg.getAudioPcmConfig().getSampleSizeBits() << std::endl;
    std::cout << "NumChannels=" << msg.getAudioPcmConfig().getNumChannels() << std::endl;
  }

#ifdef WIN32
  _close(fd);
#else
  close(fd);
#endif

  return 0;
}
