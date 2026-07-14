if(NOT DEFINED RTS_CONFIG_INCLUDE_DIR)
    message(FATAL_ERROR "RTS_CONFIG_INCLUDE_DIR is required for Cortex-M4F")
endif()
foreach(required RTS_CORTEX_M_NVIC_PRIORITY_BITS
                 RTS_CORTEX_M_PENDSV_PRIORITY
                 RTS_CORTEX_M_SVC_PRIORITY)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required for Cortex-M4F")
    endif()
endforeach()

enable_language(ASM)

add_library(rts_cortex_m4f_port STATIC
    port_stack.c
    port_start.c
    port_switch.c
    port_asm.S
)
target_include_directories(rts_cortex_m4f_port
    PRIVATE
        ${RTS_CONFIG_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/kernel
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(rts_cortex_m4f_port PRIVATE
    RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
    RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
    RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
)
target_compile_options(rts_cortex_m4f_port PRIVATE
    -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
    -ffreestanding -fno-builtin
    -Wall -Wextra -Werror -Wpedantic
)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(rts_cortex_m4f_port PRIVATE -mgeneral-regs-only)
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(rts_cortex_m4f_port PRIVATE -mfpu=none)
endif()

add_library(rts_cortex_m4f_stack_contract_test OBJECT
    ${PROJECT_SOURCE_DIR}/tests/unit/test_cortex_m4f_stack_frame.c
)
target_include_directories(rts_cortex_m4f_stack_contract_test PRIVATE
    ${RTS_CONFIG_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/kernel
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(rts_cortex_m4f_stack_contract_test PRIVATE
    RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
    RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
    RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
)
target_compile_options(rts_cortex_m4f_stack_contract_test PRIVATE
    -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
    -ffreestanding -fno-builtin
    -Wall -Wextra -Werror -Wpedantic
)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(rts_cortex_m4f_stack_contract_test
        PRIVATE -mgeneral-regs-only)
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(rts_cortex_m4f_stack_contract_test PRIVATE -mfpu=none)
endif()

add_library(rts_cortex_m4f_start_contract_test OBJECT
    ${PROJECT_SOURCE_DIR}/tests/unit/test_cortex_m4f_start_handoff.c
)
target_include_directories(rts_cortex_m4f_start_contract_test PRIVATE
    ${RTS_CONFIG_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/kernel
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(rts_cortex_m4f_start_contract_test PRIVATE
    RTS_CORTEX_M_NVIC_PRIORITY_BITS=${RTS_CORTEX_M_NVIC_PRIORITY_BITS}
    RTS_CORTEX_M_PENDSV_PRIORITY=${RTS_CORTEX_M_PENDSV_PRIORITY}
    RTS_CORTEX_M_SVC_PRIORITY=${RTS_CORTEX_M_SVC_PRIORITY}
)
target_compile_options(rts_cortex_m4f_start_contract_test PRIVATE
    -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
    -ffreestanding -fno-builtin
    -Wall -Wextra -Werror -Wpedantic
)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(rts_cortex_m4f_start_contract_test
        PRIVATE -mgeneral-regs-only)
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(rts_cortex_m4f_start_contract_test PRIVATE -mfpu=none)
endif()
