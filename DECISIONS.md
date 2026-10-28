# Architecture decisions

## ADR-0009: Establish the Orchestrator preflight boundary

- Date: 2026-08-09
- Status: superseded by ADR-0015 for the source baseline; scope remains valid.
- Context: the porting specification requires reproducible source, dependencies, API evidence, tests, and legal notices.
- Decision: evaluate the visual scripting language, graph editor, runtime, resources, debugger, dialogue support, and serialized-resource compatibility without claiming an implemented port.
- Consequence: the source snapshot stays intact until the preflight gate is explicit.

## ADR-0010: Use `redotchestrator` as the official downstream name

- Date: 2026-08-11
- Status: accepted (user decision).
- Decision: the public plugin name is exactly lowercase `redotchestrator`.
- Consequence: downstream-facing documentation, artwork, release artifacts, and update metadata must use this name once implementation begins.
- Caveat: public release remains subject to Redot trademark/brand clearance.

## ADR-0011: Preserve compatibility identifiers in the first port

- Date: 2026-08-11
- Status: accepted and implemented.
- Decision: initially preserve `OScript`, `.os`, `.torch`, the `orchestrator/` settings namespace, native class names, and likely `res://addons/orchestrator`.
- Rationale: these identifiers participate in serialization, resource loading, settings, and native ABI registration; renaming them is a data migration, not a cosmetic rebrand.
- Alternative: globally rename all Orchestrator identifiers during the port.
- Consequence: public branding can be `redotchestrator` while compatibility identity remains stable and is migrated only under a separate tested decision.

## ADR-0012: Adapt upstream `v2.3.7.stable`

- Date: 2026-08-11
- Status: superseded by ADR-0015 after the user clarified the product goal.
- Decision: use `a869d57801dfec9a282ee59f7487c29bc04dcc82`, the commit resolved by `v2.3.7.stable`, as the source baseline.
- Rationale: it is the current Godot 4.5-targeted upstream release, matching Redot 26.2's Godot 4.5.2 compatibility API more closely than upstream main.
- Alternative: start from current main or an unpinned source archive.
- Consequence: newer upstream changes require explicit classification and backport evidence.

## ADR-0013: Neutralize the upstream updater before interactive editor validation

- Date: 2026-08-11
- Status: implemented.
- Decision: disable or replace the upstream updater before any interactive Redot editor/editor-library test in a writable project.
- Rationale: its current feed, archive extraction, and verification model exposes a user-confirmed installer for incompatible Godot artifacts and lacks a safe downstream trust boundary.
- Consequence: editor fixture testing began only after the fork-only, verified, staged updater path was in place.

## ADR-0014: Use a dedicated installable release package

- Date: 2026-08-11
- Status: accepted and implemented as a deterministic tag artifact.
- Decision: publish a strictly validated release ZIP containing the addon, every descriptor library, and required rights files at the expected `addons/orchestrator` layout.
- Rationale: the development repository contains source and per-platform build outputs, while users need one complete installable package.
- Consequence: the release job waits for every platform, rejects incomplete distributions, and produces an Asset Library-compatible archive rather than treating a source ZIP as the plugin.

## ADR-0015: Port the latest stable upstream release

- Date: 2026-08-11
- Status: accepted (user decision).
- Decision: use `v2.5.stable` at `775cc4549657199b385f0a16e4c5523aba8b5500` as the implementation baseline; retain `v2.3.7.stable` only as a Redot-near compatibility reference.
- Rationale: the product goal is to port current Orchestrator functionality, not select the version already closest to Redot's inherited API.
- Alternative: ship the Godot 4.5-targeted 2.3 line because it requires less adaptation.
- Consequence: the port must deliberately bridge Godot 4.7/GDExtension v10 source to Redot 26.2's Godot 4.5.2 compatibility API.

## ADR-0016: Do not install upstream Godot binaries as Redot binaries

