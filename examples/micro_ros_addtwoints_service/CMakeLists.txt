cmake_minimum_required(VERSION 3.10)
project(micro_ros_addtwoints_service C)

set(MICROROS_DIR "" CACHE PATH "Path to micro-ROS src/ directory")
if(NOT MICROROS_DIR)
    message(FATAL_ERROR "Set -DMICROROS_DIR=<path/to/micro_ros_arduino/src>")
endif()

set(TRANSPORT_DIR "${CMAKE_SOURCE_DIR}/../../transport")

add_executable(micro_ros_addtwoints_service
    main.c
    ${TRANSPORT_DIR}/udp_transport.c
    ${TRANSPORT_DIR}/time_compat.c
)

target_include_directories(micro_ros_addtwoints_service PRIVATE ${MICROROS_DIR})

target_link_libraries(micro_ros_addtwoints_service
    ${MICROROS_DIR}/ev3/libmicroros.a
    m pthread
)
