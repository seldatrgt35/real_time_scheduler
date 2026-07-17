# Scheduler Policy Guide

Exactly one policy selector is one at compile time.

| Policy | Required task metadata | Ready ordering | Tie behavior |
| --- | --- | --- | --- |
| FP | valid application priority | highest numeric effective priority | FIFO; yield/slice rotate peers |
| RMS | period, `deadline <= period` | startup-derived FP rank; shortest period highest | equal periods share FIFO rank |
| EDF | relative deadline | earliest wrap-safe absolute deadline | release sequence/FIFO |

RMS ranks are recomputed after each startup task creation against the closed set
registered so far; previously registered tasks may move before start. The final
rank set is frozen when scheduling starts. Runtime mechanics, including classic
priority inheritance, are FP mechanics.

EDF refreshes release tick, absolute deadline, and sequence when a task becomes
ready after blocking. Yield rotates only equal-deadline peers. Idle is forced to
the tail. The timer-service task uses a one-tick relative deadline so queued
callbacks cannot be silently starved. EDF mutex waiters and inheritance remain
fixed-priority-based; deadline inheritance is not implemented. No policy offers
feasibility guarantees without external admission analysis.
