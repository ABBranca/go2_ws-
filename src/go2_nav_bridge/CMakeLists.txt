cmake_minimum_required(VERSION 3.8)
project(go2_nav_bridge)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wdouble-promotion)
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-D_FORTIFY_SOURCE=2)
  endif()
  add_link_options(-Wl,-z,relro -Wl,-z,now)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(unitree_go REQUIRED)

# Add executable
add_executable(bridge_node
  src/bridge_node.cpp
  src/bridge_utils.cpp
)

target_include_directories(bridge_node PRIVATE include)
ament_target_dependencies(bridge_node rclcpp geometry_msgs unitree_go)

# Install executable
install(TARGETS
  bridge_node
  DESTINATION lib/${PROJECT_NAME}
)

# Install launch files and config
install(DIRECTORY
  launch
  config
  DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()

  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_bridge_utils test/test_bridge_utils.cpp src/bridge_utils.cpp)
  target_include_directories(test_bridge_utils PRIVATE include)
endif()

ament_package()
