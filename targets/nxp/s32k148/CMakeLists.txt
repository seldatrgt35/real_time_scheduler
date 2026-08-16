if(NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "RTS_BUILD_S32K148_SMOKE requires an ARM bare-metal toolchain file")
endif()

# Real FlexCAN ISR-to-task calibration. SDK sources stay external to this repo.
function(rts_add_flexcan_isr_target)
set(RTS_S32K148_SDK_ROOT "C:/NXP/S32_SDK_S32K1xx_RTM_4.0.2" CACHE PATH
    "NXP S32 SDK root used by the FlexCAN ISR latency calibration")
set(RTS_S32K148_SDK_FLEXCAN_BOARD
    "${RTS_S32K148_SDK_ROOT}/examples/S32K148/demo_apps/flexcan_encrypted/board")
if(EXISTS "${RTS_S32K148_SDK_ROOT}/platform/drivers/src/flexcan/flexcan_driver.c")
    add_executable(star_s32k148_flexcan_isr_latency
        ${RTS_S32K148_KERNEL_SOURCES}
        ${PROJECT_SOURCE_DIR}/examples/s32k148_flexcan_isr_latency/main.c
        ${PROJECT_SOURCE_DIR}/examples/s32k148_flexcan_isr_latency/osif_flexcan_stub.c
        ${RTS_S32K148_SDK_FLEXCAN_BOARD}/clock_config.c
        ${RTS_S32K148_SDK_FLEXCAN_BOARD}/peripherals_flexcan_config_1.c
        ${RTS_S32K148_SDK_ROOT}/platform/devices/startup.c
        ${RTS_S32K148_SDK_ROOT}/platform/devices/S32K148/startup/system_S32K148.c
        ${RTS_S32K148_SDK_ROOT}/platform/devices/S32K148/startup/gcc/startup_S32K148.S
        ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/clock/S32K1xx/clock_S32K1xx.c
        ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/interrupt/interrupt_manager.c
        ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/flexcan/flexcan_driver.c
        ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/flexcan/flexcan_hw_access.c
        ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/edma/edma_driver.c
        ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/edma/edma_hw_access.c)
    set_target_properties(star_s32k148_flexcan_isr_latency PROPERTIES
        OUTPUT_NAME star_s32k148_flexcan_isr_latency SUFFIX .elf)
    target_include_directories(star_s32k148_flexcan_isr_latency PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f ${CMAKE_CURRENT_SOURCE_DIR}
        ${RTS_S32K148_SDK_FLEXCAN_BOARD}
        ${RTS_S32K148_SDK_ROOT}/platform/devices/S32K148/include
        ${RTS_S32K148_SDK_ROOT}/platform/devices/S32K148
        ${RTS_S32K148_SDK_ROOT}/platform/devices/common
        ${RTS_S32K148_SDK_ROOT}/platform/devices
        ${RTS_S32K148_SDK_ROOT}/platform/drivers/inc
        ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/flexcan
        ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/edma
        ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/clock/S32K1xx
        ${RTS_S32K148_SDK_ROOT}/rtos/osif)
    target_compile_definitions(star_s32k148_flexcan_isr_latency PRIVATE
        CPU_S32K148 DEV_ERROR_DETECT
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
    target_compile_options(star_s32k148_flexcan_isr_latency PRIVATE
        -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections
        -fdata-sections -mgeneral-regs-only)
    target_link_libraries(star_s32k148_flexcan_isr_latency PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_flexcan_isr_latency PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -Wl,--gc-sections
        -Wl,--defsym,g_pfnVectors=__VECTOR_RAM
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_flexcan_isr_latency>/star_s32k148_flexcan_isr_latency.map
        -T${RTS_S32K148_SDK_ROOT}/platform/devices/S32K148/linker/gcc/S32K148_256_flash.ld
        -specs=nano.specs -specs=nosys.specs)
    add_custom_target(star_target_s32k148_flexcan_isr_latency
        DEPENDS star_s32k148_flexcan_isr_latency)

    set(STAR_TASK_CONTROLLER_ROOT
        "C:/Users/nxg19748/OneDrive - NXP/Desktop/real_time_scheduler_task_controller"
        CACHE PATH "STAR task-controller repository used by the full-stack demo")
    set(STAR_FULL_STACK_SOURCE
        "${STAR_TASK_CONTROLLER_ROOT}/task_admission_framework/examples/powertrain_full_stack_demo.c")
    if(EXISTS "${STAR_FULL_STACK_SOURCE}")
        add_executable(star_s32k148_powertrain_full_stack_demo
            ${PROJECT_SOURCE_DIR}/examples/s32k148_powertrain_full_stack_demo/main.c
            ${PROJECT_SOURCE_DIR}/examples/s32k148_flexcan_isr_latency/osif_flexcan_stub.c
            ${STAR_FULL_STACK_SOURCE}
            ${RTS_S32K148_SDK_FLEXCAN_BOARD}/clock_config.c
            ${RTS_S32K148_SDK_FLEXCAN_BOARD}/peripherals_flexcan_config_1.c
            ${RTS_S32K148_SDK_ROOT}/platform/devices/startup.c
            ${RTS_S32K148_SDK_ROOT}/platform/devices/S32K148/startup/system_S32K148.c
            ${RTS_S32K148_SDK_ROOT}/platform/devices/S32K148/startup/gcc/startup_S32K148.S
            ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/clock/S32K1xx/clock_S32K1xx.c
            ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/interrupt/interrupt_manager.c
            ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/flexcan/flexcan_driver.c
            ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/flexcan/flexcan_hw_access.c
            ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/edma/edma_driver.c
            ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/edma/edma_hw_access.c)
        set_target_properties(star_s32k148_powertrain_full_stack_demo PROPERTIES
            OUTPUT_NAME star_s32k148_powertrain_full_stack_demo SUFFIX .elf)
        target_include_directories(star_s32k148_powertrain_full_stack_demo PRIVATE
            ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR}
            ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/kernel
            ${CMAKE_CURRENT_SOURCE_DIR} ${RTS_S32K148_SDK_FLEXCAN_BOARD}
            ${STAR_TASK_CONTROLLER_ROOT}/task_admission_framework/examples
            ${RTS_S32K148_SDK_ROOT}/platform/devices/S32K148/include
            ${RTS_S32K148_SDK_ROOT}/platform/devices/S32K148
            ${RTS_S32K148_SDK_ROOT}/platform/devices/common
            ${RTS_S32K148_SDK_ROOT}/platform/devices
            ${RTS_S32K148_SDK_ROOT}/platform/drivers/inc
            ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/flexcan
            ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/edma
            ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/clock/S32K1xx
            ${RTS_S32K148_SDK_ROOT}/rtos/osif)
        target_compile_definitions(star_s32k148_powertrain_full_stack_demo PRIVATE
            CPU_S32K148 DEV_ERROR_DETECT)
        target_compile_options(star_s32k148_powertrain_full_stack_demo PRIVATE
            -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding
            -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables
            -ffunction-sections -fdata-sections -mgeneral-regs-only)
        target_link_libraries(star_s32k148_powertrain_full_stack_demo PRIVATE gcc)
        target_link_options(star_s32k148_powertrain_full_stack_demo PRIVATE
            -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -Wl,--gc-sections
            -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_powertrain_full_stack_demo>/star_s32k148_powertrain_full_stack_demo.map
            -T${RTS_S32K148_SDK_ROOT}/platform/devices/S32K148/linker/gcc/S32K148_256_flash.ld
            -specs=nano.specs -specs=nosys.specs)
        add_custom_target(star_target_s32k148_powertrain_full_stack_demo
            DEPENDS star_s32k148_powertrain_full_stack_demo)

        set(STAR_GITHUB_UDS_SOURCE
            "${STAR_TASK_CONTROLLER_ROOT}/task_admission_framework/examples/github_uds_ecu_blind_holdout.c")
        add_executable(star_s32k148_github_uds_blind_holdout
            ${PROJECT_SOURCE_DIR}/examples/s32k148_powertrain_full_stack_demo/main.c
            ${PROJECT_SOURCE_DIR}/examples/s32k148_flexcan_isr_latency/osif_flexcan_stub.c
            ${STAR_GITHUB_UDS_SOURCE}
            ${RTS_S32K148_SDK_FLEXCAN_BOARD}/clock_config.c
            ${RTS_S32K148_SDK_FLEXCAN_BOARD}/peripherals_flexcan_config_1.c
            ${RTS_S32K148_SDK_ROOT}/platform/devices/startup.c
            ${RTS_S32K148_SDK_ROOT}/platform/devices/S32K148/startup/system_S32K148.c
            ${RTS_S32K148_SDK_ROOT}/platform/devices/S32K148/startup/gcc/startup_S32K148.S
            ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/clock/S32K1xx/clock_S32K1xx.c
            ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/interrupt/interrupt_manager.c
            ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/flexcan/flexcan_driver.c
            ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/flexcan/flexcan_hw_access.c
            ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/edma/edma_driver.c
            ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/edma/edma_hw_access.c)
        set_target_properties(star_s32k148_github_uds_blind_holdout PROPERTIES
            OUTPUT_NAME star_s32k148_github_uds_blind_holdout SUFFIX .elf)
        target_include_directories(star_s32k148_github_uds_blind_holdout PRIVATE
            ${PROJECT_SOURCE_DIR}/examples/s32k148_github_uds_blind_holdout
            ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR}
            ${RTS_S32K148_SDK_FLEXCAN_BOARD}
            ${STAR_TASK_CONTROLLER_ROOT}/task_admission_framework/examples
            ${RTS_S32K148_SDK_ROOT}/platform/devices/S32K148/include
            ${RTS_S32K148_SDK_ROOT}/platform/devices/S32K148
            ${RTS_S32K148_SDK_ROOT}/platform/devices/common
            ${RTS_S32K148_SDK_ROOT}/platform/devices
            ${RTS_S32K148_SDK_ROOT}/platform/drivers/inc
            ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/flexcan
            ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/edma
            ${RTS_S32K148_SDK_ROOT}/platform/drivers/src/clock/S32K1xx
            ${RTS_S32K148_SDK_ROOT}/rtos/osif)
        target_compile_definitions(star_s32k148_github_uds_blind_holdout PRIVATE
            CPU_S32K148 DEV_ERROR_DETECT
            g_star_full_stack_complete=g_star_github_uds_complete
            g_star_full_stack_current=g_star_github_uds_current
            g_star_full_stack_results=g_star_github_uds_results)
        target_compile_options(star_s32k148_github_uds_blind_holdout PRIVATE
            -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding
            -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables
            -ffunction-sections -fdata-sections -mgeneral-regs-only)
        target_link_libraries(star_s32k148_github_uds_blind_holdout PRIVATE gcc)
        target_link_options(star_s32k148_github_uds_blind_holdout PRIVATE
            -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -Wl,--gc-sections
            -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_github_uds_blind_holdout>/star_s32k148_github_uds_blind_holdout.map
            -T${RTS_S32K148_SDK_ROOT}/platform/devices/S32K148/linker/gcc/S32K148_256_flash.ld
            -specs=nano.specs -specs=nosys.specs)
        add_custom_target(star_target_s32k148_github_uds_blind_holdout
            DEPENDS star_s32k148_github_uds_blind_holdout)
    endif()
