Queue::WaitFreeQueue::WaitFreeQueue(long numThreads, int patience) : patience(patience) {
    queue = new WaitFreeQueueStruct();
    queue->headNodePointer = new Node();
    queue->tailHandle = nullptr;
    queue->numThreads = numThreads;
    handles = new Handle *[numThreads]();
    for (long tid = 0; tid < numThreads; tid++) {
        handles[tid] = new Handle();
        registerThread(queue, handles[tid]);
    }
}

Queue::WaitFreeQueue::~WaitFreeQueue() {
    for (long tid = 0; tid < queue->numThreads; tid++) {
        delete handles[tid];
    }
    delete[] handles;
    Node *currentNodeToDelete = queue->headNodePointer;
    Node *nextNodeToDelete;
    while (currentNodeToDelete != nullptr) {
        nextNodeToDelete = currentNodeToDelete->nextNodePointer;
        delete currentNodeToDelete;
        currentNodeToDelete = nextNodeToDelete;
    }
    delete queue;
}

void Queue::WaitFreeQueue::enqueue(void *elem, long tid) {
    enqueue_(queue, handles[tid], elem, patience);
}

void *Queue::WaitFreeQueue::dequeue(long tid) {
    return dequeue_(queue, handles[tid], patience);
}
