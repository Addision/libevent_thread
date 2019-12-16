#pragma once
#include <mutex>
#include <condition_variable>

typedef std::mutex Mutex;
typedef std::recursive_mutex RecursiveMutex;
typedef std::lock_guard<RecursiveMutex> RecursiveLock;
typedef std::lock_guard<Mutex> GuardLock;

typedef std::unique_lock<Mutex> UniqueLock;
typedef std::condition_variable Condition;