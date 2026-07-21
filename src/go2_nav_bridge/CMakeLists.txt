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
find_package(nav_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(unitree_go REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)

# Add executable
add_executable(bridge_node
  src/bridge_node.cpp
  src/bridge_utils.cpp
)

target_include_directories(bridge_node PRIVATE include)
ament_target_dependencies(bridge_node rclcpp geometry_msgs unitree_go)

add_executable(map_odom_broadcaster src/map_odom_broadcaster.cpp)
ament_target_dependencies(map_odom_broadcaster
  rclcpp
  geometry_msgs
  tf2_ros
  tf2_geometry_msgs
)

add_executable(odom_tf_broadcaster src/odom_tf_broadcaster.cpp)
ament_target_dependencies(odom_tf_broadcaster
  rclcpp
  nav_msgs
  sensor_msgs
  geometry_msgs
  tf2
  tf2_ros
)

add_executable(scan_preprocessor src/scan_preprocessor.cpp)
ament_target_dependencies(scan_preprocessor
  rclcpp
  sensor_msgs
)

# Install executable
install(TARGETS
  bridge_node
  map_odom_broadcaster
  odom_tf_broadcaster
  scan_preprocessor
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
