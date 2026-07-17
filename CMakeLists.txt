cmake_minimum_required(VERSION 3.20)

project(real_time_scheduler VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(rts_public_api INTERFACE)
target_include_directories(rts_public_api INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_custom_target(rts_release_audit
        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/release_audit.py
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        VERBATIM)
endif()

find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    add_custom_target(rts_docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${PROJECT_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        VERBATIM)
endif()

option(RTS_BUILD_HOST_TESTS "Build deterministic host-port tests" ON)
if(RTS_BUILD_HOST_TESTS)
    option(RTS_HOST_FREESTANDING_TEST_RUNTIME
        "Link host tests without a platform C runtime" OFF)
    if(RTS_HOST_FREESTANDING_TEST_RUNTIME)
        add_library(rts_test_freestanding_runtime STATIC
            ${PROJECT_SOURCE_DIR}/tests/support/freestanding_memory.c)
        target_compile_options(rts_test_freestanding_runtime PRIVATE
            -ffreestanding -fno-builtin -fno-autolink
            -mno-stack-arg-probe)
        add_compile_options(-ffreestanding -fno-builtin -fno-autolink
                            -mno-stack-arg-probe)
        add_link_options(-nostdlib "-Wl,/entry:main,/subsystem:console")
        link_libraries(rts_test_freestanding_runtime)
    endif()
    enable_testing()
    add_subdirectory(tests)
endif()

option(RTS_BUILD_S32K148_SMOKE "Build the S32K148 cooperative smoke image" OFF)
if(RTS_BUILD_S32K148_SMOKE)
    set(RTS_S32K148_POLICY "FP" CACHE STRING
        "S32K148 scheduler policy: FP, RMS, or EDF")
    set_property(CACHE RTS_S32K148_POLICY PROPERTY STRINGS FP RMS EDF)
    if(RTS_S32K148_POLICY STREQUAL "FP")
        set(_rts_s32k148_config "config")
    elseif(RTS_S32K148_POLICY STREQUAL "RMS")
        set(_rts_s32k148_config "config_rms")
    elseif(RTS_S32K148_POLICY STREQUAL "EDF")
        set(_rts_s32k148_config "config_edf")
    else()
        message(FATAL_ERROR "RTS_S32K148_POLICY must be FP, RMS, or EDF")
    endif()
    set(RTS_BUILD_CORTEX_M4F_PORT ON CACHE BOOL
        "Build the Cortex-M4F execution port" FORCE)
    set(RTS_CONFIG_INCLUDE_DIR
        "${PROJECT_SOURCE_DIR}/targets/nxp/s32k148/${_rts_s32k148_config}" CACHE PATH
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
