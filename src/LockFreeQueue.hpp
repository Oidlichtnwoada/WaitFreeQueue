#pragma once

#include "concurrentqueue.h"
#include "FifoQueue.hpp"

namespace Queue {
    class LockFreeQueue : public Queue::FifoQueue {
    public:
        LockFreeQueue() = default;

        ~LockFreeQueue() = default;

        void enqueue(void *elem, long tid);

        void *dequeue(long tid);

    private:
        moodycamel::ConcurrentQueue<void *> queue;
    };
}

#include "LockFreeQueue.tpp"
