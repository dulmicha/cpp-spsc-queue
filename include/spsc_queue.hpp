#ifndef SPSC_QUEUE_HPP
#define SPSC_QUEUE_HPP

#include <atomic>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace spsc {

namespace detail {

// Round up to the nearest power of two. Returns 1 for input 0 or 1.
constexpr std::size_t round_up_to_power_of_two(std::size_t n) noexcept {
  if (n <= 1)
    return 1;
  --n;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  if constexpr (sizeof(std::size_t) >= 8) {
    n |= n >> 32;
  }
  return n + 1;
}

// Cache line size for false-sharing prevention via alignment padding.
#ifdef __cpp_lib_hardware_interference_size
inline constexpr std::size_t cache_line_size =
    std::hardware_destructive_interference_size;
#else
inline constexpr std::size_t cache_line_size = 64;
#endif

} // namespace detail

/**
 * @brief Bounded lock-free Single-Producer Single-Consumer (SPSC) queue.
 *
 * Thread-safety contract:
 *   - Exactly ONE producer thread may call try_push / emplace.
 *   - Exactly ONE consumer thread may call try_pop.
 *   - empty(), full(), size(), capacity() may be called from any single thread
 *     but return an approximate snapshot under concurrent access.
 *
 * The requested capacity is rounded up to the next power of two so that index
 * wrapping uses a fast bitwise AND mask instead of modulo division.
 *
 * @tparam T Element type. Must be move-constructible and move-assignable.
 */
