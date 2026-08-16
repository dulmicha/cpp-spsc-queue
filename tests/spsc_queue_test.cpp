#include <gtest/gtest.h>
#include "spsc_queue.hpp"

namespace spsc::test {

TEST(SPSCQueueTest, SkeletonPlaceholderTest) {
    SPSCQueue<int> queue(10);
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.capacity(), 0u); // Placeholder returns 0 currently
}

} // namespace spsc::test
