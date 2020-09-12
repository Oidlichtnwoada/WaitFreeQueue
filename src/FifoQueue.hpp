#pragma once

namespace Queue {
    class FifoQueue {
    public:
        virtual ~FifoQueue() = default;

        virtual void enqueue(void *elem, long tid) = 0;

        virtual void *dequeue(long tid) = 0;
    };
}
