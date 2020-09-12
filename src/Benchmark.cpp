#include <atomic>
#include <functional>
#include <iostream>
#include <string>
#include <chrono>
#include <ctime>
#include "cxxopts.hpp"
#include "defaults.h"
#include "GlobalLockQueue.hpp"
#include "LockFreeQueue.hpp"
#include "WaitFreeQueue.hpp"

using namespace std::chrono;

// setting default parameters
//region Configuration
long num_threads = DEFAULT_NUM_THREADS;
long num_objects = DEFAULT_NUM_OBJECTS;
long initial_queue_size = DEFAULT_INITIAL_QUEUE_SIZE;
long enqueuer_threads = DEFAULT_ENQUEUER_THREADS;
long test_repetitions = DEFAULT_TEST_REPETITIONS;
int patience = DEFAULT_PATIENCE;
bool test_waitfree = DEFAULT_TEST_WAITFREE;
bool test_lockfree = DEFAULT_TEST_LOCKFREE;
bool test_globallock = DEFAULT_TEST_GLOBALLOCK;
bool is_correctness_test = DEFAULT_IS_CORRECTNESS_TEST;
bool include_header = DEFAULT_INCLUDE_HEADER;
//endregion

// triggers the parallel execution of the passed function with the passed arguments, a thread id and a barrier counter
//region Helper Functionality
template<typename Function, typename... Args>
void parallelExecution(Function f, Args ... args) {
    auto *threadPool = new std::thread[num_threads];
    auto atomic_barrier = new std::atomic<long>(0);
    for (long tid = 1; tid < num_threads; tid++) {
        threadPool[tid] = std::thread(f, tid, atomic_barrier, args...);
    }
    f(0, atomic_barrier, args...);

    for (long tid = 1; tid < num_threads; tid++) {
        threadPool[tid].join();
    }

    delete[] threadPool;
    delete atomic_barrier;
}

// add the specified amount of elements to the passed queue
void addElementsToQueue(Queue::FifoQueue *queue, long elements) {
    for (long i = 0; i < elements; i++) {
        queue->enqueue(DEFAULT_ENQUEUE_VALUE, 0);
    }
}

// each thread can only proceed if all threads have executed the barrier
void threadBarrier(std::atomic<long> *atomic_barrier) {
    atomic_barrier->fetch_add(1);
    while (atomic_barrier->load() < num_threads) {}
}

long getNumberOfEnqueuers() {
    return enqueuer_threads;
}

long getNumberOfDequeuers() {
    return num_threads - getNumberOfEnqueuers();
}

// fills the queue appropriately before each benchmark
void fillQueue(long tid, Queue::FifoQueue *queue) {
    if (tid == 0) {
        addElementsToQueue(queue, initial_queue_size);
        if (getNumberOfEnqueuers() == 0) {
            addElementsToQueue(queue, num_objects);
        }
    }
}

// simulates the presence of enqueuers during benchmark
void enqueuerContention(long tid, std::atomic<long> *atomic_barrier, Queue::FifoQueue *queue) {
    while (atomic_barrier->load() < (num_threads + getNumberOfDequeuers())) {
        for (long i = 0; i < num_objects / getNumberOfDequeuers(); i++) {
            queue->enqueue(DEFAULT_ENQUEUE_VALUE, tid);
        }
    }
}

// simulates the presence of dequeuers during benchmark
void dequeuerContention(long tid, std::atomic<long> *atomic_barrier, Queue::FifoQueue *queue) {
    while (atomic_barrier->load() < (num_threads + getNumberOfEnqueuers())) {
        for (long i = 0; i < num_objects / getNumberOfEnqueuers(); i++) {
            queue->dequeue(tid);
        }
    }
}
//endregion

