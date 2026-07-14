cmake_minimum_required(VERSION 3.20)

project(real_time_scheduler LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(RTS_BUILD_HOST_TESTS "Build deterministic host-port tests" ON)
if(RTS_BUILD_HOST_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

option(RTS_BUILD_CORTEX_M4F_PORT "Build the Cortex-M4F execution port" OFF)
if(RTS_BUILD_CORTEX_M4F_PORT)
    add_subdirectory(ports/arm/cortex_m4f)
endif()
