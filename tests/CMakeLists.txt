add_library(rts_intrusive_list STATIC
    ${PROJECT_SOURCE_DIR}/kernel/intrusive_list.c
)

target_include_directories(rts_intrusive_list
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/tests/config
)

add_executable(rts_test_intrusive_list
    unit/test_intrusive_list.c
)

target_include_directories(rts_test_intrusive_list
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/tests/config
)

target_link_libraries(rts_test_intrusive_list PRIVATE rts_intrusive_list)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rts_intrusive_list PRIVATE -Wall -Wextra -Werror)
    target_compile_options(rts_test_intrusive_list PRIVATE -Wall -Wextra -Werror)
elseif(MSVC)
    target_compile_options(rts_intrusive_list PRIVATE /W4 /WX)
    target_compile_options(rts_test_intrusive_list PRIVATE /W4 /WX)
endif()

add_test(NAME rts_intrusive_list COMMAND rts_test_intrusive_list)

add_library(rts_ready_queue STATIC
    ${PROJECT_SOURCE_DIR}/kernel/ready_queue.c
)

target_include_directories(rts_ready_queue
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/tests/config
)

target_link_libraries(rts_ready_queue PRIVATE rts_intrusive_list)

add_executable(rts_test_ready_queue
    unit/test_ready_queue.c
)

target_include_directories(rts_test_ready_queue
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/tests/config
)

target_link_libraries(rts_test_ready_queue
    PRIVATE
        rts_ready_queue
        rts_intrusive_list
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rts_ready_queue PRIVATE -Wall -Wextra -Werror)
    target_compile_options(rts_test_ready_queue PRIVATE -Wall -Wextra -Werror)
elseif(MSVC)
    target_compile_options(rts_ready_queue PRIVATE /W4 /WX)
    target_compile_options(rts_test_ready_queue PRIVATE /W4 /WX)
endif()

add_test(NAME rts_ready_queue COMMAND rts_test_ready_queue)

add_library(rts_delay_queue STATIC
    ${PROJECT_SOURCE_DIR}/kernel/delay_queue.c
)

target_include_directories(rts_delay_queue
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/tests/config
)

target_link_libraries(rts_delay_queue PRIVATE rts_intrusive_list)

add_executable(rts_test_delay_queue
    unit/test_delay_queue.c
)

target_include_directories(rts_test_delay_queue
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/tests/config
)

target_link_libraries(rts_test_delay_queue
    PRIVATE
        rts_delay_queue
        rts_intrusive_list
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rts_delay_queue PRIVATE -Wall -Wextra -Werror)
    target_compile_options(rts_test_delay_queue PRIVATE -Wall -Wextra -Werror)
elseif(MSVC)
    target_compile_options(rts_delay_queue PRIVATE /W4 /WX)
    target_compile_options(rts_test_delay_queue PRIVATE /W4 /WX)
endif()

add_test(NAME rts_delay_queue COMMAND rts_test_delay_queue)

add_library(rts_task_pool STATIC
    ${PROJECT_SOURCE_DIR}/kernel/task_pool.c
)

target_include_directories(rts_task_pool
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/tests/config
)

add_executable(rts_test_task_pool
    unit/test_task_pool.c
)

target_include_directories(rts_test_task_pool
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/tests/config
)

target_link_libraries(rts_test_task_pool PRIVATE rts_task_pool)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rts_task_pool PRIVATE -Wall -Wextra -Werror)
    target_compile_options(rts_test_task_pool PRIVATE -Wall -Wextra -Werror)
elseif(MSVC)
    target_compile_options(rts_task_pool PRIVATE /W4 /WX)
    target_compile_options(rts_test_task_pool PRIVATE /W4 /WX)
endif()

add_test(NAME rts_task_pool COMMAND rts_test_task_pool)

add_library(rts_task_validation STATIC
    ${PROJECT_SOURCE_DIR}/kernel/task_validation.c
)

target_include_directories(rts_task_validation
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/tests/config
)

add_executable(rts_test_task_validation
    unit/test_task_validation.c
)

target_include_directories(rts_test_task_validation
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/tests/config
)

