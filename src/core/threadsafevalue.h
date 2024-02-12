//
//  threadsafevaluecache.h
//

#pragma once

#include <mutex>

// Helper class for for sharing a value type between threads.
// It allows many threads to get or set a value atomically.

template <typename T>
class ThreadSafeValue
{
public:
    ThreadSafeValue() {}
    ThreadSafeValue(const T& v) : _value { v } {}

    // returns copy
    T Get() const
    {
        std::lock_guard<std::mutex> guard(_mutex);
        return _value;
    }

    // atomic set
    void Set(const T& v)
    {
        std::lock_guard<std::mutex> guard(_mutex);
        _value = v;
    }

    using WithLockCallbackConst = std::function<void(const T&)>;
    using WithLockCallback = std::function<void(T&)>;
    void WithLock(WithLockCallbackConst cb) const
    {
        std::lock_guard<std::mutex> guard(_mutex);
        cb(_value);
    }

    void WithLock(WithLockCallback cb)
    {
        std::lock_guard<std::mutex> guard(_mutex);
        cb(_value);
    }

private:
    mutable std::mutex _mutex;
    T _value;

    // no copies
    ThreadSafeValue(const ThreadSafeValue&) = delete;
    ThreadSafeValue& operator=(const ThreadSafeValue&) = delete;
};

