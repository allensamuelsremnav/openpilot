#include "lnx_base_device.h"
#include <alsa/asoundlib.h>

using namespace audio;

LnxBaseDevice::LnxBaseDevice(PcmConfig cfg, int dir) {
  int err;

  this->dir = dir;

  if(dir) {
       if((err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0 ) {
          fprintf(stderr, "Cannot open speaker. (%s)\n", snd_strerror(err));
          exit(1);
      }

  }
  else {
      if((err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_CAPTURE, 0)) < 0 ) {
          fprintf(stderr, "Cannot open microphone. (%s)\n", snd_strerror(err));
          exit(1);
      }
  }
  

  // ALlocate structure
  if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
      fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n",
              snd_strerror (err));
      exit (1);
  }
               
  if ((err = snd_pcm_hw_params_any (handle, hw_params)) < 0) {
      fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n",
           snd_strerror (err));
      exit (1);
  }
  
  if(cfg.sample_size_bits == 8) {
      if ((err = snd_pcm_hw_params_set_format (handle, hw_params, SND_PCM_FORMAT_U8)) < 0) {
          fprintf (stderr, "cannot set sample format (%s)\n",
              snd_strerror (err));
          exit (1);
      }
  }
  else {
      fprintf (stderr, "Invalid sampel size %0d bits", cfg.sample_size_bits);
      exit(1);
  }

  
  if ((err = snd_pcm_hw_params_set_rate (handle, hw_params, cfg.sampling_rate_hz, 0)) < 0) {
      fprintf (stderr, "cannot set sample rate (%s)\n",
           snd_strerror (err));
      exit (1);
  }
  

  if ((err = snd_pcm_hw_params_set_channels (handle, hw_params, cfg.num_channels)) < 0) {
      fprintf (stderr, "cannot set channel count (%s)\n",
               snd_strerror (err));
      exit (1);
  }
 

  if ((err = snd_pcm_hw_params_set_access (handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
      fprintf (stderr, "cannot set access type (%s)\n",
           snd_strerror (err));
      exit (1);
  }

  if(cfg.input_buf_size > 0) {
    int frame_size = cfg.sample_size_bits / 8 * cfg.num_channels;
    snd_pcm_uframes_t num_frames = cfg.input_buf_size / frame_size;

    if ((err = snd_pcm_hw_params_set_period_size_near(handle, hw_params,  &num_frames, &dir)) < 0) {
     fprintf (stderr, "cannot set audio buf size=%ld frames (%s)\n", num_frames,
           snd_strerror (err));
      exit (1); 
    }
    
  }

  // Write the entire configuraion to the device.
  if ((err = snd_pcm_hw_params (handle, hw_params)) < 0) {
      fprintf (stderr, "cannot set parameters (%s)\n",
           snd_strerror (err));
      exit (1);
  }

  // Prepare the device for use.
  if ((err = snd_pcm_prepare (handle)) < 0) {
      fprintf (stderr, "cannot prepare audio interface for use (%s)\n",
              snd_strerror (err));
      exit (1);
  }

  fprintf(stdout, "Device is ready with handle=%s, state=%s\n", 
          snd_pcm_name(handle), snd_pcm_state_name(snd_pcm_state(handle)));

  // Get some useful debug stats.
  // Period size would be the size of the udp packet produced.
  alsa_frame_size = cfg.sample_size_bits / 8 * cfg.num_channels;
  snd_pcm_hw_params_get_period_size(hw_params, &alsa_period_size, &dir);
  snd_pcm_hw_params_get_period_time(hw_params, &alsa_period_time, &dir);

}


LnxBaseDevice::~LnxBaseDevice() {
  fprintf(stdout, "Shutting down the device\n");
  snd_pcm_hw_params_free (hw_params);
  snd_pcm_drain(handle);
  snd_pcm_close (handle);
}