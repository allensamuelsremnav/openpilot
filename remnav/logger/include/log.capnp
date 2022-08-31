@0xcdc83ed35d2c8e31;

using Audio = import "audiolog.capnp";

struct Message {
    logMonoTime @0 :UInt64;
    valid @1 :Bool = true;

    union {
        audioPcmConfig @2 :Audio.AudioPcmConfig;
        audioSocketConfig @3 :Audio.AudioSocketConfig;
    }
}
