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

option(RTS_BUILD_S32K148_SMOKE "Build the S32K148 cooperative smoke image" OFF)
if(RTS_BUILD_S32K148_SMOKE)
    set(RTS_BUILD_CORTEX_M4F_PORT ON CACHE BOOL
        "Build the Cortex-M4F execution port" FORCE)
    set(RTS_CONFIG_INCLUDE_DIR
        "${PROJECT_SOURCE_DIR}/targets/nxp/s32k148/config" CACHE PATH
        "Directory containing the selected rts_config.h" FORCE)
    set(RTS_CORTEX_M_NVIC_PRIORITY_BITS 4 CACHE STRING
        "Implemented S32K148 NVIC priority bits" FORCE)
    set(RTS_CORTEX_M_PENDSV_PRIORITY 15 CACHE STRING
        "Logical PendSV priority" FORCE)
    set(RTS_CORTEX_M_SVC_PRIORITY 13 CACHE STRING
        "Logical SVC priority" FORCE)
endif()

option(RTS_BUILD_CORTEX_M4F_PORT "Build the Cortex-M4F execution port" OFF)
if(RTS_BUILD_CORTEX_M4F_PORT)
    enable_language(ASM)
    add_subdirectory(ports/arm/cortex_m4f)
endif()

if(RTS_BUILD_S32K148_SMOKE)
    add_subdirectory(targets/nxp/s32k148)
endif()
