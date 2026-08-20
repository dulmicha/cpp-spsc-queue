#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include "spsc_queue.hpp"

namespace spsc::test {

// Concurrent integer transfer
TEST(SPSCQueueStressTest, ConcurrentProducerConsumer) {
    constexpr std::size_t kNumItems = 1'000'000;
    constexpr std::size_t kQueueCapacity = 1024;

    SPSCQueue<std::uint64_t> q(kQueueCapacity);

    std::thread producer([&] {
        for (std::uint64_t i = 0; i < kNumItems; ++i) {
            while (!q.try_push(i)) {
                // Spin until a slot opens up
            }
        }
    });

    std::vector<std::uint64_t> received;
    received.reserve(kNumItems);

    std::thread consumer([&] {
        std::uint64_t val = 0;
        for (std::size_t i = 0; i < kNumItems; ++i) {
            while (!q.try_pop(val)) {
                // Spin until an item is available
            }
            received.push_back(val);
        }
    });

    producer.join();
    consumer.join();

    // Verify we got every item exactly once, in strict monotonic order
    ASSERT_EQ(received.size(), kNumItems);
    for (std::uint64_t i = 0; i < kNumItems; ++i) {
        ASSERT_EQ(received[i], i) << "Mismatch at index " << i;
    }

    EXPECT_TRUE(q.empty());
}

// Concurrent move-only type transfer
TEST(SPSCQueueStressTest, ConcurrentMoveOnlyTransfer) {
    constexpr std::size_t kNumItems = 100'000;
    constexpr std::size_t kQueueCapacity = 512;

    SPSCQueue<std::unique_ptr<std::uint64_t>> q(kQueueCapacity);

    std::thread producer([&] {
        for (std::uint64_t i = 0; i < kNumItems; ++i) {
            auto ptr = std::make_unique<std::uint64_t>(i);
            while (!q.try_push(std::move(ptr))) {
                // Spin until a slot opens up
            }
        }
    });

    std::vector<std::uint64_t> received;
    received.reserve(kNumItems);

    std::thread consumer([&] {
        std::unique_ptr<std::uint64_t> val;
        for (std::size_t i = 0; i < kNumItems; ++i) {
            while (!q.try_pop(val)) {
                // Spin until an item is available
            }
            ASSERT_NE(val, nullptr);
            received.push_back(*val);
        }
    });

    producer.join();
    consumer.join();

    // Verify complete ordered transfer with no data corruption
    ASSERT_EQ(received.size(), kNumItems);
    for (std::uint64_t i = 0; i < kNumItems; ++i) {
        ASSERT_EQ(received[i], i) << "Mismatch at index " << i;
    }

    EXPECT_TRUE(q.empty());
}

// Repeated fill-drain under contention
TEST(SPSCQueueStressTest, RepeatedFillDrainConcurrent) {
    constexpr std::size_t kQueueCapacity = 64;
    constexpr std::size_t kCycles = 10'000;
    constexpr std::size_t kTotalItems = kQueueCapacity * kCycles;

    SPSCQueue<std::uint64_t> q(kQueueCapacity);

    std::atomic<bool> producer_done{false};

    std::thread producer([&] {
        for (std::uint64_t i = 0; i < kTotalItems; ++i) {
            while (!q.try_push(i)) {
                // Spin
            }
        }
        producer_done.store(true, std::memory_order_release);
    });

    std::uint64_t total_sum = 0;
    std::size_t total_count = 0;

    std::thread consumer([&] {
        std::uint64_t val = 0;
        while (total_count < kTotalItems) {
            if (q.try_pop(val)) {
                total_sum += val;
                ++total_count;
            }
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(total_count, kTotalItems);

    // Verify sum: sum(0..n-1) = n*(n-1)/2
    const std::uint64_t expected_sum = kTotalItems * (kTotalItems - 1) / 2;
    EXPECT_EQ(total_sum, expected_sum);
}

} // namespace spsc::test
