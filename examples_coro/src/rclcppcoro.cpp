
#include <cassert>
#include <memory>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

#include <cppcoro/generator.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/sync_wait.hpp>

cppcoro::task<std_msgs::msg::String> get_chatter(rclcpp::Node* node, rclcpp::WaitSet& wait_set, rclcpp::Subscription<std_msgs::msg::String>::SharedPtr sub)
{
  RCLCPP_INFO(node->get_logger(), "Waiting...");
  while (1) {
    auto wait_result = wait_set.wait(std::chrono::seconds(1));
    if (wait_result.kind() == rclcpp::WaitResultKind::Ready) {
      // size_t guard_conditions_num = wait_set.get_rcl_wait_set().size_of_guard_conditions;
      size_t subscriptions_num = wait_set.get_rcl_wait_set().size_of_subscriptions;

      for (size_t i = 0; i < subscriptions_num; i++) {
        // auto& sub = wait_result.get_wait_set().get_rcl_wait_set().subscriptions[i];
        if (sub) {
          RCLCPP_INFO(node->get_logger(), "subscription %zu triggered", i + 1);
          std_msgs::msg::String msg;
          rclcpp::MessageInfo msg_info;
          if (sub->take(msg, msg_info)) {
            co_return msg;
          } else {
            RCLCPP_INFO(node->get_logger(), "subscription %zu: No message", i + 1);
          }
        }
      }
    } else if (wait_result.kind() == rclcpp::WaitResultKind::Timeout) {
      RCLCPP_INFO(node->get_logger(), "wait-set waiting failed with timeout");
    } else if (wait_result.kind() == rclcpp::WaitResultKind::Empty) {
      RCLCPP_INFO(node->get_logger(), "wait-set waiting failed because wait-set is empty");
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
      size_t subscriptions_num = wait_set.get_rcl_wait_set().size_of_subscriptions;

      for (size_t i = 0; i < subscriptions_num; i++) {
        // auto& sub = wait_result.get_wait_set().get_rcl_wait_set().subscriptions[i];
        if (sub) {
          RCLCPP_INFO(node->get_logger(), "subscription %zu triggered", i + 1);
          std_msgs::msg::String msg;
          rclcpp::MessageInfo msg_info;
          if (sub->take(msg, msg_info)) {
            co_yield msg;
          } else {
            RCLCPP_INFO(node->get_logger(), "subscription %zu: No message", i + 1);
          }
        }
      }
    } else if (wait_result.kind() == rclcpp::WaitResultKind::Timeout) {
      RCLCPP_INFO(node->get_logger(), "wait-set waiting failed with timeout");
    } else if (wait_result.kind() == rclcpp::WaitResultKind::Empty) {
      RCLCPP_INFO(node->get_logger(), "wait-set waiting failed because wait-set is empty");
    }
  }
}

cppcoro::task<> usage_example(rclcpp::Node& node)
{
  // auto do_nothing = [](std_msgs::msg::String::UniquePtr) {assert(false);};
  // auto sub1 = node.create_subscription<std_msgs::msg::String>("topic", 10, do_nothing);
  // rclcpp::WaitSet wait_set({}, {});
  // wait_set.add_subscription(sub1);  // FIXME: add it in the ctor
  // wait_set.add_subscription(sub2);
  // wait_set.add_guard_condition(guard_condition2);
  // Calling function creates a new task but doesn't start
  // executing the coroutine yet.
  while (1) {
    // cppcoro::task<std_msgs::msg::String> chatter_task = get_chatter(&node, wait_set, sub1);

    // Coroutine is only started when we later co_await the task.
    // auto msg = co_await chatter_task;
    // RCLCPP_INFO(node.get_logger(), "subscription: I heard '%s'", msg.data.c_str());
    for (auto msg : chatter_generator(&node)) {
      RCLCPP_INFO(node.get_logger(), "subscription: I heard '%s'", msg.data.c_str());
    }
  }
}

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);

  auto node = std::make_shared<rclcpp::Node>("rclcppcoro_example_node");

  rclcpp::on_shutdown([]() {

  });

  RCLCPP_INFO(node->get_logger(), "Action: Nothing triggered");

  sync_wait(usage_example(*node));

  return 0;
}
