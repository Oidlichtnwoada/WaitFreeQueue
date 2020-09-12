#pragma once

#ifndef WFQ_CELL_ARRAY_SIZE
#define WFQ_CELL_ARRAY_SIZE 1024
#endif

#ifndef WFQ_PATIENCE
#define WFQ_PATIENCE 16
#endif

#define nillptr ((void*)1)

// Profiling region
volatile bool __profiling_loop = false;
volatile thread_local uint64_t __profiling_loop_enq_patience = 0;
volatile thread_local uint64_t __profiling_loop_deq_patience = 0;
volatile thread_local uint64_t __profiling_loop_enq_fp = 0;
volatile thread_local uint64_t __profiling_loop_enq_sp = 0;
volatile thread_local uint64_t __profiling_loop_enq_slow_dijkstra = 0;
volatile thread_local uint64_t __profiling_loop_enq_slow_lin = 0;
volatile thread_local uint64_t __profiling_loop_deq_help = 0;
volatile thread_local uint64_t __profiling_loop_deq_fp = 0;
volatile thread_local uint64_t __profiling_loop_deq_sp = 0;
volatile thread_local uint64_t __profiling_num_cas = 0;
volatile thread_local uint64_t __profiling_num_cas_failed = 0;

template<typename _Tp>
inline bool CAE(std::atomic<_Tp> &atomic, _Tp &__p1, _Tp __p2,
                std::memory_order __m = std::memory_order_seq_cst) {
    if (__profiling_loop) {
        __profiling_num_cas++;
        if (atomic.compare_exchange_strong(__p1, __p2, __m)) {
            return true;
        } else {
            __profiling_num_cas_failed++;
            return false;
        }
    } else {
        return atomic.compare_exchange_strong(__p1, __p2, __m);
    }
}

struct EnqueueRequest {
    std::atomic<void *> valuePointer;
    std::atomic<long> lastTriedCellIndex;
};

struct DequeueRequest {
    std::atomic<long> lastTriedCellIndex;
    std::atomic<long> priorCandidateCellIndex;
};

struct Cell {
    std::atomic<void *> valuePointer;
    std::atomic<EnqueueRequest *> pendingEnqueueRequestPointer;
    std::atomic<DequeueRequest *> pendingDequeueRequestPointer;
};

struct Node {
    long nodeIndex;
    std::atomic<Node *> nextNodePointer;
    Cell cellArray[WFQ_CELL_ARRAY_SIZE];
};

struct Handle {
    std::atomic<Handle *> nextHandlePointer;
    std::atomic<long> hazardNodeIndex;
    std::atomic<Node *> enqueueNodePointer;
    long enqueueNodeIndex;
    std::atomic<Node *> dequeueNodePointer;
    long dequeueNodeIndex;
    EnqueueRequest enqueueRequest;
    DequeueRequest dequeueRequest;
    long nextEnqueueCellIndex;
    Handle *nextEnqueuerHandlePointerToHelp;
    Handle *nextDequeuerHandlePointerToHelp;
};

struct WaitFreeQueueStruct {
    std::atomic<long> nextEnqueueCellIndex;
    std::atomic<long> nextDequeueCellIndex;
    std::atomic<long> headNodeIndex;
    std::atomic<Node *> headNodePointer;
    std::atomic<Handle *> tailHandle;
    long numThreads;
};

