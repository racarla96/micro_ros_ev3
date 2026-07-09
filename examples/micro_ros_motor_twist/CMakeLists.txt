cmake_minimum_required(VERSION 3.7)
project(micro_ros_motor_twist C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MICROROS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/microros" CACHE PATH "Path to micro-ROS src/ directory (contains headers and ev3/libmicroros.a)")
if(NOT EXISTS "${MICROROS_DIR}/ev3/libmicroros.a")
    message(FATAL_ERROR "libmicroros.a not found in ${MICROROS_DIR} — vendor it under third_party/microros or pass -DMICROROS_DIR=<path> (see README)")
endif()

set(TRANSPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../transport")
set(EV3DEV_LANG_CPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/ev3dev-lang-cpp")

add_executable(micro_ros_motor_twist
    main.cpp
    ${EV3DEV_LANG_CPP_DIR}/ev3dev.cpp
    ${TRANSPORT_DIR}/udp_transport.c
    ${TRANSPORT_DIR}/time_compat.c
    ${TRANSPORT_DIR}/config.c
)

target_include_directories(micro_ros_motor_twist PRIVATE ${MICROROS_DIR} ${EV3DEV_LANG_CPP_DIR})

# EV3DEV_PLATFORM_EV3 selects the "ev3-ports:outX" address scheme in
# ev3dev.h; _GLIBCXX_USE_NANOSLEEP is needed for std::this_thread::sleep_for
# on this cross toolchain's libstdc++.
target_compile_definitions(micro_ros_motor_twist PRIVATE
    EV3DEV_PLATFORM_EV3
    _GLIBCXX_USE_NANOSLEEP
)

target_link_libraries(micro_ros_motor_twist
    ${MICROROS_DIR}/ev3/libmicroros.a
    m pthread
)