// starts the benchmark for each specified queue
//region Benchmark test
void benchmarkEnqueueLatencyWorkload(long tid, std::atomic<long> *atomic_barrier, Queue::FifoQueue *queue,
                                     std::atomic<long> *enqueueLatencySum) {
    fillQueue(tid, queue);
    threadBarrier(atomic_barrier);

    if (tid < getNumberOfEnqueuers()) {
        // Enqueuer
        long duration_sum = 0;
        for (long i = 0; i < num_objects / getNumberOfEnqueuers(); i++) {
            high_resolution_clock::time_point start = high_resolution_clock::now();
            queue->enqueue(DEFAULT_ENQUEUE_VALUE, tid);
            high_resolution_clock::time_point end = high_resolution_clock::now();
            duration_sum += duration_cast<nanoseconds>(end - start).count();
        }
        enqueueLatencySum->fetch_add(duration_sum);
        atomic_barrier->fetch_add(1);
    } else {
        // Dequeuer
        dequeuerContention(tid, atomic_barrier, queue);
    }
}

void benchmarkDequeueLatencyWorkload(long tid, std::atomic<long> *atomic_barrier, Queue::FifoQueue *queue,
                                     std::atomic<long> *dequeueLatencySum) {
    fillQueue(tid, queue);
    threadBarrier(atomic_barrier);

    if (tid < getNumberOfEnqueuers()) {
        // Enqueuer
        enqueuerContention(tid, atomic_barrier, queue);
    } else {
        // Dequeuer
        long duration_sum = 0;
        for (long i = 0; i < num_objects / getNumberOfDequeuers(); i++) {
            high_resolution_clock::time_point start = high_resolution_clock::now();
            queue->dequeue(tid);
            high_resolution_clock::time_point end = high_resolution_clock::now();
            duration_sum += duration_cast<nanoseconds>(end - start).count();
        }
        dequeueLatencySum->fetch_add(duration_sum);
        atomic_barrier->fetch_add(1);
    }
}

void benchmarkEnqueueThroughputWorkload(long tid, std::atomic<long> *atomic_barrier, Queue::FifoQueue *queue,
                                        std::atomic<long> *enqueueThroughputDuration,
                                        std::mutex *throughputDurationLock) {
    fillQueue(tid, queue);
    threadBarrier(atomic_barrier);

    if (tid < getNumberOfEnqueuers()) {
        // Enqueuer
        high_resolution_clock::time_point start = high_resolution_clock::now();
        for (long i = 0; i < num_objects / getNumberOfEnqueuers(); i++) {
            queue->enqueue(DEFAULT_ENQUEUE_VALUE, tid);
        }
        high_resolution_clock::time_point end = high_resolution_clock::now();
        long duration = duration_cast<nanoseconds>(end - start).count();

        // only save the duration of the slowest thread
        throughputDurationLock->lock();
        if (duration > enqueueThroughputDuration->load()) {
            enqueueThroughputDuration->store(duration);
        }
        throughputDurationLock->unlock();
        atomic_barrier->fetch_add(1);
    } else {
        // Dequeuer
        dequeuerContention(tid, atomic_barrier, queue);
    }

}

void benchmarkDequeueThroughputWorkload(long tid, std::atomic<long> *atomic_barrier, Queue::FifoQueue *queue,
                                        std::atomic<long> *dequeueThroughputDuration,
                                        std::mutex *throughputDurationLock) {
    fillQueue(tid, queue);
    threadBarrier(atomic_barrier);

    if (tid < getNumberOfEnqueuers()) {
        // Enqueuer
        enqueuerContention(tid, atomic_barrier, queue);
    } else {
        // Dequeuer
        high_resolution_clock::time_point start = high_resolution_clock::now();
        for (long i = 0; i < num_objects / getNumberOfDequeuers(); i++) {
            queue->dequeue(tid);
        }
        high_resolution_clock::time_point end = high_resolution_clock::now();
        long duration = duration_cast<nanoseconds>(end - start).count();

        // only save the duration of the slowest thread
        throughputDurationLock->lock();
        if (duration > dequeueThroughputDuration->load()) {
            dequeueThroughputDuration->store(duration);
        }
        throughputDurationLock->unlock();
        atomic_barrier->fetch_add(1);
    }

}