// creates a ring of thread handles
void registerThread(WaitFreeQueueStruct *queue, Handle *handle) {
    handle->nextHandlePointer = nullptr;
    handle->hazardNodeIndex = -1;
    handle->enqueueNodePointer = queue->headNodePointer.load();
    handle->enqueueNodeIndex = handle->enqueueNodePointer.load()->nodeIndex;
    handle->dequeueNodePointer = queue->headNodePointer.load();
    handle->dequeueNodeIndex = handle->dequeueNodePointer.load()->nodeIndex;
    handle->enqueueRequest.lastTriedCellIndex = 0;
    handle->enqueueRequest.valuePointer = nullptr;
    handle->dequeueRequest.lastTriedCellIndex = 0;
    handle->dequeueRequest.priorCandidateCellIndex = -1;
    handle->nextEnqueueCellIndex = 0;
    Handle *acquiredHandleTail = queue->tailHandle;
    if (acquiredHandleTail == nullptr) {
        handle->nextHandlePointer = handle;
        if (queue->tailHandle.compare_exchange_strong(acquiredHandleTail, handle)) {
            handle->nextEnqueuerHandlePointerToHelp = handle->nextHandlePointer;
            handle->nextDequeuerHandlePointerToHelp = handle->nextHandlePointer;
            return;
        }
    }
    Handle *nextHandlePointer;
    do {
        nextHandlePointer = queue->tailHandle.load()->nextHandlePointer.load();
        handle->nextHandlePointer = nextHandlePointer;
    } while (!queue->tailHandle.load()->nextHandlePointer.compare_exchange_strong(nextHandlePointer, handle));
    handle->nextEnqueuerHandlePointerToHelp = handle->nextHandlePointer;
    handle->nextDequeuerHandlePointerToHelp = handle->nextHandlePointer;
}

// returns the currentNode where an operation takes place, the hazardNode,
// the search starts at oldNode if hazardNodeIndex < currentNodeIndex
Node *check(std::atomic<long> *hazardNodeIndexPointer, Node *currentNode, Node *oldNode) {
    long hazardNodeIndex = hazardNodeIndexPointer->load();
    if (hazardNodeIndex < currentNode->nodeIndex) {
        while (oldNode->nodeIndex < hazardNodeIndex) {
            oldNode = oldNode->nextNodePointer;
        }
        currentNode = oldNode;
    }
    return currentNode;
}

// replaces the content of nodePointer with currentNode if nodePointer is smaller comparing indices, if it has succeeded
// additionally the currentNode is set to the acquiredNodePtr, then hazardNode is searched via check and returned, in
// the other case currentNode can be returned directly because it is the hazardNode
Node *
update(std::atomic<Node *> *nodePointer, Node *currentNode, std::atomic<long> *hazardNodeIndexPointer, Node *oldNode) {
    Node *acquiredNodePointer = nodePointer->load();
    if (acquiredNodePointer->nodeIndex < currentNode->nodeIndex) {
        if (!CAE(*nodePointer, acquiredNodePointer, currentNode)) {
            if (acquiredNodePointer->nodeIndex < currentNode->nodeIndex) {
                currentNode = acquiredNodePointer;
            }
        }
        currentNode = check(hazardNodeIndexPointer, currentNode, oldNode);
    }
    return currentNode;
}

// searches for unused nodes and frees them
void cleanup(WaitFreeQueueStruct *queue, Handle *handle) {
    long acquiredHeadNodeIndex = queue->headNodeIndex.load();
    Node *currentNodePointer = handle->dequeueNodePointer;
    if (currentNodePointer->nodeIndex - acquiredHeadNodeIndex < queue->numThreads) {
        // return if there are not enough nodes to cleanup
        return;
    }
    if (!CAE(queue->headNodeIndex, acquiredHeadNodeIndex, (long) -1)) {
        // try to signal that this thread is doing the cleanup, otherwise return
        return;
    }
    long nextDequeueCellIndex = queue->nextDequeueCellIndex;
    long nextEnqueueCellIndex = queue->nextEnqueueCellIndex;
    while (nextEnqueueCellIndex <= nextDequeueCellIndex &&
           !CAE(queue->nextEnqueueCellIndex, nextEnqueueCellIndex, nextDequeueCellIndex + 1)) {
        // increase enqueueCellIndex to nextDequeueCellIndex + 1 to enable more fast path enqueues,
    }
    Node *currentHeadNodePointer = queue->headNodePointer;
    Handle *currentHandle = handle;
    Handle *currentHandleArray[queue->numThreads];
    long index = 0;
    do {
        currentNodePointer = check(&currentHandle->hazardNodeIndex, currentNodePointer, currentHeadNodePointer);
        currentNodePointer = update(&currentHandle->enqueueNodePointer, currentNodePointer,
                                    &currentHandle->hazardNodeIndex, currentHeadNodePointer);
        currentNodePointer = update(&currentHandle->dequeueNodePointer, currentNodePointer,
                                    &currentHandle->hazardNodeIndex, currentHeadNodePointer);
        currentHandleArray[index++] = currentHandle;
        currentHandle = currentHandle->nextHandlePointer;
    } while (currentNodePointer->nodeIndex > acquiredHeadNodeIndex && currentHandle != handle);
    // reverse traversal of the previous stored handles
    while (currentNodePointer->nodeIndex > acquiredHeadNodeIndex && --index >= 0) {
        currentNodePointer = check(&currentHandleArray[index]->hazardNodeIndex, currentNodePointer,
                                   currentHeadNodePointer);
    }
    long currentNodeIndex = currentNodePointer->nodeIndex;
    if (currentNodeIndex <= acquiredHeadNodeIndex) {
        // update headNodeIndex if there are no nodes to delete
        queue->headNodeIndex.store(acquiredHeadNodeIndex);
    } else {
        queue->headNodePointer = currentNodePointer;
        queue->headNodeIndex.store(currentNodeIndex);
        // delete all unused nodes
        while (currentHeadNodePointer != currentNodePointer) {
            Node *nextNodeToDelete = currentHeadNodePointer->nextNodePointer;
            delete currentHeadNodePointer;
            currentHeadNodePointer = nextNodeToDelete;
        }
    }
}

