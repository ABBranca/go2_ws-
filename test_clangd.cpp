#include <rclcpp/rclcpp.hpp>
class TestNode : public rclcpp::Node {
public:
  TestNode() : Node("test") {}
  void test() {
    static rclcpp::Time last_logged(0, 0, (*this->get_clock()).get_clock_type());
  }
};
