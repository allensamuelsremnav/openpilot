#ifndef AUDIO_TS_FIFO_H_
#define AUDIO_TS_FIFO_H_

#include <deque>
#include <mutex>
#include <condition_variable>

#include "fifo_get_intf.h"
#include "fifo_put_intf.h"

namespace audio {

template <class T>
class TsFifo 
  : public FifoGetIntf<T>,
    public FifoPutIntf<T> {

  public:
    TsFifo() {}

    void put(T item) {
      deq_mutex.lock();
      deq.push_back(item);
      deq_mutex.unlock();
      cv.notify_one();
    }

    T get() {
      if (empty()) {
        wait_for_item();
      }

      deq_mutex.lock();
      T item = deq[0];
      deq.pop_front();
      deq_mutex.unlock();
      return item;
    }

    int size() {
        deq_mutex.lock();
        int len = (int) deq.size();
        deq_mutex.unlock();
        return len;
    }

    bool empty() {
        return size() == 0;
    }

    void wait_for_item() {
      while(empty()) {
        std::unique_lock<std::mutex> lk1(deq_mutex);
        cv.wait(lk1);
      }
    }

  private:
    std::mutex deq_mutex;
    std::condition_variable cv;

    std::deque<T> deq;
};

}
#endif