endif()
endfunction()

if(NOT DEFINED RTS_S32K148_DEVICE_INCLUDE_DIR)
    message(FATAL_ERROR
        "RTS_S32K148_DEVICE_INCLUDE_DIR must contain NXP S32K148.h")
endif()
if(NOT EXISTS "${RTS_S32K148_DEVICE_INCLUDE_DIR}/S32K148.h")
    message(FATAL_ERROR
        "${RTS_S32K148_DEVICE_INCLUDE_DIR}/S32K148.h was not found")
endif()
if(NOT DEFINED RTS_S32K148_CMSIS_INCLUDE_DIR)
    message(FATAL_ERROR
        "RTS_S32K148_CMSIS_INCLUDE_DIR must contain CMSIS core_cm4.h")
endif()
if(NOT EXISTS "${RTS_S32K148_CMSIS_INCLUDE_DIR}/core_cm4.h")
    message(FATAL_ERROR
        "${RTS_S32K148_CMSIS_INCLUDE_DIR}/core_cm4.h was not found")
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
    target_led.c
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
    ${RTS_S32K148_CMSIS_INCLUDE_DIR}
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
    ${RTS_S32K148_CMSIS_INCLUDE_DIR}
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
    gcc
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
find_program(RTS_S32K148_OBJCOPY NAMES arm-none-eabi-objcopy llvm-objcopy REQUIRED)
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
    COMMAND ${RTS_S32K148_OBJCOPY} -O ihex
            $<TARGET_FILE:rts_example_s32k148_basic>
            $<TARGET_FILE_DIR:rts_example_s32k148_basic>/rts_s32k148_smoke.hex
    COMMAND ${RTS_S32K148_OBJCOPY} -O srec
            $<TARGET_FILE:rts_example_s32k148_basic>
            $<TARGET_FILE_DIR:rts_example_s32k148_basic>/rts_s32k148_smoke.srec
    VERBATIM
)

add_custom_target(rts_target_s32k148_smoke
    DEPENDS rts_example_s32k148_basic)

