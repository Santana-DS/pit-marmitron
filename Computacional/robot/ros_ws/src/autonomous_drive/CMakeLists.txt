cmake_minimum_required(VERSION 3.5)
project(autonomous_drive)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(gazebo_msgs REQUIRED)

add_executable(vel_rodas src/calculo_vel_rodas.cpp)
add_executable(mux src/mux2.cpp)
add_executable(pos_gps src/pos_to_lat_sim.cpp)
add_executable(ground_truth_publisher src/gps.cpp)

ament_target_dependencies(ground_truth_publisher
  rclcpp
  gazebo_msgs
  geometry_msgs
  nav_msgs
)

ament_target_dependencies(vel_rodas
  rclcpp
  geometry_msgs
  std_msgs
)

ament_target_dependencies(mux
  rclcpp
  geometry_msgs
  std_msgs
)

ament_target_dependencies(pos_gps
  rclcpp
  nav_msgs
  sensor_msgs
)

install(TARGETS 
  ground_truth_publisher
  mux
  vel_rodas
  pos_gps
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY
  launch
  configs
  description
  worlds
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
