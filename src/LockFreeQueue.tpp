void Queue::LockFreeQueue::enqueue(void *elem, long tid) {
    queue.enqueue(elem);
}

void *Queue::LockFreeQueue::dequeue(long tid) {
    void *elem;
    bool notEmpty = queue.try_dequeue(elem);
    if (notEmpty) {
        return elem;
    } else {
        return nullptr;
    }
}