- Date: 2026-08-11
- Status: accepted safety decision.
- Decision: keep the checksum-verified `v2.5.stable` binaries in `.out` as reference fixtures only and rebuild every claimed platform artifact against Redot.
- Rationale: the upstream descriptor requires Godot 4.7, and the binaries were linked against upstream Godot bindings rather than redot-cpp 26.2.
- Consequence: acquired Godot binaries remain quarantined; Windows binaries are locally rebuilt and all release platforms are rebuilt by CI from the pinned Redot bindings.

## ADR-0017: Define completion as full operation on Redot 26.2

- Date: 2026-08-11
- Status: implemented and verified locally and in hosted platform CI.
- Decision: port every applicable `v2.5.stable` workflow so `redotchestrator` clean-installs, enables, authors and persists graphs, executes and debugs them, integrates with the editor, updates safely from a controlled Redot channel, and exports every claimed target on Redot 26.2.
- Rationale: the requested outcome is a working latest-version plugin on Redot 26.2, not merely acquired binaries, a successful compile, or a native-library load.
- Alternative: stop after compilation and representative smoke tests.
- Consequence: every applicable upstream feature needs a traceable fixture and passing result; an omitted or untested feature blocks completion unless the user explicitly accepts it as a scoped exception. The first hosted matrix passed with 19 successful jobs in run `31560191202`.

## ADR-0018: Separate Redot product version from inherited API compatibility

- Date: 2026-08-11
- Status: accepted and implemented.
- Decision: present support as Redot 26.2 while setting the GDExtension descriptor minimum to the inherited 4.5.2 compatibility API.
- Rationale: Redot reports runtime version `26.2.0`, but its GDExtension schema derives from Godot 4.5.2; conflating the two would either mislabel the product or make the descriptor unloadable.
- Consequence: release manifests use `v26.2.0`, projects retain the `4.5` feature tag, and the descriptor uses `compatibility_minimum="4.5.2"`.

## ADR-0019: Fail closed on update metadata and installation

- Date: 2026-08-11
- Status: accepted and implemented.
- Decision: require repository, release tag, asset filename, size, GitHub digest, controlled manifest SHA-256, and Redot compatibility to agree before downloading or installing.
- Rationale: repository-only URL filtering does not prevent tag/asset confusion, corrupted downloads, path traversal, or partial replacement.
- Consequence: unsafe metadata is omitted; unsafe ZIP entries are rejected; installation stages beside the addon, backs up the old directory, activates by rename, and rolls back on activation failure.

- Addendum (2026-10-19): self-update refuses pre-existing symlinks or junctions
  between the project root and addon, staging or backup directories; projects
  with a linked `addons` directory must update manually. These checks are defence
  in depth against accidental or pre-existing links. An active local process
  running as the same user with project write access can already replace addon
  binaries directly, so mid-install directory or junction substitution by that
  process is outside the updater threat model. Windows staging activation keeps
  its directory handle through the rename; portable checks do not promise
  protection against an active same-user attacker. The interruption exactly
  between the backup and activation renames remains a manual-recovery limitation.

## ADR-0020: Bound Windows VM recursion without changing normal execution

- Date: 2026-08-11
- Status: accepted and implemented.
- Decision: use heap-backed VM frames for large/deep invocations and reject recursion beyond 112 frames on MSVC.
- Rationale: the upstream VM frame is large enough to exhaust the smaller Windows thread stack near the upstream recursion fixture, while a global compiler exception/stack policy would have broader side effects.
- Consequence: the upstream depth-100 recursion fixture passes; excessive recursion fails deterministically instead of overflowing the process stack.

## ADR-0021: Package only after the complete platform matrix

- Date: 2026-08-11
- Status: accepted and implemented.
- Decision: assemble release artifacts only after Linux, macOS, Windows, Android, Web, and iOS jobs succeed, then validate the descriptor against the combined distribution.
- Rationale: a syntactically valid archive can still be unusable when one descriptor library is absent.
- Consequence: `tools/package_release.py` fails closed on missing targets, development artifacts, symlinks, cache data, removed artwork, missing rights files, or a tag/version mismatch and emits a deterministic ZIP and updater manifest.

