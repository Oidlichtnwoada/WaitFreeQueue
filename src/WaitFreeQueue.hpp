#pragma once

#include "FifoQueue.hpp"
#include "WaitFreeQueue.h"

namespace Queue {
    class WaitFreeQueue : public Queue::FifoQueue {
    public:
        explicit WaitFreeQueue(long numThreads, int patience);

        ~WaitFreeQueue();

        void enqueue(void *elem, long tid);

        void *dequeue(long tid);

    private:
        WaitFreeQueueStruct *queue;
        Handle **handles;

        const int patience;
    };
}

#include "WaitFreeQueue.tpp"
