#include "audio_logger.h"
using namespace audio;

#include <chrono>
using namespace std::chrono;

#include <capnp/message.h>
#include <capnp/serialize.h>
#include "log.capnp.h"
#include "msg_logger.h"

void AudioLogger::log_pcm_config(PcmConfig& cfg) {
  ::capnp::MallocMessageBuilder message;
  MsgLogger& log = MsgLogger::get_instance();

  Message::Builder msg = message.initRoot<Message>();

  auto millis = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
  msg.setLogMonoTime(millis);
  msg.setValid(1);

  AudioPcmConfig::Builder pcm_cfg = msg.initAudioPcmConfig();
  pcm_cfg.setSampleSizeBits(cfg.sample_size_bits);
  pcm_cfg.setSamplingRateHz(cfg.sampling_rate_hz);
  pcm_cfg.setBufSize(cfg.input_buf_size);
  pcm_cfg.setNumChannels(cfg.num_channels);
  
  log.msg(message);
}

void AudioLogger::log_server_config(const char* server_ip, int udp_port, int buf_size) {
  ::capnp::MallocMessageBuilder message;
  MsgLogger& log = MsgLogger::get_instance();

  Message::Builder msg = message.initRoot<Message>();
  auto millis = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
  msg.setLogMonoTime(millis);
  msg.setValid(1);
  
  AudioSocketConfig::Builder sock_cfg = msg.initAudioSocketConfig();
  sock_cfg.setServerIP(server_ip);
  sock_cfg.setUdpPort(udp_port);
  sock_cfg.setAudioBufSize(buf_size);
  sock_cfg.setSocketStatus(AudioSocketConfig::SocketStatus::INIT);
 
  log.msg(message); 
}