void loopProfilingWorkload(long tid, std::atomic<long> *atomic_barrier, Queue::FifoQueue *queue,
                           std::atomic<uint64_t> *profiling_loop_deq_patience_sum,
                           std::atomic<uint64_t> *profiling_loop_enq_patience_sum,
                           std::atomic<uint64_t> *profiling_loop_enq_fp_sum,
                           std::atomic<uint64_t> *profiling_loop_enq_sp_sum,
                           std::atomic<uint64_t> *profiling_loop_enq_slow_dijkstra_sum,
                           std::atomic<uint64_t> *profiling_loop_enq_slow_lin_sum,
                           std::atomic<uint64_t> *profiling_loop_deq_help_sum,
                           std::atomic<uint64_t> *profiling_loop_deq_fp_sum,
                           std::atomic<uint64_t> *profiling_loop_deq_sp_sum,
                           std::atomic<uint64_t> *profiling_num_cas_sum,
                           std::atomic<uint64_t> *profiling_num_cas_failed_sum
                           ) {
    fillQueue(tid, queue);
    threadBarrier(atomic_barrier);

    if (tid < getNumberOfEnqueuers()) {
        for (long i = 0; i < num_objects / getNumberOfEnqueuers(); i++) {
            queue->enqueue(DEFAULT_ENQUEUE_VALUE, tid);
        }
        atomic_barrier->fetch_add(1);
    } else {
        dequeuerContention(tid, atomic_barrier, queue);
    }

    uint64_t profiling_loop_deq_patience;
    uint64_t profiling_loop_enq_patience;
    uint64_t profiling_loop_enq_fp;
    uint64_t profiling_loop_enq_sp;
    uint64_t profiling_loop_enq_slow_dijkstra;
    uint64_t profiling_loop_enq_slow_lin;
    uint64_t profiling_loop_deq_help;
    uint64_t profiling_loop_deq_fp;
    uint64_t profiling_loop_deq_sp;
    uint64_t profiling_num_cas;
    uint64_t profiling_num_cas_failed;

    get_and_clear_profiling_data(
            profiling_loop_deq_patience,
            profiling_loop_enq_patience,
            profiling_loop_enq_fp,
            profiling_loop_enq_sp,
            profiling_loop_enq_slow_dijkstra,
            profiling_loop_enq_slow_lin,
            profiling_loop_deq_help,
            profiling_loop_deq_fp,
            profiling_loop_deq_sp,
            profiling_num_cas,
            profiling_num_cas_failed
    );

    profiling_loop_deq_patience_sum->fetch_add(profiling_loop_deq_patience);
    profiling_loop_enq_patience_sum->fetch_add(profiling_loop_enq_patience);
    profiling_loop_enq_fp_sum->fetch_add(profiling_loop_enq_fp);
    profiling_loop_enq_sp_sum->fetch_add(profiling_loop_enq_sp);
    profiling_loop_enq_slow_dijkstra_sum->fetch_add(profiling_loop_enq_slow_dijkstra);
    profiling_loop_enq_slow_lin_sum->fetch_add(profiling_loop_enq_slow_lin);
    profiling_loop_deq_help_sum->fetch_add(profiling_loop_deq_help);
    profiling_loop_deq_fp_sum->fetch_add(profiling_loop_deq_fp);
    profiling_loop_deq_sp_sum->fetch_add(profiling_loop_deq_sp);
    profiling_num_cas_sum->fetch_add(profiling_num_cas);
    profiling_num_cas_failed_sum->fetch_add(profiling_num_cas_failed);
}

