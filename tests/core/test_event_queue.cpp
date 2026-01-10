#include <gtest/gtest.h>
#include "core/event_queue.h"
#include <thread>
#include <string>

TEST(EventQueueTest, PushAndTryPop) {
    diana::EventQueue<int> queue;
    
    queue.push(42);
    queue.push(100);
    
    auto val1 = queue.try_pop();
    ASSERT_TRUE(val1.has_value());
    EXPECT_EQ(val1.value(), 42);
    
    auto val2 = queue.try_pop();
    ASSERT_TRUE(val2.has_value());
    EXPECT_EQ(val2.value(), 100);
    
    auto val3 = queue.try_pop();
    EXPECT_FALSE(val3.has_value());
}

TEST(EventQueueTest, EmptyAndSize) {
    diana::EventQueue<std::string> queue;
    
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);
    
    queue.push("hello");
    EXPECT_FALSE(queue.empty());
    EXPECT_EQ(queue.size(), 1);
    
    queue.push("world");
    EXPECT_EQ(queue.size(), 2);
    
    queue.try_pop();
    EXPECT_EQ(queue.size(), 1);
}

TEST(EventQueueTest, Clear) {
    diana::EventQueue<int> queue;
    
    queue.push(1);
    queue.push(2);
    queue.push(3);
    
    EXPECT_EQ(queue.size(), 3);
    queue.clear();
    EXPECT_TRUE(queue.empty());
}

TEST(EventQueueTest, ThreadSafety) {
    diana::EventQueue<int> queue;
    const int num_items = 1000;
    
    std::thread producer([&queue, num_items]() {
        for (int i = 0; i < num_items; ++i) {
            queue.push(i);
        }
    });
    
    std::thread consumer([&queue, num_items]() {
        int count = 0;
        while (count < num_items) {
            auto val = queue.try_pop();
            if (val.has_value()) {
                ++count;
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    EXPECT_TRUE(queue.empty());
}

TEST(EventQueueTest, WaitPop) {
    diana::EventQueue<int> queue;
    
    std::thread producer([&queue]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        queue.push(999);
    });
    
    int val = queue.wait_pop();
    EXPECT_EQ(val, 999);
    
    producer.join();
}
