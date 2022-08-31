#include "win_speaker.h"
#include "dbg_utils.h"

#include <iostream>

using namespace audio;

void WinSpeaker::run(int ntimes) {
  int num_entries = fifo.size();
  int num_pkts = 0;
  int max_fifo_depth = 0;

 
  std::cout << "[VER=1.0] Starting with fifo size=" << num_entries << std::endl;

  while(1) {
   
    WinAudioBufferSP buf = fifo.get();

    if(timestamp_mode) {
      // Warning: C28159 for 64b count. But 32b count for timestamp is ok. Waived.
      DWORD pkt_tstamp = DbgUtils::get_timestamp((char*)buf->ptr()); 
      total_latency += (GetTickCount() - pkt_tstamp);

      if (num_pkts > 0 && (num_pkts % 50 == 0)) {
        printf("AverageLatency over %d pkts=%0.2f msec, FifoSize=%d\n", 
               num_pkts, average_latency(num_pkts), fifo.size());
      }

    }

    // Prepare the header for playback on sound card
    if(waveOutPrepareHeader(handle, &buf->wave_hdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR){
      std::cout << "Error preparing Header!" << std::endl;
      return;
    }

    // Play the sound!
    if(waveOutWrite(handle, &buf->wave_hdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
      std::cout << "Error writing to sound card!" << std::endl;
      return;
    }
    
    inuse_fifo.push_back(buf);

    if (inuse_fifo.size() > 16) {    // Arbitary
      // Try unprepare our wav header
      WinAudioBufferSP active_buf = inuse_fifo.front();
      MMRESULT rslt = waveOutUnprepareHeader(handle, &active_buf->wave_hdr, sizeof(WAVEHDR));
      
      if (rslt == MMSYSERR_NOERROR) {
        inuse_fifo.pop_front();
      }
    }

    num_pkts += 1;
    if (fifo.size() > max_fifo_depth) {
     max_fifo_depth = fifo.size();
    }

    if(num_pkts % 50 == 0) {
      std::cout << "NumPkts=" << num_pkts << ", MaxFifoDepth=" << max_fifo_depth
                << ", CurrentFifoDepth=" << fifo.size() 
                << ", InuseFifoCnt=" << inuse_fifo.size() << std::endl;
      max_fifo_depth = 0;
    }

    if (ntimes > 0) {
      if (num_pkts >= ntimes) break;
    }
  }

  std::cout << "Enqued " << num_pkts << " packets to the speaker." << std::endl;
  int num_entries_left = (int) inuse_fifo.size();
  std::cout << "Flushing " << inuse_fifo.size() << " entries from the active fifo" << std::endl;

  WinAudioBufferSP inuse_buf;
  for (int i = 0; i < num_entries_left; i++) {
    inuse_buf = inuse_fifo.front();
    while(waveOutUnprepareHeader(handle, &inuse_buf->wave_hdr, sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
      Sleep(100);
    }
    inuse_fifo.pop_front();
  }

  std::cout << "Finished freeing all the buffers. InUseSize=" << inuse_fifo.size() << std::endl;

}

WinSpeaker::~WinSpeaker() {
  waveOutClose(handle);
}