void
run_benchmark_test_internal(const std::function<Queue::FifoQueue *()> &queue_factory,
                            const char *queue_name) {
    // "WFQ_MAX_PATIENCE" << SEP << "WFQ_ENQ_PATIENCE_ITER" << SEP
    //                  << "WFQ_DEQ_PATIENCE_ITER" << SEP << "WFQ_ENQ_SLOW_DIJKSTRA_ITER" << SEP << "WFQ_ENQ_SLOW_LIN_ITER"
    long enqueueLatency;
    long dequeueLatency;
    double enqueueThroughput;
    double dequeueThroughput;
    uint64_t dequeuePatience = 0;
    uint64_t enqueuePatience = 0;
    uint64_t enqueueFastPath = 0;
    uint64_t enqueueSlowPath = 0;
    uint64_t enqueueSlowDijkstra = 0;
    uint64_t enqueueSlowLinerization = 0;
    uint64_t dequeueHelper = 0;
    uint64_t dequeueFastPath = 0;
    uint64_t dequeueSlowPath = 0;
    uint64_t num_cas_sum = 0;
    uint64_t num_cas_failed_sum = 0;

    {
        auto queue = queue_factory();
        std::atomic<long> enqueueLatencySum = 0;
        parallelExecution(benchmarkEnqueueLatencyWorkload, queue, &enqueueLatencySum);
        if (getNumberOfEnqueuers() > 0) {
            enqueueLatency = enqueueLatencySum / num_objects;
        } else {
            enqueueLatency = -1;
        }
        delete queue;
    }
    {
        auto queue = queue_factory();
        std::atomic<long> dequeueLatencySum = 0;
        parallelExecution(benchmarkDequeueLatencyWorkload, queue, &dequeueLatencySum);
        if (getNumberOfDequeuers() > 0) {
            dequeueLatency = dequeueLatencySum / num_objects;
        } else {
            dequeueLatency = -1;
        }
        delete queue;
    }
    {
        auto queue = queue_factory();
        std::atomic<long> enqueueThroughputDuration = 0;
        auto throughputDurationLock = std::mutex();

        parallelExecution(benchmarkEnqueueThroughputWorkload, queue, &enqueueThroughputDuration,
                          &throughputDurationLock);
        if (getNumberOfEnqueuers() > 0) {
            enqueueThroughput = (double) (1000 * num_objects) / (double) enqueueThroughputDuration;
        } else {
            enqueueThroughput = -1;
        }
        delete queue;
    }
    {
        auto queue = queue_factory();
        std::atomic<long> dequeueThroughputDuration = 0;
        auto throughputDurationLock = std::mutex();

        parallelExecution(benchmarkDequeueThroughputWorkload, queue, &dequeueThroughputDuration,
                          &throughputDurationLock);
        if (getNumberOfDequeuers() > 0) {
            dequeueThroughput = (double) (1000 * num_objects) / (double) dequeueThroughputDuration;
        } else {
            dequeueThroughput = -1;
        }
        delete queue;
    }
    {
        auto queue = queue_factory();
        std::atomic<long> dequeueThroughputDuration = 0;
        auto throughputDurationLock = std::mutex();

        parallelExecution(benchmarkDequeueThroughputWorkload, queue, &dequeueThroughputDuration,
                          &throughputDurationLock);
        if (getNumberOfDequeuers() > 0) {
            dequeueThroughput = (double) (1000 * num_objects) / (double) dequeueThroughputDuration;
        } else {
            dequeueThroughput = -1;
        }
        delete queue;
    }
    if (strcmp(queue_name, "WFQ") == 0) {
        std::atomic<uint64_t> profiling_loop_deq_patience_sum(0);
        std::atomic<uint64_t> profiling_loop_enq_patience_sum(0);
        std::atomic<uint64_t> profiling_loop_enq_fp_sum(0);
        std::atomic<uint64_t> profiling_loop_enq_sp_sum(0);
        std::atomic<uint64_t> profiling_loop_enq_slow_dijkstra_sum(0);
        std::atomic<uint64_t> profiling_loop_enq_slow_lin_sum(0);
        std::atomic<uint64_t> profiling_loop_deq_help_sum(0);
        std::atomic<uint64_t> profiling_loop_deq_fp_sum(0);
        std::atomic<uint64_t> profiling_loop_deq_sp_sum(0);
        std::atomic<uint64_t> profiling_num_cas_sum(0);
        std::atomic<uint64_t> profiling_num_cas_failed_sum(0);

        auto queue = queue_factory();
        __profiling_loop = true;
        parallelExecution(loopProfilingWorkload, queue,
                          &profiling_loop_deq_patience_sum,
                          &profiling_loop_enq_patience_sum,
                          &profiling_loop_enq_fp_sum,
                          &profiling_loop_enq_sp_sum,
                          &profiling_loop_enq_slow_dijkstra_sum,
                          &profiling_loop_enq_slow_lin_sum,
                          &profiling_loop_deq_help_sum,
                          &profiling_loop_deq_fp_sum,
                          &profiling_loop_deq_sp_sum,
                          &profiling_num_cas_sum,
                          &profiling_num_cas_failed_sum);
        __profiling_loop = false;

        // No doubles, because the numbersa are too big, use long and rounding.
        dequeuePatience = profiling_loop_deq_patience_sum;
        enqueuePatience =  profiling_loop_enq_patience_sum;
        enqueueFastPath = profiling_loop_enq_fp_sum;
        enqueueSlowPath = profiling_loop_enq_sp_sum;
        enqueueSlowDijkstra =  profiling_loop_enq_slow_dijkstra_sum;
        enqueueSlowLinerization =  profiling_loop_enq_slow_lin_sum;
        dequeueHelper = profiling_loop_deq_help_sum;
        dequeueFastPath = profiling_loop_deq_fp_sum;
        dequeueSlowPath = profiling_loop_deq_sp_sum;
        num_cas_sum = profiling_num_cas_sum;
        num_cas_failed_sum = profiling_num_cas_failed_sum;
    }
    std::cout << "BENCH" << SEP << queue_name << SEP << initial_queue_size << SEP << enqueuer_threads << SEP
              << num_threads << SEP << num_objects << SEP << enqueueLatency << SEP << dequeueLatency << SEP
              << enqueueThroughput << SEP << dequeueThroughput << SEP << patience << SEP
              << enqueueFastPath << SEP << enqueueSlowPath << SEP << dequeueFastPath << SEP << dequeueSlowPath << SEP
              << enqueuePatience << SEP << dequeuePatience << SEP << enqueueSlowDijkstra << SEP
              << enqueueSlowLinerization << SEP << dequeueHelper << SEP << num_cas_sum << SEP << num_cas_failed_sum
              << std::endl;

}

