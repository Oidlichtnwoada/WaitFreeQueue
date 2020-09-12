#pragma once

#include "FifoQueue.hpp"

#include <mutex>
#include <queue>

namespace Queue {
    class GlobalLockQueue : public Queue::FifoQueue {
    public:
        GlobalLockQueue() = default;

        ~GlobalLockQueue() = default;

        void enqueue(void *elem, long tid);

        void *dequeue(long tid);

    private:
        std::mutex queueLock;
        std::queue<void *> queue;
    };
}

#include "GlobalLockQueue.tpp"
