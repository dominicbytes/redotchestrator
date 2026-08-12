# Implementation plan and completion record

- Official downstream name: `redotchestrator`
- Source baseline: `v2.5.stable` / `775cc4549657199b385f0a16e4c5523aba8b5500`
- Target: Redot LTS 26.2 / GDExtension compatibility API 4.5.2
- Local implementation status: complete
- Fork status: ready for owner review and first hosted CI run
- Public release status: externally gated by name clearance and hosted CI

## Assumptions retained

- Public branding changes do not rename compatibility-sensitive `OScript`,
  `.os`, `.torch`, `orchestrator/`, native class names, or
  `res://addons/orchestrator`.
- Upstream Godot 4.7 binaries are reference artifacts only and are never shipped
  as Redot binaries.
- Hosted CI is the authority for platforms unavailable on the Windows
  workstation; packaging fails closed if any descriptor target is absent.

## Completed sequence

1. **Source and dependency lock — complete.** The working branch starts at exact
   upstream `v2.5.stable`; Redot engine and redot-cpp use immutable 26.2 gitlinks.
2. **4.7-to-Redot bridge — complete.** GDExtension interfaces, API metadata,
   resource-format and script-language lifecycle, parser ownership, and VM stack
   behavior were adapted for Redot 26.2.
3. **Updater safety — complete.** The upstream channel was replaced with the
   fork channel. Tag/name/URL/digest/size checks, path containment, collision and
   device-name rejection, size limits, staging, backup, activation, and rollback
   are implemented and covered by a native CTest target.
4. **Functional fixtures — complete locally.** The official Redot editor passes
   all 42 upstream-derived scene tests, including errors, resources, calls,
   coroutines, signals, containers, control flow, lifecycle, and recursion.
5. **Release path — complete locally.** Windows editor/release DLLs build cleanly;
   the editor imports the demo; the official release template loads and exits;
   an embedded-PCK Windows export launches and exits 0.
6. **Identity and rights — complete in source.** Restricted upstream logos and
   Godot-logo project icons were removed; original SVG artwork, asset provenance,
   Apache modification records, and non-affiliation text were added.
7. **Cross-platform automation — complete in repository.** Linux, macOS, Windows,
   Android, Web, and iOS jobs build the descriptor matrix from pinned inputs.
   The tag job combines artifacts only after every platform succeeds, runs a
   strict deterministic packager, updates the controlled manifest, and publishes
   only from `dominicbytes/redotchestrator`.

## Verification record

| Gate | Result |
|---|---|
| Official Redot runtime identity | PASS — `26.2.0 stable official` |
| Windows editor and release builds | PASS |
| Full integration runner | PASS — 42/42 |
| Native updater-security CTest | PASS |
| Deterministic package unit tests | PASS — 4/4 |
| Release-template runtime | PASS |
| Embedded-PCK export runtime | PASS — exit 0 |
| Exported/source release DLL equality | PASS — identical SHA-256 |
| GitHub workflow syntax/static analysis | PASS — actionlint 1.7.12 |
| Hosted non-Windows platform execution | PENDING — first fork CI |
| Public-name permission | BLOCKED EXTERNALLY — Redot trademark clearance |

## Remaining publication sequence

1. Obtain written permission or identify an applicable published policy for the
   `redotchestrator` name.
2. With owner authorization, create/push `dominicbytes/redotchestrator` and run
   the full hosted matrix.
3. Repair any host-specific failure; do not weaken the package validator or
   remove descriptor targets to force a green result.
4. Tag the green commit `v2.5.stable` and verify the release ZIP, manifest,
   checksums, updater listing, and clean install from the exact public URL.

## Completion interpretation

The port's source implementation and local Redot 26.2 verification are complete.
Compilation alone was not used as acceptance: parser/resource/runtime fixtures,
the editor library, the release library, updater policy, real export, and
packaging logic all have executable evidence. Remote platform runs and trademark
permission are external release gates that cannot be completed before the fork
exists or on behalf of the trademark owner.