// returns a Cell pointer to the requested Cell, updates the nodePointer and allocates all required nodes
Cell *findCell(std::atomic<Node *> *nodePointer, long cellIndex) {
    Node *currentNode = *nodePointer;
    for (long currentNodeIndex = currentNode->nodeIndex;
         currentNodeIndex < cellIndex / WFQ_CELL_ARRAY_SIZE; currentNodeIndex++) {
        // Traverse list to target node with id = floor(cellIndex/WFQ_CELL_ARRAY_SIZE)
        Node *nextNode = currentNode->nextNodePointer;
        if (nextNode == nullptr) {
            // the list needs another node
            // allocate one and try to extend the list
            Node *newNode = new Node();
            newNode->nodeIndex = currentNodeIndex + 1;
            if (CAE(currentNode->nextNodePointer, nextNode, newNode)) {
                nextNode = newNode;
            } else {
                // we were to slow -> another thread already extended the list
                nextNode = currentNode->nextNodePointer;
                delete newNode;
            }
        }
        currentNode = nextNode;
    }
    *nodePointer = currentNode;
    return &currentNode->cellArray[cellIndex % WFQ_CELL_ARRAY_SIZE];
}

// eventually enqueues the valuePointer over the fast path without the help of other threads
bool enqueueFastPath(WaitFreeQueueStruct *queue, Handle *handle, void *valuePointer, long *lastTriedCellIndex) {
    // obtain cell index and locate candidate cell
    long enqueueCellIndex = queue->nextEnqueueCellIndex.fetch_add(1);
    Cell *enqueueCell = findCell(&handle->enqueueNodePointer, enqueueCellIndex);
    void *compareValue = nullptr;
    if (CAE(enqueueCell->valuePointer, compareValue, valuePointer)) {
        // successfully enqueued
        return true;
    } else {
        // did not manage to exchange the value pointer -> failed to enqueue
        // return last triedCell to be faster next time.
        *lastTriedCellIndex = enqueueCellIndex;
        return false;
    }
}