## ADR-0022: Use original downstream artwork

- Date: 2026-08-11
- Status: accepted and implemented.
- Decision: remove the separately restricted upstream Orchestrator logos and Godot-logo project icons and replace them with original SVG artwork that does not reproduce the Redot or Godot marks.
- Rationale: Apache-2.0 source rights did not establish permission for every upstream brand asset.
- Consequence: all redistributed visual sources are listed in `ASSET_LICENSES.md`; public use of the word `Redot` remains a separate trademark gate.

## ADR-0023: Share the 26.2 binding baseline with Redot 26.3

- Date: 2026-10-19.
- Status: implemented; official Windows editor shutdown remains an engine gate.
- Decision: retain redot-cpp `9e7161bd`, its 4.5.2 API and existing script-instance
  structures. Patch only the build-tree Variant cleanup table lookup to delegate
  newer, unknown types to the engine destructor. Keep both GDExtension API bounds
  at 4.5.2. Use an explicit Redot manifest range from 26.2.0 through 26.3.0 and
  bump the plugin maintenance version so older installations can see the update.
- Rationale: authoritative stable-engine dumps preserve every existing class
  method/hash, virtual signature, interface function and native structure layout.
  The additive Struct type does not change Variant storage, but it exposes an
  out-of-bounds cleanup table lookup in the old bindings. A reference-lifetime
  regression fails with the old DLL and passes with the guarded build.
- Alternatives: a 26.3 custom API would raise the binding runtime minimum and
  require typed Struct integration, breaking the shared 26.2 build. The official
  redot-cpp default branch still points to the 26.2 tag, so there is no newer
  official binding revision to adopt.
- Consequences: existing graphs keep their API/serialization identifiers; this
  update does not add typed Struct authoring or persistence. The updater queries
  the Redot product version rather than its inherited Godot version, preserves
  legacy same-minor manifests, validates explicit bounds, and omits incompatible
  releases from notifications. The new Linux 26.3 job tests the same editor
  artifact used by 26.2. Engine shutdown failures stay visible; no crash or leak
  suppression is added to CI.

## ADR-0024: Reserve downstream maintenance versions

- Date: 2026-10-19.
- Status: accepted and implemented.
- Decision: use `v2.5.200.stable` for the next downstream release and reserve
  maintenance numbers 200 through 255 for this fork's 2.5 line. Preserve all
  inherited upstream tags. The package is
  `redotchestrator-v2.5.200.stable-plugin.zip`.
- Rationale: `v2.5.stable` is inherited and upstream's 2.5.1 release line must
  remain available for future imports. A high downstream maintenance number
  avoids that line while remaining within the one-byte maintenance encoding
  in `VERSION_HEX`. No numeric reservation can prohibit a future upstream
  choice; verify tag availability again before each release.
- Parser evaluation: `tools/package_release.py` derives the tag from major,
  minor, nonzero maintenance and status; `.github/workflows/runner.yml` derives
  the archive name directly from the tag. Both the package validator and updater
  accept only numeric version components and `stable`, `rcN` or `devN` status.
  `OrchestratorVersion` parses and orders maintenance numerically and recognizes
  `stable` for normal update notifications. This choice requires no parser change.
- Alternatives: downstream status suffixes or extra dot components are rejected
  by the validators; `version_build` and `version_module_config` affect display
  identity but do not enter release tags or updater comparisons. A small
  maintenance increment risks colliding with upstream's next maintenance release.
- Consequences: keep `VERSION`, release examples, lock metadata and package
  expectations aligned. Re-evaluate the reservation before exhausting it or
  merging an upstream release that uses the same number. Do not move an upstream
  tag to release downstream code.
