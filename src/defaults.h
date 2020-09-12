#pragma once

#define SEP ","
#define DEFAULT_ENQUEUE_VALUE (void *) 0xFFFF

constexpr int DEFAULT_NUM_THREADS = 64;
constexpr int DEFAULT_NUM_OBJECTS = 10000000;
constexpr long DEFAULT_ENQUEUER_THREADS = 1;
constexpr int DEFAULT_TEST_REPETITIONS = 5;
constexpr int DEFAULT_PATIENCE = 16;
constexpr int DEFAULT_INITIAL_QUEUE_SIZE = 0;
constexpr bool DEFAULT_TEST_WAITFREE = false;
constexpr bool DEFAULT_TEST_LOCKFREE = false;
constexpr bool DEFAULT_TEST_GLOBALLOCK = false;
constexpr bool DEFAULT_IS_CORRECTNESS_TEST = false;
constexpr bool DEFAULT_INCLUDE_HEADER = false;
