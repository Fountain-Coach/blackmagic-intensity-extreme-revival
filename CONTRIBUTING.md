# Contributing: repair as a reproducible practice

You can help even without reverse-engineering experience. Report exact versions, test a supported source, identify a missing tool, document an error, or review the environmental claims.

## Evidence first

For a hardware observation include, when safe: device model, PCI or USB IDs, host model (not serial number), operating system version, driver and application versions, connection topology, exact error message, whether a known unprotected input was attached, date, and commands to reproduce it.

Distinguish clearly:
- **Observed:** output directly seen on hardware, in logs, or from an executable test.
- **Derived:** static analysis, disassembly or a model interpretation.
- **Hypothesis:** a possible explanation that has not been tested.

Do not report a successful capture merely because an application launches or a driver attaches.

## Safe operating practices

- Work on owned or otherwise authorized devices and software.
- Prefer read-only diagnostics and disposable copies of binaries.
- Do not flash firmware, erase storage, modify EEPROM, or replace kernel components without a separate rollback and recovery plan.
- Do not upload proprietary binaries, firmware, SDK archives, full decompilations, or copied vendor source.
- Remove credentials, network addresses not needed for reproduction, personal media, machine serials, and identifying logs.
- Respect copyright and protection on incoming media; use test patterns or other authorized signals.
- Document affected host and software versions rather than proposing unsupported blanket fixes.

## Project decisions

This is an independent repair project. Our forward video pipeline must not depend on Syphon or obsolete application bridges. Platform independence and replaceable backends are primary design requirements. A new kernel driver is a **research objective**, not a completed feature.

Scope proposals should include a small verifiable acceptance test. See [investigation](docs/investigation.md) and [sustainability ledger](docs/sustainability.md).

## Conduct

Welcome careful questioning, evidence-based disagreement, corrections, and people with different technical backgrounds. Avoid harassment, personal attacks, and unsupported claims about contributors' competence. For potentially sensitive findings, contact a maintainer privately before posting operational details.

The repository is not affiliated with Blackmagic Design.
