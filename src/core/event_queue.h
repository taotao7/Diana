#pragma once

#include <deque>
#include <mutex>
#include <condition_variable>
#include <optional>

namespace diana {

template<typename T>
class EventQueue {
public:
    void push(T event) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(std::move(event));
        cv_.notify_one();
    }

    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        T event = std::move(queue_.front());
        queue_.pop_front();
        return event;
    }

    T wait_pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty(); });
        T event = std::move(queue_.front());
        queue_.pop_front();
        return event;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<T> queue_;
};

}
