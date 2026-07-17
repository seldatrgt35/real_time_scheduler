if(NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "RTS_BUILD_S32K148_SMOKE requires an ARM bare-metal toolchain file")
endif()
if(NOT DEFINED RTS_S32K148_DEVICE_INCLUDE_DIR)
    message(FATAL_ERROR
        "RTS_S32K148_DEVICE_INCLUDE_DIR must contain NXP S32K148.h and CMSIS core headers")
endif()
if(NOT EXISTS "${RTS_S32K148_DEVICE_INCLUDE_DIR}/S32K148.h")
    message(FATAL_ERROR
        "${RTS_S32K148_DEVICE_INCLUDE_DIR}/S32K148.h was not found")
endif()

set(RTS_S32K148_LINKER_SCRIPT
    "${CMAKE_CURRENT_SOURCE_DIR}/linker/s32k148_flash.ld" CACHE FILEPATH
    "S32K148 flash linker script")

set(RTS_S32K148_KERNEL_SOURCES
    ${PROJECT_SOURCE_DIR}/kernel/intrusive_list.c
    ${PROJECT_SOURCE_DIR}/kernel/ready_queue.c
    ${PROJECT_SOURCE_DIR}/kernel/delay_queue.c
    ${PROJECT_SOURCE_DIR}/kernel/task_pool.c
    ${PROJECT_SOURCE_DIR}/kernel/task_validation.c
    ${PROJECT_SOURCE_DIR}/kernel/task_object.c
    ${PROJECT_SOURCE_DIR}/kernel/task_handle.c
    ${PROJECT_SOURCE_DIR}/kernel/task_create.c
    ${PROJECT_SOURCE_DIR}/kernel/kernel_state.c
    ${PROJECT_SOURCE_DIR}/kernel/cpu_local.c
    ${PROJECT_SOURCE_DIR}/kernel/scheduler_init.c
    ${PROJECT_SOURCE_DIR}/kernel/scheduler_select.c
    ${PROJECT_SOURCE_DIR}/kernel/switch_plan.c
    ${PROJECT_SOURCE_DIR}/kernel/task_yield.c
    ${PROJECT_SOURCE_DIR}/kernel/task_delay.c
    ${PROJECT_SOURCE_DIR}/kernel/wait_object.c
    ${PROJECT_SOURCE_DIR}/kernel/semaphore.c
    ${PROJECT_SOURCE_DIR}/kernel/priority_inheritance.c
    ${PROJECT_SOURCE_DIR}/kernel/mutex.c
    ${PROJECT_SOURCE_DIR}/kernel/timer.c
    ${PROJECT_SOURCE_DIR}/kernel/timer_queue.c
    ${PROJECT_SOURCE_DIR}/kernel/timer_callback_queue.c
    ${PROJECT_SOURCE_DIR}/kernel/timer_service.c
    ${PROJECT_SOURCE_DIR}/kernel/fatal.c
    ${PROJECT_SOURCE_DIR}/kernel/stack_check.c
    ${PROJECT_SOURCE_DIR}/kernel/runtime_stats.c
    ${PROJECT_SOURCE_DIR}/kernel/trace.c
    ${PROJECT_SOURCE_DIR}/kernel/invariant_check.c
    ${PROJECT_SOURCE_DIR}/kernel/scheduler_start.c
    ${PROJECT_SOURCE_DIR}/kernel/time.c
    ${PROJECT_SOURCE_DIR}/kernel/power.c
    ${PROJECT_SOURCE_DIR}/kernel/scheduler_policy.c
    ${PROJECT_SOURCE_DIR}/kernel/policy_fixed_priority.c
    ${PROJECT_SOURCE_DIR}/kernel/policy_rms.c
    ${PROJECT_SOURCE_DIR}/kernel/policy_edf.c
)

add_library(rts_target_s32k148 STATIC
    target.c
    target_diagnostics.c
    target_port.c
    target_power.c
    target_runtime.c
    target_tick.c
    target_tick_math.c
    startup/target_handlers.S
)
target_include_directories(rts_target_s32k148 PRIVATE
    ${RTS_S32K148_DEVICE_INCLUDE_DIR}
    ${RTS_CONFIG_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/kernel
    ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(rts_target_s32k148 PRIVATE
    RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
    RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
    RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
)

add_executable(rts_example_s32k148_basic
    ${RTS_S32K148_KERNEL_SOURCES}
    ${PROJECT_SOURCE_DIR}/examples/s32k148_basic/main.c
    ${PROJECT_SOURCE_DIR}/examples/s32k148_basic/register_test.S
    startup/startup_s32k148.S
)
set_target_properties(rts_example_s32k148_basic PROPERTIES
    OUTPUT_NAME rts_s32k148_smoke
    SUFFIX .elf
)
target_include_directories(rts_example_s32k148_basic PRIVATE
    ${RTS_S32K148_DEVICE_INCLUDE_DIR}
    ${RTS_CONFIG_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/kernel
    ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
    ${PROJECT_SOURCE_DIR}/examples/s32k148_basic
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(rts_example_s32k148_basic PRIVATE
    RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
    RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
    RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
)

foreach(target rts_target_s32k148 rts_example_s32k148_basic)
    target_compile_options(${target} PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic
    )
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target} PRIVATE -mgeneral-regs-only)
    elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(${target} PRIVATE -mfpu=none)
    endif()
endforeach()

target_link_libraries(rts_example_s32k148_basic PRIVATE
    rts_target_s32k148
    rts_cortex_m4f_port
)
target_link_options(rts_example_s32k148_basic PRIVATE
    -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
    -nostdlib
    -Wl,--gc-sections
    -Wl,-Map,$<TARGET_FILE_DIR:rts_example_s32k148_basic>/rts_s32k148_smoke.map
    -T${RTS_S32K148_LINKER_SCRIPT}
)

find_program(RTS_S32K148_OBJDUMP NAMES arm-none-eabi-objdump llvm-objdump REQUIRED)
find_program(RTS_S32K148_NM NAMES arm-none-eabi-nm llvm-nm REQUIRED)
find_program(RTS_S32K148_SIZE NAMES arm-none-eabi-size llvm-size REQUIRED)
add_custom_command(TARGET rts_example_s32k148_basic POST_BUILD
    COMMAND ${CMAKE_COMMAND}
            -DELF_FILE=$<TARGET_FILE:rts_example_s32k148_basic>
            -DOUTPUT_DIR=$<TARGET_FILE_DIR:rts_example_s32k148_basic>
            -DNM_TOOL=${RTS_S32K148_NM}
            -DOBJDUMP_TOOL=${RTS_S32K148_OBJDUMP}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/verify_image.cmake
    COMMAND ${CMAKE_COMMAND}
            -DELF_FILE=$<TARGET_FILE:rts_example_s32k148_basic>
            -DOUTPUT_DIR=$<TARGET_FILE_DIR:rts_example_s32k148_basic>
            -DSIZE_TOOL=${RTS_S32K148_SIZE}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/report_sizes.cmake
    VERBATIM
)

add_custom_target(rts_target_s32k148_smoke
    DEPENDS rts_example_s32k148_basic)