// enqueues a valuePointer over the slow path, registers enqueueRequest on each candidate cell, eventually gets
// help from other thread, this is when CAS fails and lastTriedIndex > nextEnqueueCell and the last while loop
// ensures linearizability
void enqueueSlowPath(WaitFreeQueueStruct *queue, Handle *handle, void *valuePointer, long lastTriedCellIndex) {
    // publish enqueue request
    EnqueueRequest *enqueueRequest = &handle->enqueueRequest;
    enqueueRequest->valuePointer = valuePointer;
    enqueueRequest->lastTriedCellIndex.store(lastTriedCellIndex);
    // local enqueue pointer as we might need an earlier cell in line 228.
    std::atomic<Node *> acquiredEnqueueNodePointer = handle->enqueueNodePointer.load();
    long nextEnqueueCellIndex;
    Cell *enqueueCell;
    do {
        if (__profiling_loop) __profiling_loop_enq_slow_dijkstra++;
        // locate new candidate cell
        nextEnqueueCellIndex = queue->nextEnqueueCellIndex.fetch_add(1);
        enqueueCell = findCell(&acquiredEnqueueNodePointer, nextEnqueueCellIndex);
        EnqueueRequest *compareValue = nullptr;
        // Dijkstra's Protocol https://sci-hub.tw/10.1145/365559.365617
        if (CAE(enqueueCell->pendingEnqueueRequestPointer, compareValue, enqueueRequest) &&
            enqueueCell->valuePointer != nillptr) {
            CAE(enqueueRequest->lastTriedCellIndex, lastTriedCellIndex, -nextEnqueueCellIndex);
            // request claimed
            break;
        }
    } while (enqueueRequest->lastTriedCellIndex > 0);
    // request claimed for a cell, find that cell
    lastTriedCellIndex = -enqueueRequest->lastTriedCellIndex;
    enqueueCell = findCell(&handle->enqueueNodePointer, lastTriedCellIndex);
    // linearizability:
    // Ensure lastTriedCellIndex is less or equal to nextEnqueueCellIndex such that later
    // enqueued values are not dequeued before our value and this operation has finished
    if (lastTriedCellIndex > nextEnqueueCellIndex) {
        long acquiredNextEnqueueCellIndex = queue->nextEnqueueCellIndex;
        while (acquiredNextEnqueueCellIndex <= lastTriedCellIndex &&
               !CAE(queue->nextEnqueueCellIndex, acquiredNextEnqueueCellIndex,
                    lastTriedCellIndex + 1)) {
            if (__profiling_loop) __profiling_loop_enq_slow_lin++;
        }
    }
    // store claimed value into the cell
    enqueueCell->valuePointer = valuePointer;
}

// enqueues a single valuePointer over the fast or slow path
void enqueue_(WaitFreeQueueStruct *queue, Handle *handle, void *valuePointer, const int patience) {
    handle->hazardNodeIndex = handle->enqueueNodeIndex;
    bool isValuePointerEnqueued = false;
    long lastTriedCellIndex;
    for (long i = 0; i < patience; i++) {
        if (__profiling_loop) __profiling_loop_enq_patience++;
        // try fast enqueue
        isValuePointerEnqueued = enqueueFastPath(queue, handle, valuePointer, &lastTriedCellIndex);
        if (isValuePointerEnqueued) {
            if (__profiling_loop) __profiling_loop_enq_fp++;
            // fast enqueue succeded
            break;
        }
    }
    if (!isValuePointerEnqueued) {
        // if we failed to enqueue via the fast path and did exhaust our patience,
        // try the save slow path method
        enqueueSlowPath(queue, handle, valuePointer, lastTriedCellIndex);
        if (__profiling_loop) __profiling_loop_enq_sp++;
    }
    handle->enqueueNodeIndex = handle->enqueueNodePointer.load()->nodeIndex;
    handle->hazardNodeIndex.store(-1);
}

