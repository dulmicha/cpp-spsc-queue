#ifndef SPSC_QUEUE_HPP
#define SPSC_QUEUE_HPP

#include <cstddef>

namespace spsc {

/**
 * @brief Bounded Single-Producer Single-Consumer Queue.
 * 
 * @tparam T The element type stored in the queue.
 */
template <typename T>
class SPSCQueue {
public:
    /**
     * @brief Constructs an SPSCQueue with a fixed capacity.
     * @param capacity Maximum number of elements the queue can hold.
     */
    explicit SPSCQueue(std::size_t capacity);

    ~SPSCQueue();

    // Disable copy semantics
    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;

    // Move semantics declaration
    SPSCQueue(SPSCQueue&&) noexcept = default;
    SPSCQueue& operator=(SPSCQueue&&) noexcept = default;

    /**
     * @brief Attempts to push an item into the queue (lvalue version).
     * @param item Item to be pushed.
     * @return true if successfully pushed, false if queue is full.
     */
    bool try_push(const T& item);

    /**
     * @brief Attempts to push an item into the queue (rvalue version).
     * @param item Item to be pushed.
     * @return true if successfully pushed, false if queue is full.
     */
    bool try_push(T&& item);

    /**
     * @brief Attempts to pop an item from the queue.
     * @param item Reference where the popped item will be assigned/moved.
     * @return true if successfully popped, false if queue is empty.
     */
    bool try_pop(T& item);

    /**
     * @brief Checks whether the queue is empty.
     * @return true if empty, false otherwise.
     */
    bool empty() const noexcept;

    /**
     * @brief Checks whether the queue is full.
     * @return true if full, false otherwise.
     */
    bool full() const noexcept;

    /**
     * @brief Returns current number of elements in the queue.
     * @return std::size_t Element count.
     */
    std::size_t size() const noexcept;

    /**
     * @brief Returns fixed capacity of the queue.
     * @return std::size_t Maximum capacity.
     */
    std::size_t capacity() const noexcept;
};

// Stub implementations for skeleton compilation
template <typename T>
SPSCQueue<T>::SPSCQueue(std::size_t capacity) {
    (void)capacity;
}

template <typename T>
SPSCQueue<T>::~SPSCQueue() = default;

template <typename T>
bool SPSCQueue<T>::try_push(const T& item) {
    (void)item;
    return false;
}

template <typename T>
bool SPSCQueue<T>::try_push(T&& item) {
    (void)item;
    return false;
}

template <typename T>
bool SPSCQueue<T>::try_pop(T& item) {
    (void)item;
    return false;
}

template <typename T>
bool SPSCQueue<T>::empty() const noexcept {
    return true;
}

template <typename T>
bool SPSCQueue<T>::full() const noexcept {
    return false;
}

template <typename T>
std::size_t SPSCQueue<T>::size() const noexcept {
    return 0;
}

template <typename T>
std::size_t SPSCQueue<T>::capacity() const noexcept {
    return 0;
}

} // namespace spsc

#endif // SPSC_QUEUE_HPP
