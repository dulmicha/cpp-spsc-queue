#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "spsc_queue.hpp"

namespace spsc::test {

// Helper: move-only type with multi-arg constructor
struct MoveOnlyWidget {
    int id;
    std::string label;

    MoveOnlyWidget(int id, std::string label) : id(id), label(std::move(label)) {}

    MoveOnlyWidget(const MoveOnlyWidget&) = delete;
    MoveOnlyWidget& operator=(const MoveOnlyWidget&) = delete;

    MoveOnlyWidget(MoveOnlyWidget&&) = default;
    MoveOnlyWidget& operator=(MoveOnlyWidget&&) = default;
};

// Helper: destructor-counting type
static int g_dtor_count = 0;

struct DtorCounter {
    DtorCounter() = default;
    DtorCounter(const DtorCounter&) = default;
    DtorCounter& operator=(const DtorCounter&) = default;
    DtorCounter(DtorCounter&&) = default;
    DtorCounter& operator=(DtorCounter&&) = default;
    ~DtorCounter() { ++g_dtor_count; }
};

// Basic primitive operations
TEST(SPSCQueueTest, BasicPushPop) {
    SPSCQueue<int> q(4);

    int val = 0;
    EXPECT_FALSE(q.try_pop(val));

    EXPECT_TRUE(q.try_push(42));
    EXPECT_TRUE(q.try_pop(val));
    EXPECT_EQ(val, 42);
}

TEST(SPSCQueueTest, PushPopMultipleItems) {
    SPSCQueue<int> q(8);

    for (int i = 0; i < 8; ++i) {
        EXPECT_TRUE(q.try_push(i));
    }

    for (int i = 0; i < 8; ++i) {
        int val = -1;
        EXPECT_TRUE(q.try_pop(val));
        EXPECT_EQ(val, i);
    }
}

TEST(SPSCQueueTest, FIFOOrdering) {
    SPSCQueue<int> q(16);

    EXPECT_TRUE(q.try_push(10));
    EXPECT_TRUE(q.try_push(20));
    EXPECT_TRUE(q.try_push(30));

    int val = 0;
    EXPECT_TRUE(q.try_pop(val));
    EXPECT_EQ(val, 10);
    EXPECT_TRUE(q.try_pop(val));
    EXPECT_EQ(val, 20);
    EXPECT_TRUE(q.try_pop(val));
    EXPECT_EQ(val, 30);
}

// Capacity & state queries
TEST(SPSCQueueTest, PowerOfTwoCapacityRounding) {
    // Exact powers of two stay the same
    EXPECT_EQ(SPSCQueue<int>(1).capacity(), 1u);
    EXPECT_EQ(SPSCQueue<int>(2).capacity(), 2u);
    EXPECT_EQ(SPSCQueue<int>(4).capacity(), 4u);
    EXPECT_EQ(SPSCQueue<int>(16).capacity(), 16u);

    // Non-powers are rounded up
    EXPECT_EQ(SPSCQueue<int>(3).capacity(), 4u);
    EXPECT_EQ(SPSCQueue<int>(5).capacity(), 8u);
    EXPECT_EQ(SPSCQueue<int>(10).capacity(), 16u);
    EXPECT_EQ(SPSCQueue<int>(100).capacity(), 128u);
}

TEST(SPSCQueueTest, EmptyAndSizeOnConstruction) {
    SPSCQueue<int> q(8);
    EXPECT_TRUE(q.empty());
    EXPECT_FALSE(q.full());
    EXPECT_EQ(q.size(), 0u);
}

TEST(SPSCQueueTest, FullState) {
    SPSCQueue<int> q(4);
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(q.try_push(i));
    }
    EXPECT_TRUE(q.full());
    EXPECT_FALSE(q.empty());
    EXPECT_EQ(q.size(), 4u);

    // Pushing into a full queue must fail
    EXPECT_FALSE(q.try_push(99));
}

TEST(SPSCQueueTest, EmptyState) {
    SPSCQueue<int> q(4);

    int val = -1;
    EXPECT_FALSE(q.try_pop(val));
    EXPECT_EQ(val, -1); // val should not be modified on failure
}

TEST(SPSCQueueTest, SizeTracking) {
    SPSCQueue<int> q(8);

    EXPECT_EQ(q.size(), 0u);
    q.try_push(1);
    EXPECT_EQ(q.size(), 1u);
    q.try_push(2);
    EXPECT_EQ(q.size(), 2u);

    int val = 0;
    q.try_pop(val);
    EXPECT_EQ(q.size(), 1u);
    q.try_pop(val);
    EXPECT_EQ(q.size(), 0u);
}