void run_benchmark_test() {
    if (include_header) {
        system_clock::time_point p = system_clock::now();
        std::time_t t = system_clock::to_time_t(p);
        std::cout << "Benchmark @ " << std::ctime(&t);
        std::cout << "TEST" << SEP << "QUEUE" << SEP << "INITIAL_QUEUE_SIZE" << SEP << "ENQ_THREADS" << SEP
                  << "NUM_THREADS" << SEP << "NUM_OBJECTS" << SEP
                  << "ENQ_LAT[ns]" << SEP << "DEQ_LAT[ns]" << SEP
                  << "ENQ_THROUGH[MOPS]" << SEP << "DEQ_THROUGH[MOPS]" << SEP
                  << "WFQ_MAX_PATIENCE" << SEP << "WFQ_ENQ_FP" << SEP << "WFQ_ENQ_SP" << SEP
                  << "WFQ_DEQ_FP" << SEP << "WFQ_DEQ_SP" << SEP << "WFQ_ENQ_PAT" << SEP
                  << "WFQ_DEQ_PAT" << SEP << "WFQ_ENQ_SLOW_DIJKSTRA" << SEP << "WFQ_ENQ_SLOW_LIN" << SEP
                  << "WFQ_DEQ_HELPER" << SEP << "WFQ_NUM_CAS" << SEP << "WFQ_NUM_CAS_FAILED" << std::endl;
    }
    for (long test = 0; test < test_repetitions; test++) {
        if (test_globallock) {
            auto queue_factory = []() { return new Queue::GlobalLockQueue(); };
            run_benchmark_test_internal(queue_factory, "GLQ");
        }
        if (test_lockfree) {
            auto queue_factory = []() { return new Queue::LockFreeQueue(); };
            run_benchmark_test_internal(queue_factory, "LFQ");
        }
        if (test_waitfree) {
            auto queue_factory = []() { return new Queue::WaitFreeQueue(num_threads, patience); };
            run_benchmark_test_internal(queue_factory, "WFQ");
        }
    }
}
//endregion