target_link_libraries(rts_test_task_validation PRIVATE rts_task_validation)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rts_task_validation PRIVATE -Wall -Wextra -Werror)
    target_compile_options(rts_test_task_validation PRIVATE -Wall -Wextra -Werror)
elseif(MSVC)
    target_compile_options(rts_task_validation PRIVATE /W4 /WX)
    target_compile_options(rts_test_task_validation PRIVATE /W4 /WX)
endif()

add_test(NAME rts_task_validation COMMAND rts_test_task_validation)

add_library(rts_task_object STATIC
    ${PROJECT_SOURCE_DIR}/kernel/task_object.c
)

target_include_directories(rts_task_object
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/tests/config
)

target_link_libraries(rts_task_object PRIVATE rts_intrusive_list)

add_executable(rts_test_task_object
    unit/test_task_object.c
)

target_include_directories(rts_test_task_object
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/tests/config
)

target_link_libraries(rts_test_task_object
    PRIVATE
        rts_task_object
        rts_task_pool
        rts_intrusive_list
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rts_task_object PRIVATE -Wall -Wextra -Werror)
    target_compile_options(rts_test_task_object PRIVATE -Wall -Wextra -Werror)
elseif(MSVC)
    target_compile_options(rts_task_object PRIVATE /W4 /WX)
    target_compile_options(rts_test_task_object PRIVATE /W4 /WX)
endif()

add_test(NAME rts_task_object COMMAND rts_test_task_object)

add_library(rts_host_stack_port STATIC
    ${PROJECT_SOURCE_DIR}/ports/host/port.c
    ${PROJECT_SOURCE_DIR}/ports/host/port_start.c
)

target_include_directories(rts_host_stack_port
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/host
        ${PROJECT_SOURCE_DIR}/tests/config
)

add_executable(rts_test_host_stack_port
    unit/test_host_stack_port.c
)

target_include_directories(rts_test_host_stack_port
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/host
        ${PROJECT_SOURCE_DIR}/tests/config
)

target_link_libraries(rts_test_host_stack_port PRIVATE rts_host_stack_port)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rts_host_stack_port PRIVATE -Wall -Wextra -Werror)
    target_compile_options(rts_test_host_stack_port PRIVATE -Wall -Wextra -Werror)
elseif(MSVC)
    target_compile_options(rts_host_stack_port PRIVATE /W4 /WX)
    target_compile_options(rts_test_host_stack_port PRIVATE /W4 /WX)
endif()

add_test(NAME rts_host_stack_port COMMAND rts_test_host_stack_port)

add_executable(rts_test_host_first_task_start
    unit/test_host_first_task_start.c
    ${PROJECT_SOURCE_DIR}/ports/host/port_start.c
)
target_include_directories(rts_test_host_first_task_start PRIVATE
    ${PROJECT_SOURCE_DIR}/tests/config
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/kernel
    ${PROJECT_SOURCE_DIR}/ports/host
)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rts_test_host_first_task_start PRIVATE
        -Wall -Wextra -Werror -Wpedantic)
elseif(MSVC)
    target_compile_options(rts_test_host_first_task_start PRIVATE /W4 /WX)
endif()
add_test(NAME rts_host_first_task_start COMMAND rts_test_host_first_task_start)

add_library(rts_kernel_state STATIC
    ${PROJECT_SOURCE_DIR}/kernel/kernel_state.c
)

target_include_directories(rts_kernel_state
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/tests/config
)

add_library(rts_task_create STATIC
    ${PROJECT_SOURCE_DIR}/kernel/task_create.c
    ${PROJECT_SOURCE_DIR}/kernel/task_handle.c
)

target_include_directories(rts_task_create
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/tests/config
)

target_link_libraries(rts_task_create
    PRIVATE
        rts_task_pool
        rts_task_validation
        rts_task_object
        rts_ready_queue
        rts_kernel_state
)

add_executable(rts_test_task_create
    unit/test_task_create.c
)

target_include_directories(rts_test_task_create
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/host
        ${PROJECT_SOURCE_DIR}/tests/config
)