template <typename T> class SPSCQueue {
  static_assert(std::is_move_constructible_v<T>,
                "SPSCQueue requires T to be move-constructible");
  static_assert(std::is_move_assignable_v<T>,
                "SPSCQueue requires T to be move-assignable");

public:
  /**
   * @brief Construct a bounded SPSC queue.
   * @param capacity Requested maximum number of elements. Will be rounded up
   *                 to the next power of two.
   */
  explicit SPSCQueue(std::size_t capacity)
      : capacity_(detail::round_up_to_power_of_two(capacity)),
        mask_(capacity_ - 1),
        buffer_(static_cast<T *>(::operator new(capacity_ * sizeof(T)))) {}

  // Destroy the queue, invoking destructors on any elements still enqueued.
  ~SPSCQueue() {
    auto read = read_idx_.load(std::memory_order_relaxed);
    const auto write = write_idx_.load(std::memory_order_relaxed);
    while (read != write) {
      std::destroy_at(&buffer_[read & mask_]);
      ++read;
    }
    ::operator delete(buffer_);
  }

  // Non-copyable, non-movable
  SPSCQueue(const SPSCQueue &) = delete;
  SPSCQueue &operator=(const SPSCQueue &) = delete;
  SPSCQueue(SPSCQueue &&) = delete;
  SPSCQueue &operator=(SPSCQueue &&) = delete;

  /**
   * @brief Try to push an item by copy.
   * @param item Item to copy into the queue.
   * @return true if enqueued, false if the queue is full.
   *
   * Producer thread only.
   */
  bool try_push(const T &item) {
    const auto write_idx = write_idx_.load(std::memory_order_relaxed);

    if (write_idx - cached_read_idx_ >= capacity_) {
      // Cached consumer position is stale; reload with acquire barrier
      // to observe the consumer's latest releases.
      cached_read_idx_ = read_idx_.load(std::memory_order_acquire);
      if (write_idx - cached_read_idx_ >= capacity_) {
        return false; // Queue is truly full.
      }
    }

    // Construct element in-place in uninitialized storage.
    ::new (static_cast<void *>(&buffer_[write_idx & mask_])) T(item);

    // Release barrier: the constructed element is visible to the consumer
    // before the updated write index.
    write_idx_.store(write_idx + 1, std::memory_order_release);
    return true;
  }

  /**
   * @brief Try to push an item by move.
   * @param item Item to move into the queue.
   * @return true if enqueued, false if the queue is full.
   *
   * Producer thread only.
   */
  bool try_push(T &&item) {
    const auto write_idx = write_idx_.load(std::memory_order_relaxed);

    if (write_idx - cached_read_idx_ >= capacity_) {
      cached_read_idx_ = read_idx_.load(std::memory_order_acquire);
      if (write_idx - cached_read_idx_ >= capacity_) {
        return false;
      }
    }

    ::new (static_cast<void *>(&buffer_[write_idx & mask_])) T(std::move(item));

    write_idx_.store(write_idx + 1, std::memory_order_release);
    return true;
  }

  /**
   * @brief Try to emplace-construct an item directly in the queue.
   * @tparam Args Constructor argument types.
   * @param args  Arguments forwarded to T's constructor.
   * @return true if enqueued, false if the queue is full.
   *
   * Producer thread only.
   */
  template <typename... Args> bool emplace(Args &&...args) {
    const auto write_idx = write_idx_.load(std::memory_order_relaxed);

    if (write_idx - cached_read_idx_ >= capacity_) {
      cached_read_idx_ = read_idx_.load(std::memory_order_acquire);
      if (write_idx - cached_read_idx_ >= capacity_) {
        return false;
      }
    }

    ::new (static_cast<void *>(&buffer_[write_idx & mask_]))
        T(std::forward<Args>(args)...);

    write_idx_.store(write_idx + 1, std::memory_order_release);
    return true;
  }

  /**
   * @brief Try to pop an item from the queue.
   * @param[out] item Destination; receives the moved-out element on success.
   * @return true if an element was dequeued, false if the queue is empty.
   *
   * Consumer thread only.
   */
  bool try_pop(T &item) {
    const auto read_idx = read_idx_.load(std::memory_order_relaxed);

    if (read_idx == cached_write_idx_) {
      // Cached producer position is stale; reload with acquire barrier
      // to observe the producer's latest releases.
      cached_write_idx_ = write_idx_.load(std::memory_order_acquire);
      if (read_idx == cached_write_idx_) {
        return false; // Queue is truly empty.
      }
    }

    // Move element out, then destroy the (moved-from) source in the slot.
    item = std::move(buffer_[read_idx & mask_]);
    std::destroy_at(&buffer_[read_idx & mask_]);

    // Release barrier: the freed slot is visible to the producer before the
    // updated read index.
    read_idx_.store(read_idx + 1, std::memory_order_release);
    return true;
  }

  /**
   * @brief Snapshot: is the queue empty?
   * @note Approximate under concurrent access (racy observation).
   */
  bool empty() const noexcept {
    return write_idx_.load(std::memory_order_acquire) ==
           read_idx_.load(std::memory_order_acquire);
  }

  /**
   * @brief Snapshot: is the queue full?
   * @note Approximate under concurrent access (racy observation).
   */
  bool full() const noexcept {
    return write_idx_.load(std::memory_order_acquire) -
               read_idx_.load(std::memory_order_acquire) >=
           capacity_;
  }

  /**
   * @brief Snapshot: approximate number of elements in the queue.
   * @note Approximate under concurrent access (racy observation).
   */
  std::size_t size() const noexcept {
    return write_idx_.load(std::memory_order_acquire) -
           read_idx_.load(std::memory_order_acquire);
  }

  /**
   * @brief Returns the actual capacity (always a power of two).
   */
  std::size_t capacity() const noexcept { return capacity_; }

private:
  // Producer-side cache line.
  // Written only by the producer thread.
  alignas(detail::cache_line_size) std::atomic<std::size_t> write_idx_{0};
  std::size_t cached_read_idx_{0};

  // Consumer-side cache line.
  //   Written only by the consumer thread.
  alignas(detail::cache_line_size) std::atomic<std::size_t> read_idx_{0};
  std::size_t cached_write_idx_{0};

  // Shared metadata (read-only after construction)
  alignas(detail::cache_line_size) const std::size_t capacity_;
  const std::size_t mask_;
  T *const buffer_;
};

} // namespace spsc

#endif // SPSC_QUEUE_HPP
