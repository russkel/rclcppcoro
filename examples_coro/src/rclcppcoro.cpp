
#include <cassert>
#include <memory>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

#include <asyncio/event_loop.h>
#include <asyncio/schedule_task.h>
#include <asyncio/task.h>
#include <asyncio/sleep.h>
#include <cppcoro/generator.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/sync_wait.hpp>

asyncio::Task<std_msgs::msg::String> get_chatter(rclcpp::Node* node, rclcpp::WaitSet& wait_set, rclcpp::Subscription<std_msgs::msg::String>::SharedPtr sub)
{
  while (1) {
    RCLCPP_DEBUG(node->get_logger(), "Waiting...");
    auto wait_result = wait_set.wait(std::chrono::milliseconds(0));
    if (wait_result.kind() == rclcpp::WaitResultKind::Ready) {
      // auto& sub = wait_result.get_wait_set().get_rcl_wait_set().subscriptions[i];
      if (sub) {
        RCLCPP_INFO(node->get_logger(), "subscription triggered");
        std_msgs::msg::String msg;
        rclcpp::MessageInfo msg_info;
        if (sub->take(msg, msg_info)) {
          co_return msg;
        } else {
          RCLCPP_ERROR(node->get_logger(), "subscription: No message");
        }
      }
    } else if (wait_result.kind() == rclcpp::WaitResultKind::Timeout) {
      RCLCPP_DEBUG(node->get_logger(), "wait-set waiting failed with timeout");
      co_await asyncio::sleep(std::chrono::milliseconds(10));
    } else if (wait_result.kind() == rclcpp::WaitResultKind::Empty) {
      RCLCPP_ERROR(node->get_logger(), "wait-set waiting failed because wait-set is empty");
    }
  }
}


cppcoro::generator<std_msgs::msg::String> chatter_generator(rclcpp::Node* node)
{
  auto do_nothing = [](std_msgs::msg::String::UniquePtr) {assert(false);};
  auto sub = node->create_subscription<std_msgs::msg::String>("topic", 10, do_nothing);
  rclcpp::WaitSet wait_set({}, {});
  wait_set.add_subscription(sub);  // FIXME: add it in the ctor
  RCLCPP_INFO(node->get_logger(), "Waiting...");

  while (true) {
    auto wait_result = wait_set.wait(std::chrono::seconds(1));
    if (wait_result.kind() == rclcpp::WaitResultKind::Ready) {
      if (sub) {
        // RCLCPP_INFO(node->get_logger(), "subscription %zu triggered", i + 1);
        std_msgs::msg::String msg;
        rclcpp::MessageInfo msg_info;
        if (sub->take(msg, msg_info)) {
          co_yield msg;
        } else {
          RCLCPP_INFO(node->get_logger(), "subscription: No message");
        }
      }
    } else if (wait_result.kind() == rclcpp::WaitResultKind::Timeout) {
      RCLCPP_INFO(node->get_logger(), "wait-set waiting failed with timeout");
    } else if (wait_result.kind() == rclcpp::WaitResultKind::Empty) {
      RCLCPP_INFO(node->get_logger(), "wait-set waiting failed because wait-set is empty");
    }
  }
}

asyncio::Task<> example_task(rclcpp::Node& node)
// cppcoro::task<> example_task(rclcpp::Node& node)
{
  auto do_nothing = [](std_msgs::msg::String::UniquePtr) {assert(false);};
  auto sub1 = node.create_subscription<std_msgs::msg::String>("topic", 10, do_nothing);
  rclcpp::WaitSet wait_set({}, {});
  wait_set.add_subscription(sub1);
  // Calling function creates a new task but doesn't start
  // executing the coroutine yet.
  while (1) {
    asyncio::Task<std_msgs::msg::String> chatter_task = get_chatter(&node, wait_set, sub1);
    // Coroutine is only started when we later co_await the task.
    auto msg = co_await chatter_task;
    RCLCPP_INFO(node.get_logger(), "subscription: I heard '%s'", msg.data.c_str());
  }
}


// asyncio::Task<> example_generator(rclcpp::Node& node)
cppcoro::task<> example_generator(rclcpp::Node& node)
{
  while (1) {
    for (auto msg : chatter_generator(&node)) {
      RCLCPP_INFO(node.get_logger(), "subscription: I heard '%s'", msg.data.c_str());
    }
  }
}

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);

  auto node = std::make_shared<rclcpp::Node>("rclcppcoro_example_node");

  auto task_handle = asyncio::schedule_task(example_task(*node));

  rclcpp::on_shutdown([&task_handle, &node]() {
    RCLCPP_INFO(node->get_logger(), "Cancelling main task");
    task_handle.cancel();
  });

  RCLCPP_INFO(node->get_logger(), "Running event loop...");

  // sync_wait(example_task(*node));
  // sync_wait(example_generator(*node));
  asyncio::get_event_loop().run_until_complete();

  // asyncio::run(the_task);
  // asyncio::run(example_generator(*node))

  return 0;
}
