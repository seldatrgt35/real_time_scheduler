#ifndef RTS_CORTEX_M4F_PORT_OFFSETS_H
#define RTS_CORTEX_M4F_PORT_OFFSETS_H

/* Shared by preprocessed assembly and C layout verification. */
#define RTS_CM4F_TCB_SAVED_SP_OFFSET             0
#define RTS_CM4F_HANDOFF_FROM_OFFSET              0
#define RTS_CM4F_HANDOFF_TO_OFFSET                4
#define RTS_CM4F_START_TASK_OFFSET                 0
#define RTS_CM4F_START_SAVED_SP_OFFSET             4
#define RTS_CM4F_START_COOKIE_OFFSET               8
#define RTS_CM4F_START_VALID_OFFSET               12

#define RTS_CM4F_EXC_RETURN_THREAD_PSP_BASIC 0xFFFFFFFD
#define RTS_CM4F_ARCH_STACK_ALIGNMENT        8u
#define RTS_CM4F_CONTROL_NPRIV_BIT           0x1
#define RTS_CM4F_CONTROL_SPSEL_BIT           0x2
#define RTS_CM4F_CONTROL_FPCA_BIT            0x4
#define RTS_CM4F_SVC_START_NUMBER            0
#define RTS_CM4F_SCB_ICSR_ADDRESS        0xE000ED04
#define RTS_CM4F_SCB_ICSR_PENDSVSET      0x10000000

#define RTS_CM4F_FRAME_R4_OFFSET                  0
#define RTS_CM4F_FRAME_R5_OFFSET                  4
#define RTS_CM4F_FRAME_R6_OFFSET                  8
#define RTS_CM4F_FRAME_R7_OFFSET                 12
#define RTS_CM4F_FRAME_R8_OFFSET                 16
#define RTS_CM4F_FRAME_R9_OFFSET                 20
#define RTS_CM4F_FRAME_R10_OFFSET                24
#define RTS_CM4F_FRAME_R11_OFFSET                28
#define RTS_CM4F_FRAME_R0_OFFSET                 32
#define RTS_CM4F_FRAME_R1_OFFSET                 36
#define RTS_CM4F_FRAME_R2_OFFSET                 40
#define RTS_CM4F_FRAME_R3_OFFSET                 44
#define RTS_CM4F_FRAME_R12_OFFSET                48
#define RTS_CM4F_FRAME_LR_OFFSET                 52
#define RTS_CM4F_FRAME_PC_OFFSET                 56
#define RTS_CM4F_FRAME_XPSR_OFFSET               60
#define RTS_CM4F_INITIAL_FRAME_SIZE_BYTES        64

#endif /* RTS_CORTEX_M4F_PORT_OFFSETS_H */