// Ring buffer wrap-around
TEST(SPSCQueueTest, RingBufferWrapAround) {
    // Use a tiny capacity to force many wrap-arounds
    SPSCQueue<int> q(4);
    constexpr int kIterations = 100'000;

    for (int i = 0; i < kIterations; ++i) {
        ASSERT_TRUE(q.try_push(i));
        int val = -1;
        ASSERT_TRUE(q.try_pop(val));
        ASSERT_EQ(val, i);
    }
}

// Move-only type support
TEST(SPSCQueueTest, MoveOnlyUniquePtr) {
    SPSCQueue<std::unique_ptr<int>> q(4);

    auto p = std::make_unique<int>(42);
    EXPECT_TRUE(q.try_push(std::move(p)));
    EXPECT_EQ(p, nullptr); // Moved from

    std::unique_ptr<int> out;
    EXPECT_TRUE(q.try_pop(out));
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(*out, 42);
}

TEST(SPSCQueueTest, MoveOnlyWidgetPushPop) {
    SPSCQueue<MoveOnlyWidget> q(4);

    MoveOnlyWidget w(1, "hello");
    EXPECT_TRUE(q.try_push(std::move(w)));

    MoveOnlyWidget out(0, "");
    EXPECT_TRUE(q.try_pop(out));
    EXPECT_EQ(out.id, 1);
    EXPECT_EQ(out.label, "hello");
}

// Emplace construction
TEST(SPSCQueueTest, EmplaceConstruction) {
    SPSCQueue<MoveOnlyWidget> q(8);

    EXPECT_TRUE(q.emplace(42, "widget-42"));
    EXPECT_TRUE(q.emplace(99, "widget-99"));

    MoveOnlyWidget out(0, "");
    EXPECT_TRUE(q.try_pop(out));
    EXPECT_EQ(out.id, 42);
    EXPECT_EQ(out.label, "widget-42");

    EXPECT_TRUE(q.try_pop(out));
    EXPECT_EQ(out.id, 99);
    EXPECT_EQ(out.label, "widget-99");
}

TEST(SPSCQueueTest, EmplaceFullQueue) {
    SPSCQueue<MoveOnlyWidget> q(2);

    EXPECT_TRUE(q.emplace(1, "a"));
    EXPECT_TRUE(q.emplace(2, "b"));
    EXPECT_FALSE(q.emplace(3, "c")); // Queue is full
}

// Destructor correctness
TEST(SPSCQueueTest, DestructorCleansUpRemainingElements) {
    g_dtor_count = 0;

    {
        SPSCQueue<DtorCounter> q(8);
        q.try_push(DtorCounter{});
        q.try_push(DtorCounter{});
        q.try_push(DtorCounter{});
        // Pop one inside a nested scope so `tmp` is destroyed before our reset
        {
            DtorCounter tmp;
            q.try_pop(tmp);
        }
        // Reset the counter
        // We only care about destructions triggered by the queue destructor (2 remaining)
        g_dtor_count = 0;
    }
    // The queue destructor should have destroyed the 2 remaining elements
    EXPECT_EQ(g_dtor_count, 2);
}

// Edge cases
TEST(SPSCQueueTest, CapacityOne) {
    SPSCQueue<int> q(1);
    EXPECT_EQ(q.capacity(), 1u);

    EXPECT_TRUE(q.try_push(7));
    EXPECT_TRUE(q.full());
    EXPECT_FALSE(q.try_push(8));

    int val = 0;
    EXPECT_TRUE(q.try_pop(val));
    EXPECT_EQ(val, 7);
    EXPECT_TRUE(q.empty());
}

TEST(SPSCQueueTest, AlternatingPushPop) {
    SPSCQueue<int> q(2);
    for (int i = 0; i < 1000; ++i) {
        ASSERT_TRUE(q.try_push(i));
        int val = -1;
        ASSERT_TRUE(q.try_pop(val));
        ASSERT_EQ(val, i);
    }
}

TEST(SPSCQueueTest, FillDrainCycles) {
    SPSCQueue<int> q(4);

    for (int cycle = 0; cycle < 100; ++cycle) {
        for (int i = 0; i < 4; ++i) {
            ASSERT_TRUE(q.try_push(cycle * 4 + i));
        }
        ASSERT_FALSE(q.try_push(-1));

        for (int i = 0; i < 4; ++i) {
            int val = -1;
            ASSERT_TRUE(q.try_pop(val));
            ASSERT_EQ(val, cycle * 4 + i);
        }
        ASSERT_TRUE(q.empty());
    }
}

} // namespace spsc::test
