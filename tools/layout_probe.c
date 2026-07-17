/*
 * Compile-only release probe.  This file intentionally has no executable
 * behavior; Clang's -fdump-record-layouts option uses these declarations to
 * report the private ABI sizes for release evidence.
 */
#include "scheduler_internal.h"
#include "timer_internal.h"

struct rts_task rts_layout_probe_task;
rts_kernel_state_t rts_layout_probe_kernel;
struct rts_timer rts_layout_probe_timer;
rts_cpu_local_state_t rts_layout_probe_cpu_local;

unsigned char rts_layout_size_task[sizeof(struct rts_task)];
unsigned char rts_layout_size_kernel[sizeof(rts_kernel_state_t)];
unsigned char rts_layout_size_timer[sizeof(struct rts_timer)];
unsigned char rts_layout_size_cpu_local[sizeof(rts_cpu_local_state_t)];