if(DEFINED STAR_BENCHMARK_ROOT)
    set(STAR_CALIBRATION_LEVEL "1" CACHE STRING
        "STAR automotive calibration level (1..15)")
    set(STAR_ACTIVE_TASK_INDEX "-1" CACHE STRING
        "STAR task index to run alone (-1 for all, otherwise 0..9)")
    set(STAR_RUNTIME_CALIBRATION "0" CACHE STRING
        "Use compact STAR runtime workload loop")
    set(STAR_RUNTIME_REPETITIONS "1" CACHE STRING
        "STAR runtime workload repetitions")

    file(GLOB STAR_BENCHMARK_TASK_SOURCES CONFIGURE_DEPENDS
        "${STAR_BENCHMARK_ROOT}/tasks/src/*.c")
    file(GLOB STAR_BENCHMARK_BOARD_SOURCES CONFIGURE_DEPENDS
        "${STAR_BENCHMARK_ROOT}/board/src/*.c")

    add_executable(star_s32k148_benchmark
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_TASK_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_scheduler_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_benchmark PROPERTIES
        OUTPUT_NAME star_s32k148_benchmark
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_benchmark PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_benchmark PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        STAR_CALIBRATION_LEVEL=${STAR_CALIBRATION_LEVEL}
        STAR_ACTIVE_TASK_INDEX=${STAR_ACTIVE_TASK_INDEX}
        STAR_RUNTIME_CALIBRATION=${STAR_RUNTIME_CALIBRATION}
        STAR_RUNTIME_REPETITIONS=${STAR_RUNTIME_REPETITIONS}
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_benchmark PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_benchmark PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_benchmark PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_benchmark>/star_s32k148_benchmark.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_benchmark
        DEPENDS star_s32k148_benchmark)

    add_executable(star_s32k148_baseline
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_TASK_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_baseline_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_baseline PROPERTIES
        OUTPUT_NAME star_s32k148_baseline
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_baseline PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_baseline PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        STAR_CALIBRATION_LEVEL=${STAR_CALIBRATION_LEVEL}
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_baseline PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_baseline PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_baseline PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_baseline>/star_s32k148_baseline.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_baseline
        DEPENDS star_s32k148_baseline)

    add_executable(star_s32k148_feature_calibration
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/feature_calibration_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_feature_calibration_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_feature_calibration PROPERTIES
        OUTPUT_NAME star_s32k148_feature_calibration
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_feature_calibration PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_feature_calibration PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_feature_calibration PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_feature_calibration PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_feature_calibration PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_feature_calibration>/star_s32k148_feature_calibration.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_feature_calibration
        DEPENDS star_s32k148_feature_calibration)

    add_executable(star_s32k148_automotive_model
        ${RTS_S32K148_KERNEL_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/automotive_mixed_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_automotive_model_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_automotive_model PROPERTIES
        OUTPUT_NAME star_s32k148_automotive_model
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_automotive_model PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
    )
    target_compile_definitions(star_s32k148_automotive_model PRIVATE
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_automotive_model PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_automotive_model PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_automotive_model PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_automotive_model>/star_s32k148_automotive_model.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_automotive_model
        DEPENDS star_s32k148_automotive_model)

    add_executable(star_s32k148_primitive_calibration
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/primitive_operation_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_primitive_calibration_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_primitive_calibration PROPERTIES
        OUTPUT_NAME star_s32k148_primitive_calibration
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_primitive_calibration PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_primitive_calibration PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_primitive_calibration PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_primitive_calibration PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_primitive_calibration PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_primitive_calibration>/star_s32k148_primitive_calibration.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_primitive_calibration
        DEPENDS star_s32k148_primitive_calibration)

    add_executable(star_s32k148_mixed_calibration
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/mixed_operation_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_mixed_calibration_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_mixed_calibration PROPERTIES
        OUTPUT_NAME star_s32k148_mixed_calibration
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_mixed_calibration PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_mixed_calibration PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_mixed_calibration PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_mixed_calibration PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_mixed_calibration PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_mixed_calibration>/star_s32k148_mixed_calibration.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_mixed_calibration
        DEPENDS star_s32k148_mixed_calibration)

    add_executable(star_s32k148_automotive_validation
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/automotive_mixed_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_automotive_validation_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_automotive_validation PROPERTIES
        OUTPUT_NAME star_s32k148_automotive_validation
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_automotive_validation PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_automotive_validation PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_automotive_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_automotive_validation PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_automotive_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_automotive_validation>/star_s32k148_automotive_validation.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_automotive_validation
        DEPENDS star_s32k148_automotive_validation)

    add_executable(star_s32k148_blind_validation
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/blind_sensor_fusion_task.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_blind_validation_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_blind_validation PROPERTIES
        OUTPUT_NAME star_s32k148_blind_validation
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_blind_validation PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_blind_validation PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_blind_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_blind_validation PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_blind_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_blind_validation>/star_s32k148_blind_validation.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_blind_validation
        DEPENDS star_s32k148_blind_validation)

    add_executable(star_s32k148_manager_demo
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/manager_demo_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_manager_demo_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_manager_demo PROPERTIES
        OUTPUT_NAME star_s32k148_manager_demo
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_manager_demo PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_manager_demo PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_manager_demo PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_manager_demo PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_manager_demo PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_manager_demo>/star_s32k148_manager_demo.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_manager_demo
        DEPENDS star_s32k148_manager_demo)

    add_executable(star_s32k148_sequential_memory
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/sequential_memory_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_sequential_memory_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_sequential_memory PROPERTIES
        OUTPUT_NAME star_s32k148_sequential_memory
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_sequential_memory PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_sequential_memory PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_sequential_memory PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_sequential_memory PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_sequential_memory PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_sequential_memory>/star_s32k148_sequential_memory.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_sequential_memory
        DEPENDS star_s32k148_sequential_memory)

    add_executable(star_s32k148_blind_memory_validation
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/blind_memory_automotive_task.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_blind_memory_validation_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_blind_memory_validation PROPERTIES
        OUTPUT_NAME star_s32k148_blind_memory_validation
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_blind_memory_validation PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_blind_memory_validation PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_blind_memory_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_blind_memory_validation PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_blind_memory_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_blind_memory_validation>/star_s32k148_blind_memory_validation.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_blind_memory_validation
        DEPENDS star_s32k148_blind_memory_validation)

    add_executable(star_s32k148_integer_arithmetic
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/integer_arithmetic_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_integer_arithmetic_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_integer_arithmetic PROPERTIES
        OUTPUT_NAME star_s32k148_integer_arithmetic
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_integer_arithmetic PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_integer_arithmetic PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_integer_arithmetic PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_integer_arithmetic PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_integer_arithmetic PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_integer_arithmetic>/star_s32k148_integer_arithmetic.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_integer_arithmetic
        DEPENDS star_s32k148_integer_arithmetic)

    add_executable(star_s32k148_blind_integer_validation
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/blind_integer_automotive_task.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_blind_integer_validation_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_blind_integer_validation PROPERTIES
        OUTPUT_NAME star_s32k148_blind_integer_validation
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_blind_integer_validation PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_blind_integer_validation PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_blind_integer_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_blind_integer_validation PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_blind_integer_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_blind_integer_validation>/star_s32k148_blind_integer_validation.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_blind_integer_validation
        DEPENDS star_s32k148_blind_integer_validation)

    add_executable(star_s32k148_branch_evaluation
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/branch_evaluation_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_branch_evaluation_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_branch_evaluation PROPERTIES
        OUTPUT_NAME star_s32k148_branch_evaluation
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_branch_evaluation PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_branch_evaluation PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_branch_evaluation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_branch_evaluation PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_branch_evaluation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_branch_evaluation>/star_s32k148_branch_evaluation.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_branch_evaluation
        DEPENDS star_s32k148_branch_evaluation)

    add_executable(star_s32k148_blind_branch_validation
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/blind_branch_automotive_task.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_blind_branch_validation_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_blind_branch_validation PROPERTIES
        OUTPUT_NAME star_s32k148_blind_branch_validation
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_blind_branch_validation PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_blind_branch_validation PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_blind_branch_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_blind_branch_validation PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_blind_branch_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_blind_branch_validation>/star_s32k148_blind_branch_validation.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_blind_branch_validation
        DEPENDS star_s32k148_blind_branch_validation)

    add_executable(star_s32k148_bitwise_operations
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/bitwise_operation_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_bitwise_operations_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_bitwise_operations PROPERTIES
        OUTPUT_NAME star_s32k148_bitwise_operations
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_bitwise_operations PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_bitwise_operations PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_bitwise_operations PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_bitwise_operations PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_bitwise_operations PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_bitwise_operations>/star_s32k148_bitwise_operations.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_bitwise_operations
        DEPENDS star_s32k148_bitwise_operations)

    add_executable(star_s32k148_blind_bitwise_validation
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/blind_branch_automotive_task.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_blind_bitwise_validation_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_blind_bitwise_validation PROPERTIES
        OUTPUT_NAME star_s32k148_blind_bitwise_validation
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_blind_bitwise_validation PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_blind_bitwise_validation PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_blind_bitwise_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_blind_bitwise_validation PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_blind_bitwise_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_blind_bitwise_validation>/star_s32k148_blind_bitwise_validation.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_blind_bitwise_validation
        DEPENDS star_s32k148_blind_bitwise_validation)

    add_executable(star_s32k148_random_indexed_memory
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/random_indexed_memory_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_random_indexed_memory_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_random_indexed_memory PROPERTIES
        OUTPUT_NAME star_s32k148_random_indexed_memory
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_random_indexed_memory PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_random_indexed_memory PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_random_indexed_memory PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_random_indexed_memory PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_random_indexed_memory PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_random_indexed_memory>/star_s32k148_random_indexed_memory.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_random_indexed_memory
        DEPENDS star_s32k148_random_indexed_memory)

    add_executable(star_s32k148_blind_random_memory_validation
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/blind_branch_automotive_task.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_blind_random_memory_validation_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_blind_random_memory_validation PROPERTIES
        OUTPUT_NAME star_s32k148_blind_random_memory_validation
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_blind_random_memory_validation PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_blind_random_memory_validation PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_blind_random_memory_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_blind_random_memory_validation PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_blind_random_memory_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_blind_random_memory_validation>/star_s32k148_blind_random_memory_validation.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_blind_random_memory_validation
        DEPENDS star_s32k148_blind_random_memory_validation)

    add_executable(star_s32k148_bulk_memory
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/bulk_memory_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_bulk_memory_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_bulk_memory PROPERTIES
        OUTPUT_NAME star_s32k148_bulk_memory
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_bulk_memory PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_bulk_memory PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_bulk_memory PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_bulk_memory PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_bulk_memory PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_bulk_memory>/star_s32k148_bulk_memory.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_bulk_memory
        DEPENDS star_s32k148_bulk_memory)

    add_executable(star_s32k148_blind_bulk_memory_validation
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/blind_branch_automotive_task.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_blind_bulk_memory_validation_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_blind_bulk_memory_validation PROPERTIES
        OUTPUT_NAME star_s32k148_blind_bulk_memory_validation
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_blind_bulk_memory_validation PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_blind_bulk_memory_validation PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_blind_bulk_memory_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_blind_bulk_memory_validation PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_blind_bulk_memory_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_blind_bulk_memory_validation>/star_s32k148_blind_bulk_memory_validation.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_blind_bulk_memory_validation
        DEPENDS star_s32k148_blind_bulk_memory_validation)

    add_executable(star_s32k148_floating_conversion
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/floating_conversion_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_floating_conversion_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_floating_conversion PROPERTIES
        OUTPUT_NAME star_s32k148_floating_conversion
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_floating_conversion PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_floating_conversion PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_floating_conversion PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_floating_conversion PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_floating_conversion PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_floating_conversion>/star_s32k148_floating_conversion.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_floating_conversion
        DEPENDS star_s32k148_floating_conversion)

    add_executable(star_s32k148_blind_floating_validation
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/blind_branch_automotive_task.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_blind_floating_validation_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_blind_floating_validation PROPERTIES
        OUTPUT_NAME star_s32k148_blind_floating_validation
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_blind_floating_validation PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_blind_floating_validation PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_blind_floating_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_blind_floating_validation PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_blind_floating_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_blind_floating_validation>/star_s32k148_blind_floating_validation.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_blind_floating_validation
        DEPENDS star_s32k148_blind_floating_validation)

    add_executable(star_s32k148_remaining_feature_calibration
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/remaining_feature_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_remaining_feature_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_remaining_feature_calibration PROPERTIES
        OUTPUT_NAME star_s32k148_remaining_feature_calibration
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_remaining_feature_calibration PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_remaining_feature_calibration PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_remaining_feature_calibration PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_remaining_feature_calibration PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port m gcc)
    target_link_options(star_s32k148_remaining_feature_calibration PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_remaining_feature_calibration>/star_s32k148_remaining_feature_calibration.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_remaining_feature_calibration
        DEPENDS star_s32k148_remaining_feature_calibration)

    add_executable(star_s32k148_final_automotive_validation
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/final_automotive_task.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_final_automotive_validation_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_final_automotive_validation PROPERTIES
        OUTPUT_NAME star_s32k148_final_automotive_validation
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_final_automotive_validation PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_final_automotive_validation PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_final_automotive_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_final_automotive_validation PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port m gcc)
    target_link_options(star_s32k148_final_automotive_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_final_automotive_validation>/star_s32k148_final_automotive_validation.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_final_automotive_validation
        DEPENDS star_s32k148_final_automotive_validation)

    add_executable(star_s32k148_gpio_access_calibration
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/gpio_access_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_gpio_access_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_gpio_access_calibration PROPERTIES
        OUTPUT_NAME star_s32k148_gpio_access_calibration
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_gpio_access_calibration PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_gpio_access_calibration PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_gpio_access_calibration PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_gpio_access_calibration PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_gpio_access_calibration PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_gpio_access_calibration>/star_s32k148_gpio_access_calibration.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_gpio_access_calibration
        DEPENDS star_s32k148_gpio_access_calibration)

    add_executable(star_s32k148_blind_gpio_validation
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/blind_gpio_automotive_task.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_blind_gpio_validation_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_blind_gpio_validation PROPERTIES
        OUTPUT_NAME star_s32k148_blind_gpio_validation
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_blind_gpio_validation PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_blind_gpio_validation PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_blind_gpio_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_blind_gpio_validation PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_blind_gpio_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_blind_gpio_validation>/star_s32k148_blind_gpio_validation.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_blind_gpio_validation
        DEPENDS star_s32k148_blind_gpio_validation)

    add_executable(star_s32k148_automotive_system_validation
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/automotive_system_validation_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_automotive_system_validation_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_automotive_system_validation PROPERTIES
        OUTPUT_NAME star_s32k148_automotive_system_validation
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_automotive_system_validation PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_automotive_system_validation PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_automotive_system_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_automotive_system_validation PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port m gcc)
    target_link_options(star_s32k148_automotive_system_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_automotive_system_validation>/star_s32k148_automotive_system_validation.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_automotive_system_validation
        DEPENDS star_s32k148_automotive_system_validation)

    add_executable(star_s32k148_basic_automotive_validation
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/basic_automotive_taskset.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_basic_automotive_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_basic_automotive_validation PROPERTIES
        OUTPUT_NAME star_s32k148_basic_automotive_validation
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_basic_automotive_validation PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_basic_automotive_validation PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_basic_automotive_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_basic_automotive_validation PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_basic_automotive_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_basic_automotive_validation>/star_s32k148_basic_automotive_validation.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_basic_automotive_validation
        DEPENDS star_s32k148_basic_automotive_validation)

    add_executable(star_s32k148_factorial_calibration
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/integer_arithmetic_tasks.c"
        "${STAR_BENCHMARK_ROOT}/calibration/branch_evaluation_tasks.c"
        "${STAR_BENCHMARK_ROOT}/calibration/bitwise_operation_tasks.c"
        "${STAR_BENCHMARK_ROOT}/calibration/sequential_memory_tasks.c"
        "${STAR_BENCHMARK_ROOT}/calibration/random_indexed_memory_tasks.c"
        "${STAR_BENCHMARK_ROOT}/calibration/bulk_memory_tasks.c"
        "${STAR_BENCHMARK_ROOT}/calibration/floating_conversion_tasks.c"
        "${STAR_BENCHMARK_ROOT}/calibration/remaining_feature_tasks.c"
        "${STAR_BENCHMARK_ROOT}/calibration/gpio_access_tasks.c"
        "${STAR_BENCHMARK_ROOT}/calibration/factorial_calibration_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_factorial_calibration_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_factorial_calibration PROPERTIES
        OUTPUT_NAME star_s32k148_factorial_calibration
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_factorial_calibration PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_factorial_calibration PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_factorial_calibration PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_factorial_calibration PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port m gcc)
    target_link_options(star_s32k148_factorial_calibration PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_factorial_calibration>/star_s32k148_factorial_calibration.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_factorial_calibration
        DEPENDS star_s32k148_factorial_calibration)

    add_executable(star_s32k148_memory_fixed_mixed_calibration
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/memory_fixed_mixed_calibration.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_memory_fixed_mixed_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_memory_fixed_mixed_calibration PROPERTIES
        OUTPUT_NAME star_s32k148_memory_fixed_mixed_calibration
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_memory_fixed_mixed_calibration PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_memory_fixed_mixed_calibration PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_memory_fixed_mixed_calibration PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_memory_fixed_mixed_calibration PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_memory_fixed_mixed_calibration PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_memory_fixed_mixed_calibration>/star_s32k148_memory_fixed_mixed_calibration.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_memory_fixed_mixed_calibration
        DEPENDS star_s32k148_memory_fixed_mixed_calibration)

    add_executable(star_s32k148_blind_automotive_memory_fixed
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/blind_automotive_memory_fixed_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_blind_automotive_memory_fixed_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_blind_automotive_memory_fixed PROPERTIES
        OUTPUT_NAME star_s32k148_blind_automotive_memory_fixed
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_blind_automotive_memory_fixed PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_blind_automotive_memory_fixed PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_blind_automotive_memory_fixed PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_blind_automotive_memory_fixed PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_blind_automotive_memory_fixed PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_blind_automotive_memory_fixed>/star_s32k148_blind_automotive_memory_fixed.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_blind_automotive_memory_fixed
        DEPENDS star_s32k148_blind_automotive_memory_fixed)

    add_executable(star_s32k148_general_c_operation_map
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/general_c_operation_map_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_general_c_operation_map_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_general_c_operation_map PROPERTIES
        OUTPUT_NAME star_s32k148_general_c_operation_map
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_general_c_operation_map PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_general_c_operation_map PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_general_c_operation_map PRIVATE
        -O0
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_general_c_operation_map PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_general_c_operation_map PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_general_c_operation_map>/star_s32k148_general_c_operation_map.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_general_c_operation_map
        DEPENDS star_s32k148_general_c_operation_map)

    add_executable(star_s32k148_atomic_integer
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/atomic_integer_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_atomic_integer_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_atomic_integer PROPERTIES
        OUTPUT_NAME star_s32k148_atomic_integer
        SUFFIX .elf
    )
    target_include_directories(star_s32k148_atomic_integer PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR}
        ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include"
    )
    target_compile_definitions(star_s32k148_atomic_integer PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
    )
    target_compile_options(star_s32k148_atomic_integer PRIVATE
        -O0
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
        -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only
    )
    target_link_libraries(star_s32k148_atomic_integer PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_atomic_integer PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_atomic_integer>/star_s32k148_atomic_integer.map
        -T${RTS_S32K148_LINKER_SCRIPT}
    )
    add_custom_target(star_target_s32k148_atomic_integer
        DEPENDS star_s32k148_atomic_integer)

    add_executable(star_s32k148_contextual_integer
        ${RTS_S32K148_KERNEL_SOURCES}
        ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/contextual_integer_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_contextual_integer_main.c"
        startup/startup_s32k148.S
    )
    set_target_properties(star_s32k148_contextual_integer PROPERTIES
        OUTPUT_NAME star_s32k148_contextual_integer SUFFIX .elf)
    target_include_directories(star_s32k148_contextual_integer PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR} "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include" "${STAR_BENCHMARK_ROOT}/board/include")
    target_compile_definitions(star_s32k148_contextual_integer PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
    target_compile_options(star_s32k148_contextual_integer PRIVATE
        -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Wpedantic
        -mgeneral-regs-only)
    target_link_libraries(star_s32k148_contextual_integer PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_contextual_integer PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_contextual_integer>/star_s32k148_contextual_integer.map
        -T${RTS_S32K148_LINKER_SCRIPT})
    add_custom_target(star_target_s32k148_contextual_integer
        DEPENDS star_s32k148_contextual_integer)

    add_executable(star_s32k148_blind_integer_chain
        ${RTS_S32K148_KERNEL_SOURCES} ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/blind_integer_chain_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_blind_integer_chain_main.c"
        startup/startup_s32k148.S)
    set_target_properties(star_s32k148_blind_integer_chain PROPERTIES
        OUTPUT_NAME star_s32k148_blind_integer_chain SUFFIX .elf)
    target_include_directories(star_s32k148_blind_integer_chain PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration" "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include")
    target_compile_definitions(star_s32k148_blind_integer_chain PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
    target_compile_options(star_s32k148_blind_integer_chain PRIVATE
        -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections
        -fdata-sections -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only)
    target_link_libraries(star_s32k148_blind_integer_chain PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_blind_integer_chain PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_blind_integer_chain>/star_s32k148_blind_integer_chain.map
        -T${RTS_S32K148_LINKER_SCRIPT})
    add_custom_target(star_target_s32k148_blind_integer_chain
        DEPENDS star_s32k148_blind_integer_chain)

    add_executable(star_s32k148_bulk_integer_validation
        ${RTS_S32K148_KERNEL_SOURCES} ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/bulk_integer_validation_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_bulk_integer_validation_main.c"
        startup/startup_s32k148.S)
    set_target_properties(star_s32k148_bulk_integer_validation PROPERTIES
        OUTPUT_NAME star_s32k148_bulk_integer_validation SUFFIX .elf)
    target_include_directories(star_s32k148_bulk_integer_validation PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration" "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include")
    target_compile_definitions(star_s32k148_bulk_integer_validation PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
    target_compile_options(star_s32k148_bulk_integer_validation PRIVATE
        -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections
        -fdata-sections -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only)
    target_link_libraries(star_s32k148_bulk_integer_validation PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_bulk_integer_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_bulk_integer_validation>/star_s32k148_bulk_integer_validation.map
        -T${RTS_S32K148_LINKER_SCRIPT})
    add_custom_target(star_target_s32k148_bulk_integer_validation
        DEPENDS star_s32k148_bulk_integer_validation)

    add_executable(star_s32k148_control_bitwise_validation
        ${RTS_S32K148_KERNEL_SOURCES} ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/control_bitwise_validation_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_control_bitwise_validation_main.c"
        startup/startup_s32k148.S)
    set_target_properties(star_s32k148_control_bitwise_validation PROPERTIES OUTPUT_NAME star_s32k148_control_bitwise_validation SUFFIX .elf)
    target_include_directories(star_s32k148_control_bitwise_validation PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f ${CMAKE_CURRENT_SOURCE_DIR}
        "${STAR_BENCHMARK_ROOT}/calibration" "${STAR_BENCHMARK_ROOT}/tasks/include" "${STAR_BENCHMARK_ROOT}/board/include")
    target_compile_definitions(star_s32k148_control_bitwise_validation PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
    target_compile_options(star_s32k148_control_bitwise_validation PRIVATE
        -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections
        -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only)
    target_link_libraries(star_s32k148_control_bitwise_validation PRIVATE rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_control_bitwise_validation PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_control_bitwise_validation>/star_s32k148_control_bitwise_validation.map
        -T${RTS_S32K148_LINKER_SCRIPT})
    add_custom_target(star_target_s32k148_control_bitwise_validation DEPENDS star_s32k148_control_bitwise_validation)

    add_executable(star_s32k148_blind_communication
        ${RTS_S32K148_KERNEL_SOURCES} ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/blind_communication_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_blind_communication_main.c"
        startup/startup_s32k148.S)
    set_target_properties(star_s32k148_blind_communication PROPERTIES OUTPUT_NAME star_s32k148_blind_communication SUFFIX .elf)
    target_include_directories(star_s32k148_blind_communication PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR} ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/kernel ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR} "${STAR_BENCHMARK_ROOT}/calibration" "${STAR_BENCHMARK_ROOT}/tasks/include" "${STAR_BENCHMARK_ROOT}/board/include")
    target_compile_definitions(star_s32k148_blind_communication PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
    target_compile_options(star_s32k148_blind_communication PRIVATE -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only)
    target_link_libraries(star_s32k148_blind_communication PRIVATE rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_blind_communication PRIVATE -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib -Wl,--gc-sections -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_blind_communication>/star_s32k148_blind_communication.map -T${RTS_S32K148_LINKER_SCRIPT})
    add_custom_target(star_target_s32k148_blind_communication DEPENDS star_s32k148_blind_communication)

    add_executable(star_s32k148_automotive_standard
        ${RTS_S32K148_KERNEL_SOURCES} ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/automotive_standard_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_automotive_standard_main.c"
        startup/startup_s32k148.S)
    set_target_properties(star_s32k148_automotive_standard PROPERTIES OUTPUT_NAME star_s32k148_automotive_standard SUFFIX .elf)
    target_include_directories(star_s32k148_automotive_standard PRIVATE ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR} ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/kernel ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f ${CMAKE_CURRENT_SOURCE_DIR} "${STAR_BENCHMARK_ROOT}/calibration" "${STAR_BENCHMARK_ROOT}/tasks/include" "${STAR_BENCHMARK_ROOT}/board/include")
    target_compile_definitions(star_s32k148_automotive_standard PRIVATE BENCHMARK_PLATFORM_CORTEX_M_DWT BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h" RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS} RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY} RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
    target_compile_options(star_s32k148_automotive_standard PRIVATE -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only)
    target_link_libraries(star_s32k148_automotive_standard PRIVATE rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_automotive_standard PRIVATE -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib -Wl,--gc-sections -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_automotive_standard>/star_s32k148_automotive_standard.map -T${RTS_S32K148_LINKER_SCRIPT})
    add_custom_target(star_target_s32k148_automotive_standard DEPENDS star_s32k148_automotive_standard)

    add_executable(star_s32k148_automotive_holdout
        ${RTS_S32K148_KERNEL_SOURCES} ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/automotive_holdout_task.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_automotive_holdout_main.c"
        startup/startup_s32k148.S)
    set_target_properties(star_s32k148_automotive_holdout PROPERTIES OUTPUT_NAME star_s32k148_automotive_holdout SUFFIX .elf)
    target_include_directories(star_s32k148_automotive_holdout PRIVATE ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR} ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/kernel ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f ${CMAKE_CURRENT_SOURCE_DIR} "${STAR_BENCHMARK_ROOT}/calibration" "${STAR_BENCHMARK_ROOT}/tasks/include" "${STAR_BENCHMARK_ROOT}/board/include")
    target_compile_definitions(star_s32k148_automotive_holdout PRIVATE BENCHMARK_PLATFORM_CORTEX_M_DWT BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h" RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS} RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY} RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
    target_compile_options(star_s32k148_automotive_holdout PRIVATE -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only)
    target_link_libraries(star_s32k148_automotive_holdout PRIVATE rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_automotive_holdout PRIVATE -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib -Wl,--gc-sections -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_automotive_holdout>/star_s32k148_automotive_holdout.map -T${RTS_S32K148_LINKER_SCRIPT})
    add_custom_target(star_target_s32k148_automotive_holdout DEPENDS star_s32k148_automotive_holdout)

    add_executable(star_s32k148_misra_automotive_complex
        ${RTS_S32K148_KERNEL_SOURCES} ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/misra_automotive_complex_task.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_misra_automotive_complex_main.c"
        startup/startup_s32k148.S)
    set_target_properties(star_s32k148_misra_automotive_complex PROPERTIES OUTPUT_NAME star_s32k148_misra_automotive_complex SUFFIX .elf)
    target_include_directories(star_s32k148_misra_automotive_complex PRIVATE ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR} ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/kernel ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f ${CMAKE_CURRENT_SOURCE_DIR} "${STAR_BENCHMARK_ROOT}/calibration" "${STAR_BENCHMARK_ROOT}/tasks/include" "${STAR_BENCHMARK_ROOT}/board/include")
    target_compile_definitions(star_s32k148_misra_automotive_complex PRIVATE BENCHMARK_PLATFORM_CORTEX_M_DWT BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h" RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS} RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY} RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
    target_compile_options(star_s32k148_misra_automotive_complex PRIVATE -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only)
    target_link_libraries(star_s32k148_misra_automotive_complex PRIVATE rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_misra_automotive_complex PRIVATE -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib -Wl,--gc-sections -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_misra_automotive_complex>/star_s32k148_misra_automotive_complex.map -T${RTS_S32K148_LINKER_SCRIPT})
    add_custom_target(star_target_s32k148_misra_automotive_complex DEPENDS star_s32k148_misra_automotive_complex)

    add_executable(star_s32k148_powertrain_numeric_calibration
        ${RTS_S32K148_KERNEL_SOURCES} ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/powertrain_numeric_calibration.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_powertrain_numeric_calibration_main.c"
        startup/startup_s32k148.S)
    set_target_properties(star_s32k148_powertrain_numeric_calibration PROPERTIES OUTPUT_NAME star_s32k148_powertrain_numeric_calibration SUFFIX .elf)
    target_include_directories(star_s32k148_powertrain_numeric_calibration PRIVATE ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR} ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/kernel ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f ${CMAKE_CURRENT_SOURCE_DIR} "${STAR_BENCHMARK_ROOT}/calibration" "${STAR_BENCHMARK_ROOT}/tasks/include" "${STAR_BENCHMARK_ROOT}/board/include")
    target_compile_definitions(star_s32k148_powertrain_numeric_calibration PRIVATE BENCHMARK_PLATFORM_CORTEX_M_DWT BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h" RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS} RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY} RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
    target_compile_options(star_s32k148_powertrain_numeric_calibration PRIVATE -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only)
    target_link_libraries(star_s32k148_powertrain_numeric_calibration PRIVATE rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_powertrain_numeric_calibration PRIVATE -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib -Wl,--gc-sections -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_powertrain_numeric_calibration>/star_s32k148_powertrain_numeric_calibration.map -T${RTS_S32K148_LINKER_SCRIPT})
    add_custom_target(star_target_s32k148_powertrain_numeric_calibration DEPENDS star_s32k148_powertrain_numeric_calibration)

    add_executable(star_s32k148_powertrain_numeric_holdout
        ${RTS_S32K148_KERNEL_SOURCES} ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/powertrain_numeric_holdout.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_powertrain_numeric_holdout_main.c"
        startup/startup_s32k148.S)
    set_target_properties(star_s32k148_powertrain_numeric_holdout PROPERTIES OUTPUT_NAME star_s32k148_powertrain_numeric_holdout SUFFIX .elf)
    target_include_directories(star_s32k148_powertrain_numeric_holdout PRIVATE ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR} ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/kernel ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f ${CMAKE_CURRENT_SOURCE_DIR} "${STAR_BENCHMARK_ROOT}/calibration" "${STAR_BENCHMARK_ROOT}/tasks/include" "${STAR_BENCHMARK_ROOT}/board/include")
    target_compile_definitions(star_s32k148_powertrain_numeric_holdout PRIVATE BENCHMARK_PLATFORM_CORTEX_M_DWT BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h" RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS} RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY} RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
    target_compile_options(star_s32k148_powertrain_numeric_holdout PRIVATE -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only)
    target_link_libraries(star_s32k148_powertrain_numeric_holdout PRIVATE rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_powertrain_numeric_holdout PRIVATE -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib -Wl,--gc-sections -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_powertrain_numeric_holdout>/star_s32k148_powertrain_numeric_holdout.map -T${RTS_S32K148_LINKER_SCRIPT})
    add_custom_target(star_target_s32k148_powertrain_numeric_holdout DEPENDS star_s32k148_powertrain_numeric_holdout)

    add_executable(star_s32k148_powertrain_cpu_tasks
        ${RTS_S32K148_KERNEL_SOURCES} ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/powertrain_cpu_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_powertrain_cpu_tasks_main.c"
        startup/startup_s32k148.S)
    set_target_properties(star_s32k148_powertrain_cpu_tasks PROPERTIES OUTPUT_NAME star_s32k148_powertrain_cpu_tasks SUFFIX .elf)
    target_include_directories(star_s32k148_powertrain_cpu_tasks PRIVATE ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR} ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/kernel ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f ${CMAKE_CURRENT_SOURCE_DIR} "${STAR_BENCHMARK_ROOT}/calibration" "${STAR_BENCHMARK_ROOT}/tasks/include" "${STAR_BENCHMARK_ROOT}/board/include")
    target_compile_definitions(star_s32k148_powertrain_cpu_tasks PRIVATE BENCHMARK_PLATFORM_CORTEX_M_DWT BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h" RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS} RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY} RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
    target_compile_options(star_s32k148_powertrain_cpu_tasks PRIVATE -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only)
    target_link_libraries(star_s32k148_powertrain_cpu_tasks PRIVATE rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_powertrain_cpu_tasks PRIVATE -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib -Wl,--gc-sections -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_powertrain_cpu_tasks>/star_s32k148_powertrain_cpu_tasks.map -T${RTS_S32K148_LINKER_SCRIPT})
    add_custom_target(star_target_s32k148_powertrain_cpu_tasks DEPENDS star_s32k148_powertrain_cpu_tasks)

    add_executable(star_s32k148_powertrain_comm
        ${RTS_S32K148_KERNEL_SOURCES} ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/powertrain_diagnostics_communication_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_powertrain_diagnostics_communication_main.c"
        startup/startup_s32k148.S)
    set_target_properties(star_s32k148_powertrain_comm PROPERTIES OUTPUT_NAME star_s32k148_powertrain_comm SUFFIX .elf)
    target_include_directories(star_s32k148_powertrain_comm PRIVATE ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR} ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/kernel ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f ${CMAKE_CURRENT_SOURCE_DIR} "${STAR_BENCHMARK_ROOT}/calibration" "${STAR_BENCHMARK_ROOT}/tasks/include" "${STAR_BENCHMARK_ROOT}/board/include")
    target_compile_definitions(star_s32k148_powertrain_comm PRIVATE BENCHMARK_PLATFORM_CORTEX_M_DWT BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h" RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS} RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY} RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
    target_compile_options(star_s32k148_powertrain_comm PRIVATE -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only)
    target_link_libraries(star_s32k148_powertrain_comm PRIVATE rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_powertrain_comm PRIVATE -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib -Wl,--gc-sections -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_powertrain_comm>/star_s32k148_powertrain_comm.map -T${RTS_S32K148_LINKER_SCRIPT})
    add_custom_target(star_target_s32k148_powertrain_diagnostics_communication DEPENDS star_s32k148_powertrain_comm)

    add_executable(star_s32k148_mixed_comm
        ${RTS_S32K148_KERNEL_SOURCES} ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/mixed_memory_communication_calibration.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_mixed_memory_communication_main.c"
        startup/startup_s32k148.S)
    set_target_properties(star_s32k148_mixed_comm PROPERTIES OUTPUT_NAME star_s32k148_mixed_memory_communication SUFFIX .elf)
    target_include_directories(star_s32k148_mixed_comm PRIVATE ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR} ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/kernel ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f ${CMAKE_CURRENT_SOURCE_DIR} "${STAR_BENCHMARK_ROOT}/calibration" "${STAR_BENCHMARK_ROOT}/tasks/include" "${STAR_BENCHMARK_ROOT}/board/include")
    target_compile_definitions(star_s32k148_mixed_comm PRIVATE BENCHMARK_PLATFORM_CORTEX_M_DWT BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h" RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS} RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY} RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
    target_compile_options(star_s32k148_mixed_comm PRIVATE -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only)
    target_link_libraries(star_s32k148_mixed_comm PRIVATE rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_mixed_comm PRIVATE -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib -Wl,--gc-sections -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_mixed_comm>/star_s32k148_mixed_memory_communication.map -T${RTS_S32K148_LINKER_SCRIPT})
    add_custom_target(star_target_s32k148_mixed_comm DEPENDS star_s32k148_mixed_comm)

    add_executable(star_s32k148_mixed_holdout
        ${RTS_S32K148_KERNEL_SOURCES} ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/mixed_memory_communication_holdout.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_mixed_memory_communication_holdout_main.c"
        startup/startup_s32k148.S)
    set_target_properties(star_s32k148_mixed_holdout PROPERTIES OUTPUT_NAME star_s32k148_mixed_memory_communication_holdout SUFFIX .elf)
    target_include_directories(star_s32k148_mixed_holdout PRIVATE ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR} ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/kernel ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f ${CMAKE_CURRENT_SOURCE_DIR} "${STAR_BENCHMARK_ROOT}/calibration" "${STAR_BENCHMARK_ROOT}/tasks/include" "${STAR_BENCHMARK_ROOT}/board/include")
    target_compile_definitions(star_s32k148_mixed_holdout PRIVATE BENCHMARK_PLATFORM_CORTEX_M_DWT BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h" RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS} RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY} RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
    target_compile_options(star_s32k148_mixed_holdout PRIVATE -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only)
    target_link_libraries(star_s32k148_mixed_holdout PRIVATE rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_mixed_holdout PRIVATE -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib -Wl,--gc-sections -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_mixed_holdout>/star_s32k148_mixed_memory_communication_holdout.map -T${RTS_S32K148_LINKER_SCRIPT})
    add_custom_target(star_target_s32k148_mixed_holdout DEPENDS star_s32k148_mixed_holdout)

    add_executable(star_s32k148_powertrain_ref
        ${RTS_S32K148_KERNEL_SOURCES} ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/powertrain_reference_application.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_powertrain_reference_main.c"
        startup/startup_s32k148.S)
    set_target_properties(star_s32k148_powertrain_ref PROPERTIES OUTPUT_NAME star_s32k148_powertrain_reference SUFFIX .elf)
    target_include_directories(star_s32k148_powertrain_ref PRIVATE ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR} ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/kernel ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f ${CMAKE_CURRENT_SOURCE_DIR} "${STAR_BENCHMARK_ROOT}/calibration" "${STAR_BENCHMARK_ROOT}/tasks/include" "${STAR_BENCHMARK_ROOT}/board/include")
    target_compile_definitions(star_s32k148_powertrain_ref PRIVATE BENCHMARK_PLATFORM_CORTEX_M_DWT BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h" RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS} RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY} RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
    target_compile_options(star_s32k148_powertrain_ref PRIVATE -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only)
    target_link_libraries(star_s32k148_powertrain_ref PRIVATE rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_powertrain_ref PRIVATE -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib -Wl,--gc-sections -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_powertrain_ref>/star_s32k148_powertrain_reference.map -T${RTS_S32K148_LINKER_SCRIPT})
    add_custom_target(star_target_s32k148_powertrain_ref DEPENDS star_s32k148_powertrain_ref)

    add_executable(star_s32k148_generalization_corpus
        ${RTS_S32K148_KERNEL_SOURCES} ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/generalization_corpus.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_generalization_corpus_main.c"
        startup/startup_s32k148.S)
    set_target_properties(star_s32k148_generalization_corpus PROPERTIES
        OUTPUT_NAME star_s32k148_generalization_corpus SUFFIX .elf)
    target_include_directories(star_s32k148_generalization_corpus PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR} "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include")
    target_compile_definitions(star_s32k148_generalization_corpus PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
    target_compile_options(star_s32k148_generalization_corpus PRIVATE
        -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding
        -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections -Wall -Wextra -Werror
        -Wpedantic -mgeneral-regs-only)
    target_link_libraries(star_s32k148_generalization_corpus PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_generalization_corpus PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_generalization_corpus>/star_s32k148_generalization_corpus.map
        -T${RTS_S32K148_LINKER_SCRIPT})
    add_custom_target(star_target_s32k148_generalization_corpus
        DEPENDS star_s32k148_generalization_corpus)

    add_executable(star_s32k148_generalization_holdout
        ${RTS_S32K148_KERNEL_SOURCES} ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/generalization_holdout_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_generalization_holdout_main.c"
        startup/startup_s32k148.S)
    set_target_properties(star_s32k148_generalization_holdout PROPERTIES
        OUTPUT_NAME star_s32k148_generalization_holdout SUFFIX .elf)
    target_include_directories(star_s32k148_generalization_holdout PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR} "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include")
    target_compile_definitions(star_s32k148_generalization_holdout PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
    target_compile_options(star_s32k148_generalization_holdout PRIVATE
        -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding
        -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections -Wall -Wextra -Werror
        -Wpedantic -mgeneral-regs-only)
    target_link_libraries(star_s32k148_generalization_holdout PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_generalization_holdout PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_generalization_holdout>/star_s32k148_generalization_holdout.map
        -T${RTS_S32K148_LINKER_SCRIPT})
    add_custom_target(star_target_s32k148_generalization_holdout
        DEPENDS star_s32k148_generalization_holdout)

    add_executable(star_s32k148_mixed_shape_corpus
        ${RTS_S32K148_KERNEL_SOURCES} ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/mixed_shape_corpus.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_mixed_shape_corpus_main.c"
        startup/startup_s32k148.S)
    set_target_properties(star_s32k148_mixed_shape_corpus PROPERTIES
        OUTPUT_NAME star_s32k148_mixed_shape_corpus SUFFIX .elf)
    target_include_directories(star_s32k148_mixed_shape_corpus PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR} "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include")
    target_compile_definitions(star_s32k148_mixed_shape_corpus PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
    target_compile_options(star_s32k148_mixed_shape_corpus PRIVATE
        -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding
        -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections -Wall -Wextra -Werror
        -Wpedantic -mgeneral-regs-only)
    target_link_libraries(star_s32k148_mixed_shape_corpus PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_mixed_shape_corpus PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_mixed_shape_corpus>/star_s32k148_mixed_shape_corpus.map
        -T${RTS_S32K148_LINKER_SCRIPT})
    add_custom_target(star_target_s32k148_mixed_shape_corpus
        DEPENDS star_s32k148_mixed_shape_corpus)

    add_executable(star_s32k148_mixed_shape_blind
        ${RTS_S32K148_KERNEL_SOURCES} ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/mixed_shape_blind_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_mixed_shape_blind_main.c"
        startup/startup_s32k148.S)
    set_target_properties(star_s32k148_mixed_shape_blind PROPERTIES
        OUTPUT_NAME star_s32k148_mixed_shape_blind SUFFIX .elf)
    target_include_directories(star_s32k148_mixed_shape_blind PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR} "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include")
    target_compile_definitions(star_s32k148_mixed_shape_blind PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
    target_compile_options(star_s32k148_mixed_shape_blind PRIVATE
        -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding
        -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections -Wall -Wextra -Werror
        -Wpedantic -mgeneral-regs-only)
    target_link_libraries(star_s32k148_mixed_shape_blind PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_mixed_shape_blind PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_mixed_shape_blind>/star_s32k148_mixed_shape_blind.map
        -T${RTS_S32K148_LINKER_SCRIPT})
    add_custom_target(star_target_s32k148_mixed_shape_blind
        DEPENDS star_s32k148_mixed_shape_blind)

    add_executable(star_s32k148_protocol_runtime_corpus
        ${RTS_S32K148_KERNEL_SOURCES} ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/protocol_runtime_corpus.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_protocol_runtime_corpus_main.c"
        startup/startup_s32k148.S)
    set_target_properties(star_s32k148_protocol_runtime_corpus PROPERTIES
        OUTPUT_NAME star_s32k148_protocol_runtime_corpus SUFFIX .elf)
    target_include_directories(star_s32k148_protocol_runtime_corpus PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR} "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include")
    target_compile_definitions(star_s32k148_protocol_runtime_corpus PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
    target_compile_options(star_s32k148_protocol_runtime_corpus PRIVATE
        -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding
        -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections -Wall -Wextra -Werror
        -Wpedantic -mgeneral-regs-only)
    target_link_libraries(star_s32k148_protocol_runtime_corpus PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_protocol_runtime_corpus PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_protocol_runtime_corpus>/star_s32k148_protocol_runtime_corpus.map
        -T${RTS_S32K148_LINKER_SCRIPT})
    add_custom_target(star_target_s32k148_protocol_runtime_corpus
        DEPENDS star_s32k148_protocol_runtime_corpus)

    add_executable(star_s32k148_protocol_runtime_blind
        ${RTS_S32K148_KERNEL_SOURCES} ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/protocol_runtime_blind_tasks.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_protocol_runtime_blind_main.c"
        startup/startup_s32k148.S)
    set_target_properties(star_s32k148_protocol_runtime_blind PROPERTIES
        OUTPUT_NAME star_s32k148_protocol_runtime_blind SUFFIX .elf)
    target_include_directories(star_s32k148_protocol_runtime_blind PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR} "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/tasks/include"
        "${STAR_BENCHMARK_ROOT}/board/include")
    target_compile_definitions(star_s32k148_protocol_runtime_blind PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
    target_compile_options(star_s32k148_protocol_runtime_blind PRIVATE
        -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding
        -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections -Wall -Wextra -Werror
        -Wpedantic -mgeneral-regs-only)
    target_link_libraries(star_s32k148_protocol_runtime_blind PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_protocol_runtime_blind PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_protocol_runtime_blind>/star_s32k148_protocol_runtime_blind.map
        -T${RTS_S32K148_LINKER_SCRIPT})
    add_custom_target(star_target_s32k148_protocol_runtime_blind
        DEPENDS star_s32k148_protocol_runtime_blind)

    add_executable(star_s32k148_execution_component_corpus
        ${RTS_S32K148_KERNEL_SOURCES} ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/execution_component_corpus.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_execution_component_corpus_main.c"
        startup/startup_s32k148.S)
    set_target_properties(star_s32k148_execution_component_corpus PROPERTIES
        OUTPUT_NAME star_s32k148_execution_component_corpus SUFFIX .elf)
    target_include_directories(star_s32k148_execution_component_corpus PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR} "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/board/include"
        "${STAR_BENCHMARK_ROOT}/tasks/include")
    target_compile_definitions(star_s32k148_execution_component_corpus PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT
        BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
    target_compile_options(star_s32k148_execution_component_corpus PRIVATE
        -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding
        -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables
        -ffunction-sections -fdata-sections -Wall -Wextra -Werror
        -Wpedantic -mgeneral-regs-only)
    target_link_libraries(star_s32k148_execution_component_corpus PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_execution_component_corpus PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
        -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_execution_component_corpus>/star_s32k148_execution_component_corpus.map
        -T${RTS_S32K148_LINKER_SCRIPT})
    add_custom_target(star_target_s32k148_execution_component_corpus
        DEPENDS star_s32k148_execution_component_corpus)

    add_executable(star_s32k148_execution_component_holdout
        ${RTS_S32K148_KERNEL_SOURCES} ${STAR_BENCHMARK_BOARD_SOURCES}
        "${STAR_BENCHMARK_ROOT}/calibration/execution_component_holdout.c"
        "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_execution_component_holdout_main.c"
        startup/startup_s32k148.S)
    set_target_properties(star_s32k148_execution_component_holdout PROPERTIES
        OUTPUT_NAME star_s32k148_execution_component_holdout SUFFIX .elf)
    target_include_directories(star_s32k148_execution_component_holdout PRIVATE
        ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR}
        ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
        ${CMAKE_CURRENT_SOURCE_DIR} "${STAR_BENCHMARK_ROOT}/calibration"
        "${STAR_BENCHMARK_ROOT}/board/include" "${STAR_BENCHMARK_ROOT}/tasks/include")
    target_compile_definitions(star_s32k148_execution_component_holdout PRIVATE
        BENCHMARK_PLATFORM_CORTEX_M_DWT BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
        RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
        RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
        RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
    target_compile_options(star_s32k148_execution_component_holdout PRIVATE
        -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding -fno-builtin
        -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections
        -fdata-sections -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only)
    target_link_libraries(star_s32k148_execution_component_holdout PRIVATE
        rts_target_s32k148 rts_cortex_m4f_port gcc)
    target_link_options(star_s32k148_execution_component_holdout PRIVATE
        -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib -Wl,--gc-sections
        -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_execution_component_holdout>/star_s32k148_execution_component_holdout.map
        -T${RTS_S32K148_LINKER_SCRIPT})
    add_custom_target(star_target_s32k148_execution_component_holdout
        DEPENDS star_s32k148_execution_component_holdout)

    set(STAR_BODY_CHASSIS_SOURCE "${STAR_TASK_CONTROLLER_ROOT}/task_admission_framework/examples/body_chassis_control_holdout.c")
    if(EXISTS "${STAR_BODY_CHASSIS_SOURCE}")
        add_executable(star_s32k148_body_chassis_holdout
            ${RTS_S32K148_KERNEL_SOURCES} ${STAR_BENCHMARK_BOARD_SOURCES}
            "${STAR_BODY_CHASSIS_SOURCE}"
            "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_body_chassis_holdout_main.c"
            startup/startup_s32k148.S)
        set_target_properties(star_s32k148_body_chassis_holdout PROPERTIES OUTPUT_NAME star_s32k148_body_chassis_holdout SUFFIX .elf)
        target_include_directories(star_s32k148_body_chassis_holdout PRIVATE
            ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR}
            ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/kernel
            ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f ${CMAKE_CURRENT_SOURCE_DIR}
            "${STAR_TASK_CONTROLLER_ROOT}/task_admission_framework/examples"
            "${STAR_BENCHMARK_ROOT}/board/include" "${STAR_BENCHMARK_ROOT}/tasks/include")
        target_compile_definitions(star_s32k148_body_chassis_holdout PRIVATE
            BENCHMARK_PLATFORM_CORTEX_M_DWT BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
            RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
            RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
            RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
        target_compile_options(star_s32k148_body_chassis_holdout PRIVATE
            -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding -fno-builtin
            -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections
            -fdata-sections -Wall -Wextra -Werror -Wpedantic -mgeneral-regs-only)
        target_link_libraries(star_s32k148_body_chassis_holdout PRIVATE rts_target_s32k148 rts_cortex_m4f_port gcc)
        target_link_options(star_s32k148_body_chassis_holdout PRIVATE
            -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib -Wl,--gc-sections
            -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_body_chassis_holdout>/star_s32k148_body_chassis_holdout.map
            -T${RTS_S32K148_LINKER_SCRIPT})
        add_custom_target(star_target_s32k148_body_chassis_holdout DEPENDS star_s32k148_body_chassis_holdout)
    endif()

    set(STAR_AUTOMOTIVE_FEATURE_SOURCE
        "${STAR_TASK_CONTROLLER_ROOT}/task_admission_framework/examples/automotive_feature_reference.c")
    if(EXISTS "${STAR_AUTOMOTIVE_FEATURE_SOURCE}")
        add_executable(star_s32k148_automotive_feature_reference
            ${RTS_S32K148_KERNEL_SOURCES} ${STAR_BENCHMARK_BOARD_SOURCES}
            "${STAR_AUTOMOTIVE_FEATURE_SOURCE}"
            "${STAR_BENCHMARK_ROOT}/board/integration/s32k148_automotive_feature_reference_main.c"
            startup/startup_s32k148.S)
        set_target_properties(star_s32k148_automotive_feature_reference PROPERTIES
            OUTPUT_NAME star_s32k148_automotive_feature_reference SUFFIX .elf)
        target_include_directories(star_s32k148_automotive_feature_reference PRIVATE
            ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR}
            ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/kernel ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
            ${CMAKE_CURRENT_SOURCE_DIR}
            "${STAR_TASK_CONTROLLER_ROOT}/task_admission_framework/examples"
            "${STAR_BENCHMARK_ROOT}/board/include"
            "${STAR_BENCHMARK_ROOT}/tasks/include")
        target_compile_definitions(star_s32k148_automotive_feature_reference PRIVATE
            BENCHMARK_PLATFORM_CORTEX_M_DWT
            BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
            RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
            RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
            RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
        target_compile_options(star_s32k148_automotive_feature_reference PRIVATE
            -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding
            -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables
            -ffunction-sections -fdata-sections -Wall -Wextra -Werror
            -Wpedantic -mgeneral-regs-only)
        target_link_libraries(star_s32k148_automotive_feature_reference PRIVATE
            rts_target_s32k148 rts_cortex_m4f_port gcc)
        target_link_options(star_s32k148_automotive_feature_reference PRIVATE
            -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
            -Wl,--gc-sections
            -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_automotive_feature_reference>/star_s32k148_automotive_feature_reference.map
            -T${RTS_S32K148_LINKER_SCRIPT})
        add_custom_target(star_target_s32k148_automotive_feature_reference
            DEPENDS star_s32k148_automotive_feature_reference)
    endif()

    set(STAR_COMPOUND_CALIBRATION_SOURCE
        "C:/Users/nxg19748/OneDrive - NXP/Desktop/real_time_scheduler_task_controller/task_admission_framework/examples/powertrain_compound_calibration.c")
    if(EXISTS "${STAR_COMPOUND_CALIBRATION_SOURCE}")
        add_executable(star_s32k148_powertrain_compound_calibration
            ${RTS_S32K148_KERNEL_SOURCES} ${STAR_BENCHMARK_BOARD_SOURCES}
            "${STAR_COMPOUND_CALIBRATION_SOURCE}"
            ${PROJECT_SOURCE_DIR}/examples/s32k148_powertrain_compound_calibration/main.c
            startup/startup_s32k148.S)
        set_target_properties(star_s32k148_powertrain_compound_calibration PROPERTIES
            OUTPUT_NAME star_s32k148_powertrain_compound_calibration SUFFIX .elf)
        target_include_directories(star_s32k148_powertrain_compound_calibration PRIVATE
            ${RTS_S32K148_DEVICE_INCLUDE_DIR} ${RTS_S32K148_CMSIS_INCLUDE_DIR}
            ${RTS_CONFIG_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/kernel ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
            ${CMAKE_CURRENT_SOURCE_DIR}
            "C:/Users/nxg19748/OneDrive - NXP/Desktop/real_time_scheduler_task_controller/task_admission_framework/examples"
            "${STAR_BENCHMARK_ROOT}/board/include"
            "${STAR_BENCHMARK_ROOT}/tasks/include")
        target_compile_definitions(star_s32k148_powertrain_compound_calibration PRIVATE
            BENCHMARK_PLATFORM_CORTEX_M_DWT
            BENCHMARK_CMSIS_DEVICE_HEADER="target_device.h"
            RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
            RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
            RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY})
        target_compile_options(star_s32k148_powertrain_compound_calibration PRIVATE
            -O0 -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -ffreestanding
            -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables
            -ffunction-sections -fdata-sections -Wall -Wextra -Werror
            -Wpedantic -mgeneral-regs-only)
        target_link_libraries(star_s32k148_powertrain_compound_calibration PRIVATE
            rts_target_s32k148 rts_cortex_m4f_port gcc)
        target_link_options(star_s32k148_powertrain_compound_calibration PRIVATE
            -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostdlib
            -Wl,--gc-sections
            -Wl,-Map,$<TARGET_FILE_DIR:star_s32k148_powertrain_compound_calibration>/star_s32k148_powertrain_compound_calibration.map
            -T${RTS_S32K148_LINKER_SCRIPT})
        add_custom_target(star_target_s32k148_powertrain_compound_calibration
            DEPENDS star_s32k148_powertrain_compound_calibration)
    endif()
endif()

rts_add_flexcan_isr_target()