// helping function to ensure that another thread completes enqueue in constant number of steps
void *helpEnqueuer(WaitFreeQueueStruct *queue, Handle *handle, Cell *dequeueCell, long dequeueCellIndex) {
    void *valuePointer = dequeueCell->valuePointer;
    if ((valuePointer != nillptr && valuePointer != nullptr) ||
        (valuePointer == nullptr && !CAE(dequeueCell->valuePointer, valuePointer, nillptr) &&
         valuePointer != nillptr)) {
        // return if there is currently no element in the dequeueCell
        return valuePointer;
    }
    // value of dequeueCell is TOP -> help slow path enqueuers
    EnqueueRequest *enqueueRequest = dequeueCell->pendingEnqueueRequestPointer;
    if (enqueueRequest == nullptr) {
        // no enqueueRequest in our dequeueCell yet
        Handle *nextEnqueuerHandlePointerToHelp = handle->nextEnqueuerHandlePointerToHelp;
        EnqueueRequest *enqueueRequestToHelp = &nextEnqueuerHandlePointerToHelp->enqueueRequest;
        long lastTriedCellIndex = enqueueRequestToHelp->lastTriedCellIndex;
        if (handle->nextEnqueueCellIndex != 0 && handle->nextEnqueueCellIndex != lastTriedCellIndex) {
            handle->nextEnqueueCellIndex = 0;
            handle->nextEnqueuerHandlePointerToHelp = nextEnqueuerHandlePointerToHelp->nextHandlePointer;
            nextEnqueuerHandlePointerToHelp = handle->nextEnqueuerHandlePointerToHelp;
            enqueueRequestToHelp = &nextEnqueuerHandlePointerToHelp->enqueueRequest;
            lastTriedCellIndex = enqueueRequestToHelp->lastTriedCellIndex;
        }
        if (lastTriedCellIndex > 0 && lastTriedCellIndex <= dequeueCellIndex &&
            !CAE(dequeueCell->pendingEnqueueRequestPointer, enqueueRequest, enqueueRequestToHelp) &&
            enqueueRequest != enqueueRequestToHelp)
            handle->nextEnqueueCellIndex = lastTriedCellIndex;
        else {
            handle->nextEnqueueCellIndex = 0;
            handle->nextEnqueuerHandlePointerToHelp = nextEnqueuerHandlePointerToHelp->nextHandlePointer;
        }
        if (enqueueRequest == nullptr &&
            CAE(dequeueCell->pendingEnqueueRequestPointer, enqueueRequest,
                (EnqueueRequest *) nillptr)) {
            enqueueRequest = (EnqueueRequest *) nillptr;
        }
    }
    if (enqueueRequest == nillptr) {
        return (queue->nextEnqueueCellIndex <= dequeueCellIndex ? nullptr : nillptr);
    }
    long nextEnqueueCellIndex = enqueueRequest->lastTriedCellIndex.load();
    void *enqueueRequestValuePointer = enqueueRequest->valuePointer.load();
    if (nextEnqueueCellIndex > dequeueCellIndex) {
        if (dequeueCell->valuePointer == nillptr && queue->nextEnqueueCellIndex <= dequeueCellIndex) {
            // there is no valuePointer available for a dequeue operation
            return nullptr;
        }
    } else {
        if ((nextEnqueueCellIndex > 0 &&
             CAE(enqueueRequest->lastTriedCellIndex, nextEnqueueCellIndex, -dequeueCellIndex)) ||
            (nextEnqueueCellIndex == -dequeueCellIndex && dequeueCell->valuePointer == nillptr)) {
            long enqueueCellIndex = queue->nextEnqueueCellIndex;
            while (enqueueCellIndex <= dequeueCellIndex &&
                   !CAE(queue->nextEnqueueCellIndex, enqueueCellIndex, dequeueCellIndex + 1)) {
                // increase nextEnqueueCellIndex to enable more fast path enqueues
            }
            dequeueCell->valuePointer = enqueueRequestValuePointer;
        }
    }
    return dequeueCell->valuePointer;
}

