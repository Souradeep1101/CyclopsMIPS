#pragma once
#include <atomic>
#include <cstddef>
#include <optional>
#include <vector>

namespace MIPS {

// Lock-Free Single-Producer / Single-Consumer Queue
// Suitable for passing MMIO events (like keyboard input)
// from the UI thread to the CPU emulator thread asynchronously.
template <typename T>
class SPSCQueue {
public:
    explicit SPSCQueue(size_t capacity) : buffer(capacity + 1) {}

    bool push(const T& item) {
        size_t current_tail = tail.load(std::memory_order_relaxed);
        size_t next_tail = increment(current_tail);

        if (next_tail == head.load(std::memory_order_acquire)) {
            return false; // Queue is full
        }

        buffer[current_tail] = item;
        tail.store(next_tail, std::memory_order_release);
        return true;
    }

    std::optional<T> pop() {
        size_t current_head = head.load(std::memory_order_relaxed);

        if (current_head == tail.load(std::memory_order_acquire)) {
            return std::nullopt; // Queue is empty
        }

        T item = buffer[current_head];
        head.store(increment(current_head), std::memory_order_release);
        return item;
    }

    bool empty() const {
        return head.load(std::memory_order_acquire) == tail.load(std::memory_order_acquire);
    }

private:
    size_t increment(size_t idx) const {
        return (idx + 1) % buffer.size();
    }

    std::vector<T> buffer;
    // Align caches to prevent false sharing between threads
    alignas(64) std::atomic<size_t> head{0};
    alignas(64) std::atomic<size_t> tail{0};
};

} // namespace MIPS
