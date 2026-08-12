# Redot 26.2 implementation report

This report closes the technical portion of the preflight plan for
`redotchestrator`. The original [preflight report](preflight-report.md) remains
an archival record of conditions before implementation; current gate status is
maintained in the repository-root `BLOCKERS.md` and `TODO.md`.

## Outcome

The latest stable Orchestrator 2.5 source now builds and runs as a Redot 26.2
GDExtension. The implementation keeps upstream resource/type identifiers for
compatibility, replaces upstream network/update identity, removes restricted
artwork, and adds deterministic all-platform release automation.

## Executed local evidence

- Official Redot editor: `26.2.stable.official.4f5b14aba`.
- Windows editor and template-release builds completed with MSVC 19.51.
- Integration runner: 42 passed, 0 failed, 0 crashed, 0 errored, 0 skipped.
- Native updater-security test: passed without compiler warnings.
- Official release-template run: exit 0.
- Fresh Windows export with embedded PCK: exit 0.
- Exported release DLL matches the built release DLL byte-for-byte.
- Deterministic-package tests: 4 passed.
- All GitHub Actions workflows pass actionlint 1.7.12.

Exact source, dependency, tool, and binary hashes are in `UPSTREAM_LOCK.md` and
the companion evidence workbook.

## Intentional compatibility boundary

The following remain upstream-compatible by design: `OScript`, `.os`, `.torch`,
registered native class names, the `orchestrator/` settings namespace, and the
`addons/orchestrator` installation path. Renaming them would require a separate
serialized-data and settings migration.

## External closure

The repository is technically ready to be posted as a fork. The first public
release remains conditional on Redot trademark clearance for the chosen name and
one green hosted all-platform matrix. The workflow enforces the latter by
withholding the combined release package when any descriptor platform artifact
is absent.
