#ifndef AGILINK_OMNIHAND_ROS2_DEMO_UTILS_HPP_
#define AGILINK_OMNIHAND_ROS2_DEMO_UTILS_HPP_

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "rcl_interfaces/msg/parameter_type.hpp"
#include "rcl_interfaces/srv/get_parameters.hpp"
#include "rclcpp/rclcpp.hpp"

#include "std_msgs/msg/int16_multi_array.hpp"

namespace agilink {
namespace omnihand {
namespace ros2 {

inline std_msgs::msg::Int16MultiArray PackInt16MultiArray1D(const std::vector<int16_t>& data) {
  std_msgs::msg::Int16MultiArray out;
  out.layout.data_offset = 0;
  if (!data.empty()) {
    std_msgs::msg::MultiArrayDimension dim;
    dim.label = "joints";
    dim.size = static_cast<uint32_t>(data.size());
    dim.stride = static_cast<uint32_t>(data.size());
    out.layout.dim.push_back(dim);
  }
  out.data = data;
  return out;
}

inline int GetJointCount(const std::string& product, const std::string& side) {
  if (product == "o12") {
    return 12;
  }
  if (product == "h3l") {
    return 4;
  }
  if (product != "h3u_m") {
    return 10;
  }
  auto node = std::make_shared<rclcpp::Node>("h3um_demo_joint_info");
  const std::string topic = "/" + product + "/" + side + "/joint_states";
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(5);
  std::vector<rclcpp::TopicEndpointInfo> endpoints;
  while (rclcpp::ok() && std::chrono::steady_clock::now() < deadline) {
    endpoints = node->get_publishers_info_by_topic(topic);
    if (!endpoints.empty()) {
      break;
    }
    auto event = node->get_graph_event();
    node->wait_for_graph_change(event, std::chrono::milliseconds(50));
  }
  if (endpoints.size() != 1) {
    throw std::runtime_error("Expected one publisher for " + topic +
                             "; found " + std::to_string(endpoints.size()));
  }
  const auto& endpoint = endpoints.front();
  const std::string owner =
      endpoint.node_namespace() +
      (endpoint.node_namespace() == "/" ? "" : "/") + endpoint.node_name();
  auto client = node->create_client<rcl_interfaces::srv::GetParameters>(
      owner + "/get_parameters");
  if (!client->wait_for_service(std::max(
          std::chrono::steady_clock::duration::zero(),
          deadline - std::chrono::steady_clock::now()))) {
    throw std::runtime_error("Timed out waiting for " + owner +
                             "/get_parameters");
  }
  auto request =
      std::make_shared<rcl_interfaces::srv::GetParameters::Request>();
  request->names = {"joint_names"};
  auto future = client->async_send_request(request);
  if (rclcpp::spin_until_future_complete(
          node, future,
          std::max(std::chrono::steady_clock::duration::zero(),
                   deadline - std::chrono::steady_clock::now())) !=
      rclcpp::FutureReturnCode::SUCCESS) {
    throw std::runtime_error("Timed out reading joint_names from " + owner);
  }
  const auto response = future.get();
  if (response->values.size() != 1 ||
      response->values.front().type !=
          rcl_interfaces::msg::ParameterType::PARAMETER_STRING_ARRAY) {
    throw std::runtime_error(
        owner + " does not provide joint_names as a string array");
  }
  const auto& names = response->values.front().string_array_value;
  const std::set<std::string> unique_names(names.begin(), names.end());
  if ((names.size() != 20 && names.size() != 21) ||
      unique_names.size() != names.size() || unique_names.count("") != 0) {
    throw std::runtime_error(owner + " returned invalid H3UM joint_names");
  }
  return static_cast<int>(names.size());
}

}  // namespace ros2
}  // namespace omnihand
}  // namespace agilink

#endif  // AGILINK_OMNIHAND_ROS2_DEMO_UTILS_HPP_