// this function calls the correctness test for each specified queue
//region Correctness test
// the workload for each thread that is executing the correctness test
void
correctness_workload(long tid, std::atomic<long> *atomic_barrier, Queue::FifoQueue *queue, std::atomic<long> *counter,
                     std::atomic<long> *elementSum, std::atomic<long> *fetchedElements,
                     std::atomic<long> *nonMonotonicElements) {
    threadBarrier(atomic_barrier);

    if (tid == 0) {
        // only one thread is enqueuing values for all other threads to maintain order in the queue ...
        for (long i = 0; i < num_objects; i++) {
            queue->enqueue((void *) counter->fetch_add(1), tid);
        }
    }

    // all the threads are dequeuing the values from the queue
    long value = 0;
    long oldValue = -1;
    while (true) {
        void *elem = queue->dequeue(tid);
        if (elem != nullptr) {
            value = (long) elem;
            if (oldValue >= value) {
                nonMonotonicElements->fetch_add(1);
            }
            oldValue = value;
            elementSum->fetch_add(value);
            fetchedElements->fetch_add(1);
        }
        if (fetchedElements->load() == num_objects) {
            break;
        }
    }
}

// this function starts the correctness test and spawns the threads
void correctness_test(Queue::FifoQueue *queue, const std::string &queueName) {
    std::atomic<long> counter = 2;
    auto elementSum = std::atomic<long>(0);
    auto fetchedElements = std::atomic<long>(0);
    auto nonMonotonicElements = std::atomic<long>(0);
    parallelExecution(correctness_workload, queue, &counter, &elementSum, &fetchedElements, &nonMonotonicElements);
    long expectedMaximumElement = num_objects + 1;
    long expectedElementSum = (expectedMaximumElement * expectedMaximumElement + expectedMaximumElement) / 2 - 1;
    std::cout << "CORR" << SEP << queueName << SEP << num_threads << SEP << num_objects << SEP;
    if (elementSum == expectedElementSum && nonMonotonicElements == 0) {
        std::cout << "YES";
    } else {
        std::cout << "NO";
    }
    std::cout << SEP << elementSum << SEP << expectedElementSum << SEP << nonMonotonicElements << std::endl;
}

void run_correctness_test() {
    if (include_header) {
        system_clock::time_point p = system_clock::now();
        std::time_t t = system_clock::to_time_t(p);
        std::cout << "Correctness test @ " << std::ctime(&t);
        std::cout << "TEST" << SEP << "QUEUE" << SEP << "NUM_THREADS" << SEP << "NUM_OBJECTS" << SEP << "CORR?" << SEP
                  << "ELEMENT_SUM" << SEP << "EXPECTED_ELEMENT_SUM" << SEP << "NON_MONOTONIC_ELEMENTS" << std::endl;
    }
    for (long test = 0; test < test_repetitions; test++) {
        if (test_globallock) {
            auto queue = new Queue::GlobalLockQueue();
            correctness_test(queue, "GLQ");
            delete queue;
        }
        if (test_lockfree) {
            auto queue = new Queue::GlobalLockQueue();
            correctness_test(queue, "LFQ");
            delete queue;
        }
        if (test_waitfree) {
            auto queue = new Queue::WaitFreeQueue(num_threads, patience);
            correctness_test(queue, "WFQ");
            delete queue;
        }
    }
}
//endregion

