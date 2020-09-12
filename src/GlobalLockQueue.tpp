void Queue::GlobalLockQueue::enqueue(void *elem, long tid) {
    queueLock.lock();
    queue.push(elem);
    queueLock.unlock();
}

void *Queue::GlobalLockQueue::dequeue(long tid) {
    void *elem = nullptr;
    queueLock.lock();
    if (!queue.empty()) {
        elem = queue.front();
        queue.pop();
    }
    queueLock.unlock();
    return elem;
}
