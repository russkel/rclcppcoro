
#include <cassert>
#include <memory>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "example_interfaces/srv/add_two_ints.hpp"

#include "rclcppcoro/rclcppcoro.hpp"

#include <asyncio/event_loop.h>
#include <asyncio/schedule_task.h>
#include <asyncio/task.h>
#include <asyncio/sleep.h>
#include <cppcoro/generator.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/sync_wait.hpp>

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
{
  auto sub1 = rclcppcoro::AsyncSubscription<std_msgs::msg::String>(node, "topic", 10);

  while (true) {
    auto msg = co_await sub1.await_message();
    RCLCPP_INFO(node.get_logger(), "subscription: I heard '%s'", msg.data.c_str());
  }
}

asyncio::Task<example_interfaces::srv::AddTwoInts::Response> handle_request(
  // const std::shared_ptr<rmw_request_id_t> request_header,
  const example_interfaces::srv::AddTwoInts::Request request)
{
  // (void)request_header;
  // RCLCPP_INFO(
  //   g_node->get_logger(),
  //   "request: %" PRId64 " + %" PRId64, request->a, request->b);
  example_interfaces::srv::AddTwoInts::Response response;
  response.sum = request.a + request.b;

  co_return response;
}

asyncio::Task<> service_task(rclcpp::Node& node)
{
  // auto srv = node->create_service<example_interfaces::srv::AddTwoInts>("add_two_ints", handle_service);
  while (true) {
    // auto request = co_await srv->await_request();
    // asyncio::schedule_task(handle_request(request));
  }
}

// asyncio::Task<> example_generator(rclcpp::Node& node)
cppcoro::task<> example_generator(rclcpp::Node& node)
{
  while (true) {
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