// check the passed cmdline parameters
//region Command line argument checking
void check_parameters() {
    if (num_threads < 1) {
        std::cerr << "Illegal number of threads: " << num_threads << std::endl;
        exit(EXIT_FAILURE);
    }
    if (num_objects < 1) {
        std::cerr << "Illegal number of objects: " << num_objects << std::endl;
        exit(EXIT_FAILURE);
    }
    if (enqueuer_threads < 0 || enqueuer_threads > num_threads) {
        std::cerr << "Illegal enqueuer threads " << enqueuer_threads << std::endl;
        exit(EXIT_FAILURE);
    }
    if (!test_globallock && !test_lockfree && !test_waitfree) {
        std::cerr << "No queues to test selected. Aborting. Try the -h option." << std::endl;
        exit(EXIT_FAILURE);
    }
}

// parse the passed cmdline options and fill the parameters for the benchmark test or the correctness test
void parse_cmdline_options(int argc, char *argv[]) {
    try {
        cxxopts::Options options(argv[0], "AMP Group 13 - FAA Queue");
        options.add_options()
                ("t,num_threads", "Number of threads that are executing in parallel",
                 cxxopts::value<long>(num_threads)->default_value(std::to_string(DEFAULT_NUM_THREADS)))
                ("o,num_objects", "Number of objects that are queued up or dequeued in total during each benchmark",
                 cxxopts::value<long>(num_objects)->default_value(std::to_string(DEFAULT_NUM_OBJECTS)))
                ("i,initial_queue_size", "Number of objects that are queued up before the benchmark is started",
                 cxxopts::value<long>(initial_queue_size)->default_value(std::to_string(DEFAULT_INITIAL_QUEUE_SIZE)))
                ("e,enqueuer_threads", "Number of enqueuer threads",
                 cxxopts::value<long>(enqueuer_threads)->default_value(
                         std::to_string(DEFAULT_ENQUEUER_THREADS)))
                ("r,test_repetitions", "Number of test run repetitions",
                 cxxopts::value<long>(test_repetitions)->default_value(std::to_string(DEFAULT_TEST_REPETITIONS)))
                ("a,patience", "Sets the patience for WaitFreeQueue enqueue & dequeue operations",
                 cxxopts::value<int>(patience)->default_value(std::to_string(DEFAULT_PATIENCE)))
                ("w,test_waitfree", "Pass option to test the WaitFreeQueue")
                ("l,test_lockfree", "Pass option to test the LockFreeQueue")
                ("g,test_globallock", "Pass option to test the GlobalLockQueue")
                ("c,is_correctness_test", "Runs the correctness test instead of the benchmark")
                ("v,include_header", "Enables the header in the console output")
                ("h,help", "Prints the help");
        auto result = options.parse(argc, argv);
        if (result.count("help")) {
            std::cout << options.help({""}) << std::endl;
            exit(EXIT_SUCCESS);
        }
        if (result.count("test_waitfree") > 0) test_waitfree = true;
        if (result.count("test_lockfree") > 0) test_lockfree = true;
        if (result.count("test_globallock") > 0) test_globallock = true;
        if (result.count("is_correctness_test") > 0) is_correctness_test = true;
        if (result.count("include_header") > 0) include_header = true;
    } catch (const cxxopts::OptionException &e) {
        std::cerr << "Error while parsing options: " << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }
}
//endregion

int main(int argc, char *argv[]) {
    parse_cmdline_options(argc, argv);
    check_parameters();
    if (is_correctness_test) {
        run_correctness_test();
    } else {
        run_benchmark_test();
    }
    return EXIT_SUCCESS;
}
