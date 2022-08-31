#include "win_microphone.h"
#include "dbg_utils.h"

#include <stdio.h>

using namespace std;
using namespace audio;

void WinMicrophone::run(int *done) {
   
  // Double buffering
  WAVEHDR wave_hdrs[2]; // WAVE header for our sound data
 
  for(int i = 0; i < 2; i++) {
    memset(wave_hdrs + i, 0, sizeof(wave_hdrs[i]));
    wave_hdrs[i].lpData = (LPSTR) malloc(audio_buf_size);
    wave_hdrs[i].dwBufferLength = audio_buf_size;
   
    if (waveInPrepareHeader(handle, &wave_hdrs[i], sizeof(wave_hdrs[i])) != MMSYSERR_NOERROR) {
        printf("Init:unable to prepare header\n");
        return;
    }

    if (waveInAddBuffer(handle, &wave_hdrs[i], sizeof(wave_hdrs[i])) != MMSYSERR_NOERROR) {
        printf("Init:unable to add header\n");
        return;
    }
  }

  printf(">> Starting to record: Audio buffer size=%d bytes\n", audio_buf_size);

  // start recording
  if (waveInStart(handle) != MMSYSERR_NOERROR) {
      printf("Failed to start recording.\n");
      return;
  }

  while(*done != 1) {
    //
    // Ping Pong Scheme
    // Poll the done status of 2 buffers.
    // As soon as buffer is done (ie it has data), send it to fifo and re-add it.
    //

    for(auto& h : wave_hdrs) {
      if(h.dwFlags & WHDR_DONE) {  // is this header done ?
        RawBufferSP buf_sp = std::make_shared<RawBuffer>(h.dwBufferLength);

        if (h.lpData == NULL) {
          throw std::runtime_error("Microphone buffer has no data!!!");
        }
        else {
          memcpy(buf_sp->ptr(), h.lpData, h.dwBufferLength);
        }

        if(timestamp_mode) {
          DbgUtils::insert_timestamp((char*)buf_sp->ptr());
        }

        fifo.put(buf_sp);

        h.dwFlags = 0;
        h.dwBytesRecorded = 0;

        // re add it
        waveInPrepareHeader(handle, &h, sizeof(h));
        waveInAddBuffer(handle, &h, sizeof(h));
      }
    }
  }
  printf(">> Stopping recording. Done=%d\n", *done);

  waveInStop(handle);
  for(auto& h : wave_hdrs) {
    free(h.lpData);  // Warning C6001 for unitialized memory.
                     // But we are just freeing memory even if uninitialized.
    waveInUnprepareHeader(handle, &h, sizeof(h));
  }

}


WinMicrophone::~WinMicrophone() {
    waveInClose(handle);
}
