@0x95eb52515d6da4fc;

struct AudioPcmConfig {
    sampleSizeBits @0 :Int32;
    samplingRateHz @1 :Int32;
    numChannels @2 :Int32;
    bufSize @3 :Int32;
}

struct AudioSocketConfig {
    serverIP @0 :Text;
    udpPort @1 :Int32;
    audioBufSize @2 :Int32;
    socketStatus @3 :SocketStatus;
    
    enum SocketStatus {
        init @0;
        connected @1;
        failed @2;
        exited @3;
    }
    
}