target_link_libraries(rts_test_task_create
    PRIVATE
        rts_task_create
        rts_task_pool
        rts_task_validation
        rts_task_object
        rts_ready_queue
        rts_intrusive_list
        rts_host_stack_port
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rts_task_create PRIVATE -Wall -Wextra -Werror)
    target_compile_options(rts_test_task_create PRIVATE -Wall -Wextra -Werror)
elseif(MSVC)
    target_compile_options(rts_task_create PRIVATE /W4 /WX)
    target_compile_options(rts_test_task_create PRIVATE /W4 /WX)
endif()

add_test(NAME rts_task_create COMMAND rts_test_task_create)

add_library(rts_kernel_bootstrap STATIC
    ${PROJECT_SOURCE_DIR}/kernel/scheduler_init.c
)

target_include_directories(rts_kernel_bootstrap
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/tests/config
)

target_link_libraries(rts_kernel_bootstrap
    PRIVATE
        rts_kernel_state
        rts_task_pool
        rts_ready_queue
        rts_delay_queue
)

add_executable(rts_test_kernel_bootstrap
    unit/test_kernel_bootstrap.c
)

target_include_directories(rts_test_kernel_bootstrap
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/host
        ${PROJECT_SOURCE_DIR}/tests/config
)

target_link_libraries(rts_test_kernel_bootstrap
    PRIVATE
        rts_kernel_bootstrap
        rts_kernel_state
        rts_task_create
        rts_task_pool
        rts_task_validation
        rts_task_object
        rts_ready_queue
        rts_delay_queue
        rts_intrusive_list
        rts_host_stack_port
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rts_kernel_state PRIVATE -Wall -Wextra -Werror)
    target_compile_options(rts_kernel_bootstrap PRIVATE -Wall -Wextra -Werror)
    target_compile_options(rts_test_kernel_bootstrap PRIVATE -Wall -Wextra -Werror)
elseif(MSVC)
    target_compile_options(rts_kernel_state PRIVATE /W4 /WX)
    target_compile_options(rts_kernel_bootstrap PRIVATE /W4 /WX)
    target_compile_options(rts_test_kernel_bootstrap PRIVATE /W4 /WX)
endif()

add_test(NAME rts_kernel_bootstrap COMMAND rts_test_kernel_bootstrap)

add_library(rts_scheduler_select STATIC
    ${PROJECT_SOURCE_DIR}/kernel/scheduler_select.c
)

target_include_directories(rts_scheduler_select
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/tests/config
)

target_link_libraries(rts_scheduler_select
    PRIVATE
        rts_kernel_state
        rts_ready_queue
)

add_executable(rts_test_scheduler_select
    unit/test_scheduler_select.c
)

target_include_directories(rts_test_scheduler_select
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/host
        ${PROJECT_SOURCE_DIR}/tests/config
)

target_link_libraries(rts_test_scheduler_select
    PRIVATE
        rts_scheduler_select
        rts_kernel_bootstrap
        rts_kernel_state
        rts_task_create
        rts_task_pool
        rts_task_validation
        rts_task_object
        rts_ready_queue
        rts_delay_queue
        rts_intrusive_list
        rts_host_stack_port
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rts_scheduler_select PRIVATE -Wall -Wextra -Werror)
    target_compile_options(rts_test_scheduler_select PRIVATE -Wall -Wextra -Werror)
elseif(MSVC)
    target_compile_options(rts_scheduler_select PRIVATE /W4 /WX)
    target_compile_options(rts_test_scheduler_select PRIVATE /W4 /WX)
endif()

add_test(NAME rts_scheduler_select COMMAND rts_test_scheduler_select)

add_library(rts_switch_plan STATIC
    ${PROJECT_SOURCE_DIR}/kernel/switch_plan.c
)

target_include_directories(rts_switch_plan
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/tests/config
)

target_link_libraries(rts_switch_plan
    PRIVATE
        rts_scheduler_select
        rts_kernel_state
        rts_ready_queue
)

add_executable(rts_test_switch_plan
    unit/test_switch_plan.c
)

target_include_directories(rts_test_switch_plan
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/host
        ${PROJECT_SOURCE_DIR}/tests/config
)

