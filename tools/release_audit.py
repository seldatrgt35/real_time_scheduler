#!/usr/bin/env python3
"""Deterministic source-level release checks for RTS v1.0.0."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def production_files() -> list[Path]:
    result: list[Path] = []
    for directory in ("kernel", "ports", "targets"):
        result.extend(
            path
            for path in (ROOT / directory).rglob("*")
            if path.suffix.lower() in {".c", ".h", ".s"}
        )
    return sorted(result)


def main() -> int:
    failures: list[str] = []
    checks: dict[str, object] = {}
    files = production_files()
    combined = "\n".join(read(path) for path in files)

    version = read(ROOT / "include/rts/rts_version.h")
    checks["version_1_0_0"] = 'RTS_VERSION_STRING "1.0.0"' in version

    heap_calls = sorted(
        str(path.relative_to(ROOT))
        for path in files
        if re.search(r"\b(?:malloc|calloc|realloc|free)\s*\(", read(path))
    )
    checks["heap_call_files"] = heap_calls

    core_files = [
        ROOT / "kernel/scheduler_select.c",
        ROOT / "kernel/scheduler_start.c",
        ROOT / "kernel/switch_plan.c",
        ROOT / "kernel/task_yield.c",
        ROOT / "kernel/task_delay.c",
        ROOT / "kernel/time.c",
    ]
    leakage_pattern = re.compile(
        r"\brts_ready_|\bready_set\b|absolute_deadline|RTS_POLICY_"
    )
    policy_leaks = sorted(
        str(path.relative_to(ROOT))
        for path in core_files
        if leakage_pattern.search(read(path))
    )
    checks["scheduler_policy_leak_files"] = policy_leaks

    task_header = read(ROOT / "kernel/task_internal.h")
    checks["saved_sp_offset_asserted"] = bool(
        re.search(
            r"offsetof\(struct rts_task, saved_stack_pointer\)\s*==\s*0",
            task_header,
        )
    )
    checks["legacy_reschedule_symbol_absent"] = (
        "rts_port_request_context_switch" not in combined
    )

    configs = sorted(ROOT.glob("tests/config*/rts_config.h")) + sorted(
        ROOT.glob("targets/nxp/s32k148/config*/rts_config.h")
    )
    bad_configs: list[str] = []
    for config in configs:
        text = read(config)
        selected = sum(
            int(match.group(1))
            for name in (
                "RTS_POLICY_FIXED_PRIORITY",
                "RTS_POLICY_RMS",
                "RTS_POLICY_EDF",
            )
            for match in [
                re.search(rf"#define\s+{name}\s+([01])\b", text)
            ]
            if match
        )
        cpu = re.search(r"#define\s+RTS_CPU_COUNT\s+1u?\b", text)
        if selected != 1 or cpu is None:
            bad_configs.append(str(config.relative_to(ROOT)))
    checks["invalid_configuration_files"] = bad_configs

    public_headers = sorted((ROOT / "include/rts").glob("*.h"))
    private_include_pattern = re.compile(
        r'#include\s+"(?:kernel/|scheduler_|task_internal|port\.h)'
    )
    public_leaks = [
        str(path.relative_to(ROOT))
        for path in public_headers
        if private_include_pattern.search(read(path))
    ]
    checks["public_private_include_leaks"] = public_leaks

    handler_sources = {
        "Reset_Handler": (
            "targets/nxp/s32k148/startup/startup_s32k148.S",
            r"^\s*Reset_Handler:\s*$",
        ),
        "SVC_Handler": (
            "ports/arm/cortex_m4f/port_asm.S",
            r"^\s*SVC_Handler:\s*$",
        ),
        "PendSV_Handler": (
            "ports/arm/cortex_m4f/port_asm.S",
            r"^\s*PendSV_Handler:\s*$",
        ),
        "SysTick_Handler": (
            "targets/nxp/s32k148/target_tick.c",
            r"\bvoid\s+SysTick_Handler\s*\(",
        ),
    }
    missing_handlers = [
        symbol
        for symbol, (relative, pattern) in handler_sources.items()
        if not re.search(pattern, read(ROOT / relative), re.M)
    ]
    checks["missing_handler_definitions"] = missing_handlers

    if not checks["version_1_0_0"]:
        failures.append("release version is not 1.0.0")
    if heap_calls:
        failures.append("dynamic-allocation calls found")
    if policy_leaks:
        failures.append("scheduler-core policy leakage found")
    if not checks["saved_sp_offset_asserted"]:
        failures.append("saved-SP offset assertion missing")
    if not checks["legacy_reschedule_symbol_absent"]:
        failures.append("legacy context-switch request symbol remains")
    if bad_configs:
        failures.append("invalid CPU/policy configuration")
    if public_leaks:
        failures.append("private include leaked into public API")
    if missing_handlers:
        failures.append("required target handler definition missing")

    report = {
        "release": "v1.0.0",
        "result": "PASS" if not failures else "FAIL",
        "checks": checks,
        "failures": failures,
    }
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
