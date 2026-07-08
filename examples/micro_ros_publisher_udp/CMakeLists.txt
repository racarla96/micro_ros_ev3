cmake_minimum_required(VERSION 3.7)
project(micro_ros_publisher_udp C)

set(MICROROS_DIR "${CMAKE_SOURCE_DIR}/../../third_party/microros" CACHE PATH "Path to micro-ROS src/ directory (contains headers and ev3/libmicroros.a)")
if(NOT EXISTS "${MICROROS_DIR}/ev3/libmicroros.a")
    message(FATAL_ERROR "libmicroros.a not found in ${MICROROS_DIR} — vendor it under third_party/microros or pass -DMICROROS_DIR=<path> (see README)")
endif()

set(TRANSPORT_DIR "${CMAKE_SOURCE_DIR}/../../transport")

add_executable(micro_ros_publisher_udp
    main.c
    ${TRANSPORT_DIR}/udp_transport.c
    ${TRANSPORT_DIR}/time_compat.c
)

target_include_directories(micro_ros_publisher_udp PRIVATE ${MICROROS_DIR})

target_link_libraries(micro_ros_publisher_udp
    ${MICROROS_DIR}/ev3/libmicroros.a
    m pthread
)