target_link_libraries(rts_test_switch_plan
    PRIVATE
        rts_switch_plan
        rts_scheduler_select
        rts_kernel_bootstrap
        rts_kernel_state
        rts_task_create
        rts_task_pool
        rts_task_validation
        rts_task_object
        rts_ready_queue
        rts_delay_queue
        rts_intrusive_list
        rts_host_stack_port
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rts_switch_plan PRIVATE -Wall -Wextra -Werror)
    target_compile_options(rts_test_switch_plan PRIVATE -Wall -Wextra -Werror)
elseif(MSVC)
    target_compile_options(rts_switch_plan PRIVATE /W4 /WX)
    target_compile_options(rts_test_switch_plan PRIVATE /W4 /WX)
endif()

add_test(NAME rts_switch_plan COMMAND rts_test_switch_plan)

set(RTS_YIELD_PRODUCTION_SOURCES
    ${PROJECT_SOURCE_DIR}/kernel/intrusive_list.c
    ${PROJECT_SOURCE_DIR}/kernel/ready_queue.c
    ${PROJECT_SOURCE_DIR}/kernel/delay_queue.c
    ${PROJECT_SOURCE_DIR}/kernel/task_pool.c
    ${PROJECT_SOURCE_DIR}/kernel/task_validation.c
    ${PROJECT_SOURCE_DIR}/kernel/task_object.c
    ${PROJECT_SOURCE_DIR}/kernel/task_handle.c
    ${PROJECT_SOURCE_DIR}/kernel/task_create.c
    ${PROJECT_SOURCE_DIR}/kernel/kernel_state.c
    ${PROJECT_SOURCE_DIR}/kernel/scheduler_init.c
    ${PROJECT_SOURCE_DIR}/kernel/scheduler_select.c
    ${PROJECT_SOURCE_DIR}/kernel/switch_plan.c
    ${PROJECT_SOURCE_DIR}/kernel/task_yield.c
    ${PROJECT_SOURCE_DIR}/ports/host/port.c
)

function(rts_add_yield_test target config_directory)
    add_executable(${target}
        unit/test_task_yield.c
        ${RTS_YIELD_PRODUCTION_SOURCES}
    )
    target_include_directories(${target}
        PRIVATE
            ${PROJECT_SOURCE_DIR}/${config_directory}
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/kernel
            ${PROJECT_SOURCE_DIR}/ports/host
    )
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Werror -Wpedantic
            -Wconversion -Wsign-conversion -Wshadow
        )
    elseif(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    endif()
    add_test(NAME ${target} COMMAND ${target})
endfunction()

rts_add_yield_test(rts_test_task_yield tests/config)
rts_add_yield_test(rts_test_task_yield_release tests/config_release)
rts_add_yield_test(rts_test_task_yield_no_slicing tests/config_no_slicing)

add_executable(rts_test_cortex_m4f_switch_bridge
    unit/test_cortex_m4f_switch_bridge.c
    ${RTS_YIELD_PRODUCTION_SOURCES}
    ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f/port_switch.c
)
target_include_directories(rts_test_cortex_m4f_switch_bridge PRIVATE
    ${PROJECT_SOURCE_DIR}/tests/config
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/kernel
    ${PROJECT_SOURCE_DIR}/ports/host
    ${PROJECT_SOURCE_DIR}/ports/arm/cortex_m4f
)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rts_test_cortex_m4f_switch_bridge PRIVATE
        -Wall -Wextra -Werror -Wpedantic
        -Wconversion -Wsign-conversion -Wshadow)
elseif(MSVC)
    target_compile_options(rts_test_cortex_m4f_switch_bridge PRIVATE /W4 /WX)
endif()
add_test(NAME rts_cortex_m4f_switch_bridge
         COMMAND rts_test_cortex_m4f_switch_bridge)

add_executable(rts_test_scheduler_start
    unit/test_scheduler_start.c
    ${RTS_YIELD_PRODUCTION_SOURCES}
    ${PROJECT_SOURCE_DIR}/kernel/scheduler_start.c
    ${PROJECT_SOURCE_DIR}/ports/host/port_start.c
)
target_include_directories(rts_test_scheduler_start PRIVATE
    ${PROJECT_SOURCE_DIR}/tests/config
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/kernel
    ${PROJECT_SOURCE_DIR}/ports/host
)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rts_test_scheduler_start PRIVATE
        -Wall -Wextra -Werror -Wpedantic
        -Wconversion -Wsign-conversion -Wshadow)
