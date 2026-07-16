set(SYMBOL_FILE "${OUTPUT_DIR}/rts_s32k148_smoke.sym")
set(DISASSEMBLY_FILE "${OUTPUT_DIR}/rts_s32k148_smoke.dis")
set(SECTION_FILE "${OUTPUT_DIR}/rts_s32k148_smoke.sections")

execute_process(COMMAND "${NM_TOOL}" -an "${ELF_FILE}"
                OUTPUT_FILE "${SYMBOL_FILE}" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${OBJDUMP_TOOL}" -d "${ELF_FILE}"
                OUTPUT_FILE "${DISASSEMBLY_FILE}" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${OBJDUMP_TOOL}" -h "${ELF_FILE}"
                OUTPUT_FILE "${SECTION_FILE}" COMMAND_ERROR_IS_FATAL ANY)

file(READ "${SYMBOL_FILE}" symbols)
file(READ "${DISASSEMBLY_FILE}" disassembly)

foreach(required Reset_Handler SVC_Handler PendSV_Handler SysTick_Handler
                 LPTMR0_IRQHandler rts_port_power_sleep
                 HardFault_Handler
                 rts_kernel_tick_advance
                 rts_kernel_fatal_at g_rts_fatal_record
                 g_pfnVectors __StackTop
                 g_task_a_stack g_task_b_stack g_task_c_stack)
    string(REGEX MATCHALL "(^|\n)[0-9a-fA-F]+[ \t]+[A-Za-z][ \t]+${required}(\r?\n|$)"
           matches "${symbols}")
    list(LENGTH matches match_count)
    if(NOT match_count EQUAL 1)
        message(FATAL_ERROR
            "Expected exactly one final-image symbol ${required}; found ${match_count}")
    endif()
endforeach()

foreach(forbidden malloc calloc realloc free printf fprintf puts rts_host_test
                  LPIT0_Ch0_IRQHandler PIT_Ch0_IRQHandler)
    if(symbols MATCHES "[ \t]${forbidden}($|[ \t])")
        message(FATAL_ERROR "Forbidden final-image symbol found: ${forbidden}")
    endif()
endforeach()

if(disassembly MATCHES "[ \t](vldr|vstr|vmov|vadd|vsub|vmul|vdiv|vpush|vpop|vsqrt|vcvt)\\.")
    message(FATAL_ERROR "VFP instruction found in no-FPU target image")
endif()
if(NOT disassembly MATCHES "mrs[ \t]+[^\n]*psp")
    message(FATAL_ERROR "Final image does not contain a PSP read")
endif()
if(NOT disassembly MATCHES "msr[ \t]+psp")
    message(FATAL_ERROR "Final image does not contain a PSP write")
endif()
if(NOT disassembly MATCHES "svc[ \t]+#0x?0")
    message(FATAL_ERROR "Final image does not contain the approved SVC #0")
endif()

message(STATUS "S32K148 smoke-image symbol and disassembly checks passed")