// helping function to ensure that another thread completes dequeue in constant number of steps
void helpDequeuer(WaitFreeQueueStruct *queue, Handle *handle, Handle *dequeuerHandle) {
    // inspect a dequeue request
    DequeueRequest *dequeueRequest = &dequeuerHandle->dequeueRequest;
    long priorCandidateCellIndex = dequeueRequest->priorCandidateCellIndex;
    long lastTriedCellIndex = dequeueRequest->lastTriedCellIndex;
    // if this request does not need help -> return
    if (priorCandidateCellIndex < lastTriedCellIndex) {
        return;
    }
    std::atomic<Node *> dequeueNodePointer{};
    dequeueNodePointer.store(dequeuerHandle->dequeueNodePointer.load());
    handle->hazardNodeIndex.store(dequeuerHandle->hazardNodeIndex.load());
    priorCandidateCellIndex = dequeueRequest->priorCandidateCellIndex;
    long currentTryCellIndex = lastTriedCellIndex + 1, oldTryCellIndex = lastTriedCellIndex, candidateCellIndex = 0;
    while (true) {
        if (__profiling_loop) __profiling_loop_deq_help++;
        // find a candidate cell, if i do not have one
        // loop breaks when either find a candidate or a
        // candidate is anounced.
        std::atomic<Node *> acquiredDequeueNodePointer = dequeueNodePointer.load();
        // search a candidate cell
        for (; priorCandidateCellIndex == oldTryCellIndex && candidateCellIndex == 0; currentTryCellIndex++) {
            Cell *nextDequeueCell = findCell(&acquiredDequeueNodePointer, currentTryCellIndex);
            long nextDequeueCellIndex = queue->nextDequeueCellIndex;
            while (nextDequeueCellIndex <= currentTryCellIndex &&
                   !CAE(queue->nextDequeueCellIndex, nextDequeueCellIndex,
                        currentTryCellIndex + 1)) {
                // reserve the next dequeue cell for this thread
            }
            // help an enqueuer thread and eventually take its valuePointer
            // its a candidate if helpEnqueuer returns nullptr or a value
            // not claimed by dequeues
            void *valuePointer = helpEnqueuer(queue, handle, nextDequeueCell, currentTryCellIndex);
            if (valuePointer == nullptr ||
                (valuePointer != nillptr && nextDequeueCell->pendingDequeueRequestPointer == nullptr)) {
                candidateCellIndex = currentTryCellIndex;
            } else {
                priorCandidateCellIndex = dequeueRequest->priorCandidateCellIndex.load();
            }
        }
        // candidate cell found or priorCandidateCellIndex != oldTryCellIndex
        if (candidateCellIndex != 0) {
            // announce candidate cell
            if (CAE(dequeueRequest->priorCandidateCellIndex, priorCandidateCellIndex,
                    candidateCellIndex)) {
                // sucessfully announced it
                priorCandidateCellIndex = candidateCellIndex;
            }
            if (priorCandidateCellIndex >= candidateCellIndex) {
                candidateCellIndex = 0;
            }
        }
        if (priorCandidateCellIndex < 0 || dequeueRequest->lastTriedCellIndex != lastTriedCellIndex) {
            // dequeue cell reserved by other
            break;
        }
        // find the announced candidate cell
        Cell *candidateCell = findCell(&dequeueNodePointer, priorCandidateCellIndex);
        DequeueRequest *compareValue = nullptr;
        if (candidateCell->valuePointer == nillptr ||
            CAE(candidateCell->pendingDequeueRequestPointer, compareValue, dequeueRequest) ||
            compareValue == dequeueRequest) {
            // request is complete, try to reset priorCandidateCellIndex
            CAE(dequeueRequest->priorCandidateCellIndex, priorCandidateCellIndex,
                -priorCandidateCellIndex);
            // dequeue cell reserved by this thread
            break;
        }
        // prepere for next iteration
        // updating the running cell indices variables
        oldTryCellIndex = priorCandidateCellIndex;
        // if announced candidate is newer than visited cell
        // abandon candidate and bump currentTryCellIndex
        if (priorCandidateCellIndex >= currentTryCellIndex) {
            currentTryCellIndex = priorCandidateCellIndex + 1;
        }
    }
}

// eventually dequeues a valuePointer over the fast path, if no value is ready helpEnqueuer returns
// a nullptr, otherwise a nillptr or a valid pointer is returned, to ensure that only one thread returns a
// a single valid pointer, there is a CAS operation on the pendingDequeueRequest for the respective cell,
// if this operation fails or it a nillptr was returned before, the fast path dequeue failed, but there may be still
// elements to dequeue
void *dequeueFastPath(WaitFreeQueueStruct *queue, Handle *handle, long *lastTriedCellIndex) {
    // obtain cell index and locate candidate cell
    long dequeueCellIndex = queue->nextDequeueCellIndex.fetch_add(1);
    Cell *dequeueCell = findCell(&handle->dequeueNodePointer, dequeueCellIndex);
    void *valuePointer = helpEnqueuer(queue, handle, dequeueCell, dequeueCellIndex);
    DequeueRequest *compareValue = nullptr;
    if (valuePointer == nullptr) {
        return nullptr;
    }
    // the cell has a value and we claimed it
    if (valuePointer != nillptr &&
        CAE(dequeueCell->pendingDequeueRequestPointer, compareValue, (DequeueRequest *) nillptr)) {
        return valuePointer;
    }
    // otherwise fail and return new lastTriedCellIndex
    *lastTriedCellIndex = dequeueCellIndex;
    return nillptr;
}

