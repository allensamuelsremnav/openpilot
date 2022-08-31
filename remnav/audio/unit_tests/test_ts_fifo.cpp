#include "ts_fifo.h"
#include <iostream>

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <random>

int MAX_ENTRIES = 1000;

void enq_thread(audio::TsFifo<int> &fifo) {
    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<> distrib(0, 3);

    std::cout << "EnqThread:Starting to push " << MAX_ENTRIES << " entries" << std::endl;
    for(int i = 0; i < MAX_ENTRIES; i++) {
        fifo.put(i);
        std::this_thread::sleep_for(std::chrono::milliseconds(distrib(gen)));
    }
}


void deq_thread(audio::TsFifo<int> &fifo) {
    int item;

    for(int i = 0; i < MAX_ENTRIES; i++) {
        //deq.wait_for_item();
        item = fifo.get();
        ASSERT_EQ(i, item);
    }
    std::cout << "DeqThread: Checked all the entries" << std::endl;
}

TEST(FifoTest, enq_deq_incr) {
    audio::TsFifo<int> fifo;

    std::thread t2(deq_thread, std::ref(fifo));
    std::thread t1(enq_thread, std::ref(fifo));
   
    t1.join();
    t2.join();

}