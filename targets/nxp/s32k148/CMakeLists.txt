if(NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "RTS_BUILD_S32K148_SMOKE requires an ARM bare-metal toolchain file")
endif()
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
endif()