// dequeues an element on the slow path if one is available, the functionality is implemented in
// the helpDequeuer function, the calling thread helps itself using this function
void *dequeueSlowPath(WaitFreeQueueStruct *queue, Handle *handle, long lastTriedCellIndex) {
    // publish dequeue request
    DequeueRequest *dequeueRequest = &handle->dequeueRequest;
    dequeueRequest->lastTriedCellIndex.store(lastTriedCellIndex);
    dequeueRequest->priorCandidateCellIndex.store(lastTriedCellIndex);
    helpDequeuer(queue, handle, handle);
    // find a destination cell and read its value
    long dequeueCellIndex = -dequeueRequest->priorCandidateCellIndex;
    Cell *dequeueCell = findCell(&handle->dequeueNodePointer, dequeueCellIndex);
    void *valuePointer = dequeueCell->valuePointer;
    return valuePointer == nillptr ? nullptr : valuePointer;
}

// dequeues a valuePointer via the fast or slow path, if no value is ready the thread helps other threads
void *dequeue_(WaitFreeQueueStruct *queue, Handle *handle, const int patience) {
    handle->hazardNodeIndex = handle->dequeueNodeIndex;
    void *valuePointer;
    long lastTriedCellIndex;
    for (long i = 0; i < patience; i++) {
        if (__profiling_loop) __profiling_loop_deq_patience++;
        valuePointer = dequeueFastPath(queue, handle, &lastTriedCellIndex);
        if (valuePointer != nillptr) {
            if (__profiling_loop) __profiling_loop_deq_fp++;
            break;
        }
    }
    if (valuePointer == nillptr) {
        valuePointer = dequeueSlowPath(queue, handle, lastTriedCellIndex);
        if (__profiling_loop) __profiling_loop_deq_sp++;
        // valuePointer is a value or EMPTY
    }
    if (valuePointer != nullptr) {
        // got a value, so help a peer out
        helpDequeuer(queue, handle, handle->nextDequeuerHandlePointerToHelp);
        // move to next peer
        handle->nextDequeuerHandlePointerToHelp = handle->nextDequeuerHandlePointerToHelp->nextHandlePointer;
    }
    handle->dequeueNodeIndex = handle->dequeueNodePointer.load()->nodeIndex;
    handle->hazardNodeIndex.store(-1);
    cleanup(queue, handle);
    return valuePointer;
}

void get_and_clear_profiling_data(
        uint64_t &profiling_loop_deq_patience,
        uint64_t &profiling_loop_enq_patience,
        uint64_t &profiling_loop_enq_fp,
        uint64_t &profiling_loop_enq_sp,
        uint64_t &profiling_loop_enq_slow_dijkstra,
        uint64_t &profiling_loop_enq_slow_lin,
        uint64_t &profiling_loop_deq_help,
        uint64_t &profiling_loop_deq_fp,
        uint64_t &profiling_loop_deq_sp,
        uint64_t &profiling_num_cas,
        uint64_t &profiling_num_cas_failed
) {
    profiling_loop_deq_patience = __profiling_loop_deq_patience;
    profiling_loop_enq_patience = __profiling_loop_enq_patience;
    profiling_loop_enq_fp = __profiling_loop_enq_fp;
    profiling_loop_enq_sp = __profiling_loop_enq_sp;
    profiling_loop_enq_slow_dijkstra = __profiling_loop_enq_slow_dijkstra;
    profiling_loop_enq_slow_lin = __profiling_loop_enq_slow_lin;
    profiling_loop_deq_help = __profiling_loop_deq_help;
    profiling_loop_deq_fp = __profiling_loop_deq_fp;
    profiling_loop_deq_sp = __profiling_loop_deq_sp;
    profiling_num_cas = __profiling_num_cas;
    profiling_num_cas_failed = __profiling_num_cas_failed;

    __profiling_loop_enq_patience = 0;
    __profiling_loop_deq_patience = 0;
    __profiling_loop_enq_fp = 0;
    __profiling_loop_enq_sp = 0;
    __profiling_loop_enq_slow_dijkstra = 0;
    __profiling_loop_enq_slow_lin = 0;
    __profiling_loop_deq_help = 0;
    __profiling_loop_deq_fp = 0;
    __profiling_loop_deq_sp = 0;
    __profiling_num_cas = 0;
    __profiling_num_cas_failed = 0;
}