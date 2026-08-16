#include <gtest/gtest.h>
#include "spsc_queue.hpp"

namespace spsc::test {

TEST(SPSCQueueStressTest, SkeletonConcurrentPlaceholderTest) {
    SPSCQueue<int> queue(100);
    EXPECT_TRUE(queue.empty());
}

} // namespace spsc::test
