
#include <cassert>
#include <memory>

#include "rclcpp/rclcpp.hpp"

#include <asyncio/event_loop.h>
#include <asyncio/schedule_task.h>
#include <asyncio/task.h>
#include <asyncio/sleep.h>
#include <cppcoro/generator.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/sync_wait.hpp>

namespace rclcppcoro
{
/*
template<typename ServiceT>
class AsyncService
{
public:
  using ServiceTaskType = std::function<
    void (
      const std::shared_ptr<typename ServiceT::Request>,
      std::shared_ptr<typename ServiceT::Response>)>;

  asyncio::Task<ServiceT::Request> await_request() {
    ServiceT::Request request;
    rmw_request_id_t request_id_out;
    srv->take_request(request, request_id_out);
    co_return request;
  }

  asyncio::Task<> service_task(rclcpp::Node& node)
  {
    auto srv_do_nothing = [](
      const std::shared_ptr<ServiceT::Request>,
      std::shared_ptr<ServiceT::Response>) {assert(false);};
    auto srv = node.create_service<ServiceT>("~/test", srv_do_nothing);

    rclcpp::WaitSet wait_set();
    wait_set.add_service(srv);
    //  set_on_new_request_callback(std::function<void(size_t)> callback)
    // single_consumer_async_auto_reset_event

    while (true) {
      // rmw_request_id_t & req_id
      ServiceT::Request request = co_await await_request();
      ServiceT::Response response = co_await do_service(request);
      srv->send_response(req_id, response);
    }
  }
}*/

template<typename SubscriptionT>
class AsyncSubscription
{
public:

  asyncio::Task<SubscriptionT> await_message()
  {
    while (true) {
      RCLCPP_DEBUG(node->get_logger(), "Waiting...");
      auto wait_result = wait_set.wait(std::chrono::milliseconds(0));
      if (wait_result.kind() == rclcpp::WaitResultKind::Ready) {
        if (sub) {
          RCLCPP_INFO(node->get_logger(), "subscription triggered");
          SubscriptionT msg;
          rclcpp::MessageInfo msg_info;
          if (sub->take(msg, msg_info)) {
            co_return msg;
          } else {
            RCLCPP_ERROR(node->get_logger(), "subscription: No message");
          }
        }
      } else if (wait_result.kind() == rclcpp::WaitResultKind::Timeout) {
        RCLCPP_DEBUG(node->get_logger(), "wait-set waiting failed with timeout");
      } else if (wait_result.kind() == rclcpp::WaitResultKind::Empty) {
        RCLCPP_ERROR(node->get_logger(), "wait-set waiting failed because wait-set is empty");
      }
      co_await asyncio::sleep(std::chrono::milliseconds(10));
    }
  }

  AsyncSubscription(rclcpp::Node& node,
      const std::string & topic_name,
      const rclcpp::QoS & qos
  ) : node(&node)
  {
    auto do_nothing = [](SubscriptionT::UniquePtr) {assert(false);};
    sub = node.create_subscription<SubscriptionT>(topic_name, qos, do_nothing);
    wait_set.add_subscription(sub);
  }

  rclcpp::Node* node;
  rclcpp::WaitSet wait_set;
  rclcpp::Subscription<SubscriptionT>::SharedPtr sub;
};

}  // namespace rclcppcoro

