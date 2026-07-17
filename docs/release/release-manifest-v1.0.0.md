# v1.0.0 Release Manifest

## Release identity

- semantic version: `1.0.0`;
- source release state: software release candidate;
- intended candidate tag: `v1.0.0-rc1`;
- language baseline: strict C11;
- supported CPU count: exactly one;
- supported architecture/target: ARM Cortex-M4F / NXP S32K148;
- license: MIT.

The final `v1.0.0` stable tag is reserved until physical target evidence closes
the conditional acceptance item. This avoids representing an unexecuted target
gate as complete.

## Source artifacts

- public headers under `include/rts/`;
- portable private kernel under `kernel/`;
- deterministic host port under `ports/host/`;
- Cortex-M4F port under `ports/arm/cortex_m4f/`;
- S32K148 startup, linker, timer, power, and integration under
  `targets/nxp/s32k148/`;
- focused and stress tests under `tests/`;
- architecture, implementation, user, porting, and release documentation under
  `docs/`;
- release/static audit and layout probe under `tools/`;
- CI definition under `.github/workflows/ci.yml`.

## Reproducibility

The authoritative source archive is produced from the signed/annotated Git tag,
not committed as a binary into the repository. A release publisher shall create
SHA-256 checksums for the source archive, S32K148 ELF, map, binary/hex image, and
hardware evidence bundle.

## Conditional evidence

The candidate does not include physical-board traces, power measurements, or
hardware WCET results. Those omissions are release blockers for the final
stable tag and are tracked in `known-limitations.md` and the Sprint 13 final
acceptance review.