elseif(MSVC)
    target_compile_options(rts_test_scheduler_start PRIVATE /W4 /WX)
endif()
add_test(NAME rts_scheduler_start COMMAND rts_test_scheduler_start)

function(rts_add_time_test target config_directory)
    add_executable(${target}
        unit/test_time.c
        ${PROJECT_SOURCE_DIR}/kernel/time.c
    )
    target_include_directories(${target} PRIVATE
        ${PROJECT_SOURCE_DIR}/${config_directory}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
    )
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Werror -Wpedantic
            -Wconversion -Wsign-conversion -Wshadow)
    elseif(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    endif()
    add_test(NAME ${target} COMMAND ${target})
endfunction()

rts_add_time_test(rts_test_time tests/config)
rts_add_time_test(rts_test_time_release tests/config_release)

add_executable(rts_test_s32k148_tick_math
    unit/test_s32k148_tick_math.c
    ${PROJECT_SOURCE_DIR}/targets/nxp/s32k148/target_tick_math.c
)
target_include_directories(rts_test_s32k148_tick_math PRIVATE
    ${PROJECT_SOURCE_DIR}/tests/config
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/targets/nxp/s32k148
)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rts_test_s32k148_tick_math PRIVATE
        -Wall -Wextra -Werror -Wpedantic
        -Wconversion -Wsign-conversion -Wshadow)
elseif(MSVC)
    target_compile_options(rts_test_s32k148_tick_math PRIVATE /W4 /WX)
endif()
add_test(NAME rts_s32k148_tick_math COMMAND rts_test_s32k148_tick_math)

add_executable(rts_test_s32k148_tick
    unit/test_s32k148_tick.c
    ${PROJECT_SOURCE_DIR}/targets/nxp/s32k148/target_tick.c
    ${PROJECT_SOURCE_DIR}/targets/nxp/s32k148/target_tick_math.c
    support/s32k148_cmsis_mock/cmsis_mock.c
)
target_include_directories(rts_test_s32k148_tick PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/support/s32k148_cmsis_mock
    ${PROJECT_SOURCE_DIR}/tests/config
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/kernel
    ${PROJECT_SOURCE_DIR}/targets/nxp/s32k148
)
target_compile_definitions(rts_test_s32k148_tick PRIVATE
    RTS_CORTEX_M_NVIC_PRIORITY_BITS=4
    RTS_CORTEX_M_SVC_PRIORITY=13
    RTS_CORTEX_M_PENDSV_PRIORITY=15
)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rts_test_s32k148_tick PRIVATE
        -Wall -Wextra -Werror -Wpedantic
        -Wconversion -Wsign-conversion -Wshadow)
elseif(MSVC)
    target_compile_options(rts_test_s32k148_tick PRIVATE /W4 /WX)
endif()
add_test(NAME rts_s32k148_tick COMMAND rts_test_s32k148_tick)

function(rts_add_task_delay_test target config_directory)
    add_executable(${target}
        unit/test_task_delay.c
        ${RTS_YIELD_PRODUCTION_SOURCES}
        ${PROJECT_SOURCE_DIR}/kernel/task_delay.c
        ${PROJECT_SOURCE_DIR}/kernel/time.c
        ${PROJECT_SOURCE_DIR}/kernel/scheduler_start.c
        ${PROJECT_SOURCE_DIR}/ports/host/port_start.c
    )
    target_include_directories(${target} PRIVATE
        ${PROJECT_SOURCE_DIR}/${config_directory}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${PROJECT_SOURCE_DIR}/ports/host
    )
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Werror -Wpedantic
            -Wconversion -Wsign-conversion -Wshadow)
    elseif(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    endif()
    add_test(NAME ${target} COMMAND ${target})
endfunction()

rts_add_task_delay_test(rts_test_task_delay tests/config)
rts_add_task_delay_test(rts_test_task_delay_release tests/config_release)
