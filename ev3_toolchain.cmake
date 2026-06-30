set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_CROSSCOMPILING 1)

set(CMAKE_C_COMPILER arm-linux-gnueabi-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabi-g++)

set(CMAKE_C_FLAGS   "-march=armv5te -mtune=arm926ej-s -mfloat-abi=soft" CACHE STRING "")
set(CMAKE_CXX_FLAGS "-march=armv5te -mtune=arm926ej-s -mfloat-abi=soft" CACHE STRING "")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
