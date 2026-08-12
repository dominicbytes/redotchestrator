# Preflight report: redotchestrator

> Archival pre-implementation record. The technical findings below describe the
> checkout before the port. See [implementation-report.md](implementation-report.md)
> and the repository-root `BLOCKERS.md` for current status.

## Gate summary

| Gate | Verdict | Meaning |
|---|---|---|
| Source candidate | **ADAPT** | Use the latest stable upstream `v2.5.stable` as the bounded port baseline; retain 2.3.7 only as a compatibility reference. |
| Implementation readiness | **BLOCKED** | Native dependencies/tooling are incomplete, the editor updater is unsafe for interactive Redot use, and no compiled Redot extension has loaded. |
| Public release | **NO-GO** | Branding permission, original artwork, installable binaries, platform tests, and package evidence are missing. |

The official downstream plugin name is exactly `redotchestrator`. No candidate third-party native code was built or executed during this inspection.

## Scope, date, and assumptions

- Inspection date: 2026-08-11
- Workspace: `D:\Claude Vault\redot\plugins\redotchestrator`
- Upstream: Crater Crash Studios' Godot Orchestrator
- Selected source: stable tag `v2.5.stable`, commit `775cc4549657199b385f0a16e4c5523aba8b5500`
- Compatibility reference: `v2.3.7.stable`, commit `a869d57801dfec9a282ee59f7487c29bc04dcc82`
- Target: Redot LTS 26.2, reporting Godot 4.5.2 as its GDExtension compatibility API
- Inspection boundary: repository/source/configuration review, primary-source web research, Redot MCP version/static validation, and artifact review only

The first port is assumed to preserve serialized/resource/API compatibility identifiers—such as `OScript`, `.os`, `.torch`, `orchestrator/`, native class names, and likely `res://addons/orchestrator`—unless a separate migration is designed and tested. The user-facing product name can change independently.

## Research questions

1. Is the checked-out source an identifiable, current, and appropriate baseline for Redot 26.2?
2. What prevents the checkout from building or loading as a Redot GDExtension today?
3. Which source, resource, update, test, and packaging surfaces carry the highest porting risk?
4. What may be reused under the source license, and what branding/artwork needs separate permission?
5. Which gates must pass before implementation, runtime testing, and public release?

## Repository and provenance snapshot

| Item | Observation | Assessment |
|---|---|---|
| Visible checkout | Shallow branch `2.3` at `a869d57801dfec9a282ee59f7487c29bc04dcc82` | Compatibility reference only; it is no longer the product baseline. |
| Target source | Quarantined exact `v2.5.stable` clone at `775cc4549657199b385f0a16e4c5523aba8b5500` | Latest published stable baseline, acquired without executing code. |
| Latest release | `v2.5.stable`, published 2026-07-04, targeting Godot 4.7/GDExtension v10 | Required product baseline by user decision. |
| Latest plugin digest | SHA-256 `3e4bab7811b89002627495973e1aa0fdf502bb8cec718a2693cce354d203efd7` | Download matched GitHub's published digest exactly; not a downstream build hash. |
| Compatibility package | `v2.3.7.stable`, SHA-256 `e5188c4e71a7c1ae6b1856b9c4bebfac6d1c84dcf39040ee5f6ddde1526fbc6f` | Reference only; not installed. |
| Language/build | C++20, CMake, SCons-assisted dependency flow, GDExtension | Native ABI/toolchain work is required. |
| Source volume | 227 headers, 213 C++ files, 83 XML docs, 56 `.torch` files, 47 test scenes | This is a substantial native editor/runtime port, not a small addon rename. |
| License | Apache-2.0 source | Reusable with license/notice obligations; artwork terms are separate. |
| Local status | Existing preflight documentation changes were present before this inspection | Preserved and updated; upstream source code was not ported. |

The `v2.5.stable` tag is lightweight and resolves directly to `775cc...`; immutable commit and release-asset hashes remain part of the downstream provenance record. The moving `2.5` branch has advanced beyond the release, so it is not silently substituted for the stable tag.

## Packaged addon inspection

`project/addons/orchestrator` currently contains the `.gdextension` descriptor, one dialogue scene/script, upstream logos, and editor icons. It contains none of the Windows, Linux, macOS, Android, or Web libraries named by the descriptor.

The exact official `v2.5.stable` plugin archive was downloaded to `.out`, matched GitHub's published SHA-256, passed archive-path validation, and was extracted only inside quarantine. It contains 16 upstream native files: Linux (4), macOS (2), Windows (2), Android (4), Web (2), and two iOS XCFramework slices. Its descriptor sets `compatibility_minimum="4.7"`; these are Godot 4.7 binaries and were deliberately not copied into the live addon.

Consequences:

- The source checkout is not installable or runnable as distributed.
- The missing files can be acquired from upstream, but no acquired upstream binary qualifies as a Redot 26.2 build.
- A GitHub source archive is not an acceptable release package.
- The eventual package needs both controlled build artifacts and an installable distribution branch/repository commit with tested platform libraries, license/copyright/modification records, permitted artwork, and checksums.
- A missing `plugin.cfg` is not itself a defect here: native GDExtensions can be discovered through their `.gdextension` descriptor. The absent libraries are the material blocker.

## Engine and native compatibility

### Verified lineage

Primary Redot source for `redot-26.2-stable` records Redot `26.2.0` with Godot compatibility version `4.5.2`. The GDExtension interface exposes that compatibility version through the Godot-version function and exposes Redot's own version separately. The project-local Redot MCP also reports Redot 26.2.

The official redot-cpp `redot-26.2-stable` tag resolves to `598ec78e86b2c240a023f6de13daba70f7de8610` and states that it was synchronized to Redot 26.2. This is the correct binding candidate to test; the installed build and successful compilation/runtime checks remain authoritative.

### Checkout inconsistencies

- `.gitmodules` names `master` branches for Godot and godot-cpp, but ordinary submodule initialization still checks out the immutable superproject gitlinks; the problem is that those pins are upstream/wrong for Redot and uninitialized, not that the current build automatically follows moving tips.
- The target 2.5 source still uses engine gitlink `4c311cbee68c0b66ff8ebb8b0defdd9979dd2a41`, which resolves to Godot 4.4-stable and is stale as a development-source reference.
- Its godot-cpp gitlink is `5ffd70e34d0ab87009a9f0ffa3361bc8f4b09731`, synchronized with Godot 4.7-stable.
- The quarantined target clone intentionally has both submodule directories uninitialized; the visible 2.3 checkout also has empty dependency directories.
- CMake attempts to initialize missing submodules and requires SCons, while no usable CMake/Ninja/SCons/C++ compiler toolchain was found on PATH in the inspected environment.
- In the 2.5 source, the `.gdextension` descriptor and test project require Godot 4.7 while `project/project.godot` still advertises Godot 4.6. The downstream Redot project must use validated Redot feature/API metadata instead of inheriting any of these labels blindly.

The latest product baseline is intentionally not the closest API match: it targets Godot 4.7/GDExtension v10 while Redot 26.2 exposes Godot 4.5.2 compatibility. The code includes a 38-file Godot compatibility layer, direct GDExtension-interface calls, and 4.7-specific branches. Compared with the 2.3.7 source, `v2.5.stable` changes 21 files under `src` (168 additions, 32 deletions) plus descriptor/platform metadata, including iOS. That bounded source delta is useful, but only a clean compile against redot-cpp 26.2 can establish the actual adaptation cost.

## Static validation and code observations

All three GDScript files parsed successfully through the Redot MCP:

- `project/addons/orchestrator/scenes/dialogue_message.gd`
- `tests/scenes/features/utils.gd`
- `tests/scenes/features/gdscript/call_instance_method.gd`

This proves only GDScript syntax compatibility. It does not validate the unbuilt native extension.

The bundled dialogue script also deserves targeted cleanup during implementation:

- `_unhandled_input()` disables player movement for any key event even when the message is not visibly active.
- `_current_tween` can be null before its first assignment but is dereferenced by input handling.
- A dictionary key is assigned to an integer selection without explicit validation.

These are bounded QA findings, not reasons to reject the upstream baseline.

## P0 editor-updater and supply-chain finding

The current updater is the most important pre-interactive-editor blocker:

- `VERSION` still identifies `Orchestrator` and points to Crater Crash Studios' GitHub release API/manifests.
- The editor checks for updates when it enters the tree and then hourly; installation is not silent and still requires user selection/confirmation.
- That one-confirmation installer can download a future upstream `-plugin.zip`, which may contain Godot rather than Redot native libraries.
- The implementation does not verify a controlled downstream digest/signature before installation.
- ZIP entries are written under `res://` without enforcing that every normalized path remains inside the intended addon directory, without size limits/staging/rollback, and without robust per-file failure recovery. Escape outside the addon is proven; escape outside the project root was not established.

`v2.5.stable` is currently the newest published release, so there is no newer package to offer today. The source still trusts the upstream Godot release channel, however; a future or malicious manifest could appear compatible through metadata even though its artifacts were not built against Redot. Disable or replace the updater before the first interactive Redot editor/editor-library test in a writable project. A replacement must constrain normalized paths, reject links/traversal/absolute paths, enforce platform/engine/channel identity, verify a controlled digest or signature, and use staged/rollback-safe writes. The updater was not shown to instantiate in a standalone runtime template.

## Resource and identity risk

The repository contains custom binary/text loaders, savers, parsers, and resource types for `.os` and `.torch`. The string `redotchestrator` does not yet occur in the inspected source, while upstream `Orchestrator`, `orchestrator`, `OScript`, and `torch` identities are widespread.

A global rename would combine three different jobs:

1. public product rebranding;
2. native type/settings/addon identity migration; and
3. serialized-resource migration.

The minimum-risk first port changes public-facing metadata and artwork but preserves compatibility identifiers. Any internal rename should be a separate migration with before/after fixtures and an explicit backward-compatibility policy.

## Test and CI assessment

- The suite has 42 scene tests, each with a corresponding `.out` expectation, plus 47 `.torch` resources.
- Individual scene-test runs receive `--quit-after` and the runner parallelizes up to four tests, but project import has no equivalent bound and the subprocess calls have no wall-clock timeout; the harness as a whole is not hang-bounded.
- If no engine path is supplied, it downloads a matching latest build from GitHub without an independently pinned checksum.
- Upstream CI runs integration tests only for the Linux x86_64 editor build; other advertised platforms are primarily build checks.
- GitHub Actions use version tags rather than immutable action commit SHAs; the emsdk setup clones a moving default branch; Python floats at `3.x`; and SCons is installed from PyPI without a hash lock.
- A downstream run without `--godot-binary` downloads and executes the latest matching Godot ZIP without an independently pinned digest. That could produce false-green Godot results for a Redot port.

The fixture inventory is valuable and should be reused, but the test project's 4.7 feature tag, engine acquisition flow, and platform coverage must be corrected before it becomes the downstream release oracle.

## Licensing, trademark, and artwork

- Retain the Apache-2.0 license copy, provide prominent modification notices, and retain pertinent existing notices. Apache-2.0 does not grant general trademark rights. Upstream has no `NOTICE` file, so do not invent a NOTICE-file requirement unless applicable notice material is later introduced.
- Remove the bundled rights-reserved Orchestrator logo files before publishing any downstream source fork—not only from binary release packages—unless explicit permission is obtained. The editor source directly loads those assets.
- Redot's official licenses/terms identify the Redot name and logo as trademarks. The user has selected `redotchestrator` as the official product name, but public use still needs written approval or a clearly applicable brand-use policy. This is a release gate, not a legal conclusion.
- `project/icon.svg` and `tests/icon.svg` are Godot-logo assets covered upstream as CC BY 4.0; replace them or provide compliant attribution. Record asset-by-asset provenance/license treatment for the remaining editor icons before redistribution.
- Use original downstream artwork. The current license file has no completed copyright statement, while the intended Asset Library channel requires license text and a copyright statement; add the appropriate downstream record without altering upstream authorship.
- The Redot Asset Library downloads a repository commit. Use a dedicated installable distribution branch/repository commit with the addon at the expected layout, tested native libraries, and local rights files, then perform a clean install from the exact submission URL. Build artifacts alone and the current nested source layout are insufficient.
- Deliberate lowercase `redotchestrator` styling may need manual Asset Library reviewer confirmation under its capitalization guidance.

An exact GitHub repository search returned no repository named `redotchestrator` on 2026-08-11. This is a narrow collision check only; it is not trademark clearance.

## Recommended primary references

### Chosen upstream baseline

- [Godot Orchestrator 2.5 branch](https://github.com/CraterCrash/godot-orchestrator/tree/2.5)
- [Godot Orchestrator v2.5.stable release](https://github.com/CraterCrash/godot-orchestrator/releases/tag/v2.5.stable)
- [Exact stable source commit](https://github.com/CraterCrash/godot-orchestrator/tree/775cc4549657199b385f0a16e4c5523aba8b5500)
- [2.3.7 compatibility reference](https://github.com/CraterCrash/godot-orchestrator/releases/tag/v2.3.7.stable)
- [Upstream documentation](https://docs.cratercrash.space/orchestrator)
- [Apache-2.0 source license](https://github.com/CraterCrash/godot-orchestrator/blob/775cc4549657199b385f0a16e4c5523aba8b5500/LICENSE)

### Redot engine and bindings

- [Redot 26.2 stable release](https://github.com/Redot-Engine/redot-engine/releases/tag/redot-26.2-stable)
- [Redot 26.2 version declaration](https://raw.githubusercontent.com/Redot-Engine/redot-engine/4f5b14abade2239104847d03d8f9056e4467cfcd/version.py)
- [Redot 26.2 GDExtension interface implementation](https://github.com/Redot-Engine/redot-engine/blob/4f5b14abade2239104847d03d8f9056e4467cfcd/core/extension/gdextension_interface.cpp)
- [Exact redot-cpp 26.2 commit](https://github.com/Redot-Engine/redot-cpp/tree/598ec78e86b2c240a023f6de13daba70f7de8610)

### Distribution and brand gates

- [Redot licenses and trademark notice](https://www.redotengine.org/licenses)
- [Redot terms](https://www.redotengine.org/terms)
- [Redot Asset Library submission requirements](https://docs.redotengine.org/community/asset_library/submitting_to_assetlib)

## Rejected candidates and non-findings

| Candidate or concern | Decision | Reason |
|---|---|---|
| Start from upstream moving `main`/2.5 head | Rejected | The user requested the latest version; the newest immutable stable release is `v2.5.stable`, while the branch has advanced beyond it. |
| Use 2.3.7 as the product baseline because it is closer to Redot | Rejected by product goal | It remains a valuable compatibility reference, but the user explicitly requires the latest stable feature set. |
| Install the acquired v2.5 upstream binaries in Redot | Rejected | Their descriptor requires Godot 4.7/GDExtension v10; they must be rebuilt against Redot 26.2. |
| Publish the repository source ZIP/current commit | Rejected | Required native libraries are absent and the nested development layout is not an installable Asset Library distribution. |
| Reuse the Orchestrator logo | Rejected pending permission | Artwork rights are not granted by the Apache source license. |
| Rename all internal identifiers immediately | Rejected for the first port | It unnecessarily combines compatibility migration with engine adaptation. |
| Treat missing `plugin.cfg` as a blocker | Rejected finding | The `.gdextension` descriptor is the relevant native entry point; missing libraries are the real issue. |
| Treat successful GDScript parsing as extension compatibility | Rejected claim | It does not load or exercise the native code. |
| Treat no GitHub name collision as trademark clearance | Rejected claim | Repository-name availability and trademark permission are different questions. |

## Adversarial evidence audit

The first six rows are labeled **SELF_REVIEW** passes. Three independent read-only critics then challenged native compatibility, supply-chain safety, and rights/identity; their reconciled results are retained as `AUD-RC-007` through `AUD-RC-009`. Material disagreements are not averaged away.

| ID | Lens | Challenged claim | Evidence | Missing proof | Impact | Verdict |
|---|---|---|---|---|---|---|
| AUD-RC-001 | Identity/provenance | `v2.5.stable` is the latest stable source actually acquired. | Latest-release API, lightweight tag, detached source clone, release package, and published digest converge on `775cc...` and SHA-256 `3e4bab...`. | Full history and downstream build provenance are absent. | Pin immutable commit/digest; retain 2.3.7 only as reference. | PASS |
| AUD-RC-002 | Feature/reference fit | Upstream implements the intended visual scripting/editor/runtime scope. | Source tree, docs, native registrations, custom resource loaders, test fixtures. | No Redot behavior exercised. | Keep full functional gate. | PASS |
| AUD-RC-003 | Redot/API compatibility | Latest Godot 4.7/GDExtension v10 source can be adapted to Redot 26.2. | Exact redot-cpp 26.2 exists; 2.3.7 provides comparison shims; latest delta is bounded but includes 4.7-specific source/descriptor behavior. | Clean compile, ABI hashes, native load, resources, editor, export, and iOS handling. | Allows a deliberate port plan, not binary reuse or readiness. | BLOCKED |
| AUD-RC-004 | License/brand | Apache-2.0 permits a public package under the selected name/artwork. | Source license is permissive; upstream and Redot both separate trademark/artwork rights. | Written name permission and original artwork. | Blocks public release. | BLOCKED |
| AUD-RC-005 | Maintenance/supply chain | A current upstream release/update path is safe to inherit. | Release is recent and maintained; the editor auto-checks upstream and exposes a user-confirmed installer with weak downstream validation. | Controlled feed, containment, digest/signature, staging/rollback policy. | Blocks interactive editor use. | BLOCKED |
| AUD-RC-006 | Unlicensed skeptic | The checkout can be advertised as a working plugin. | Descriptor exists, but every referenced native library and both dependency trees are absent. | Reproducible build and clean-install evidence. | Blocks implementation/release claims. | FAIL |
| AUD-RC-007 | Independent native/build critic | The existing tests or downloadable binaries justify readiness. | Upstream artifacts are 4.7-only; exact Redot bindings support a rebuild; empty/wrong live dependencies and Linux-only integration execution remain. | Compile/link, generated API/precision, native load, round trips, editor/export proof. | Upholds the split gate; downloaded binaries remain reference-only. | BLOCKED |
| AUD-RC-008 | Independent supply-chain critic | The updater automatically installs on first load and is the only acquisition concern. | It auto-checks but requires confirmation; installer lacks containment/digest/staging/rollback; tests fetch unverified latest Godot; CI inputs float. | Controlled Redot feed/binary, immutable CI inputs, provenance/attestation. | Keeps P0 before writable interactive editor use; broadens downstream CI gate. | BLOCKED |
| AUD-RC-009 | Independent rights/identity critic | Artwork cleanup can wait until binary packaging. | Rights-reserved logos are tracked and loaded by source; Godot/custom icon provenance and Asset Library distribution layout are incomplete. | Permission/original art, per-asset record, copyright/modification record, installable distribution commit, reviewer confirmation. | Blocks public fork/listing/release; supports preserving machine identifiers. | BLOCKED |
| AUD-RC-010 | User scope correction | The port should select the version already closest to Redot. | User explicitly requires the latest stable feature set; GitHub identifies `v2.5.stable` as latest. | Redot-specific build/adaptation evidence. | Supersedes the 2.3.7 implementation baseline and increases the API bridge scope. | PASS |

## Concrete implementation-plan changes

1. Put updater neutralization and archive-verification tests before the first interactive editor/editor-library run in a writable project.
2. Promote exact `v2.5.stable` source into the implementation worktree; keep 2.3.7 only as a comparison fixture.
3. Replace upstream Godot 4.7 bindings with immutable Redot 26.2/redot-cpp 26.2 pins and record the engine executable, generated API/interface, precision, architecture, compiler, CMake, and SCons hashes.
4. Compile before broad refactoring; classify each 4.7-to-Redot incompatibility and make the smallest Redot-specific change.
5. Preserve resource/type/settings identity while `.torch`/`.os` round-trip fixtures establish compatibility.
6. Align the test project with the shipped compatibility version, pin/verify the exact Redot executable, and add wall-clock timeouts to import and test subprocesses.
7. Pin immutable CI/actions/toolchain inputs and require platform tests for every latest-release target: Linux, macOS, Windows, Android, Web, and iOS.
8. Before any public fork, remove rights-reserved logos, resolve Godot/editor-icon provenance, record Apache modification/copyright obligations, and obtain Redot name/capitalization clearance.
9. Produce an Asset Library-compatible distribution commit and clean-install it from the exact submission URL.

## Unresolved evidence and success criteria

The preflight becomes implementation-ready only when:

- interactive editor use cannot install uncontrolled upstream Godot artifacts or write outside its addon directory, and partial failures cannot leave a mixed installation;
- exact Redot/redot-cpp/toolchain inputs are available and pinned;
- `v2.5.stable` native editor and release builds complete against redot-cpp 26.2 and load in bounded Redot runs;
- representative resource/runtime/editor fixtures pass without unexplained errors; and
- the current source-only packaging claim is replaced by tested artifacts.

Any public fork additionally requires removing or licensing rights-reserved upstream logos and resolving all retained-asset provenance. Public release further requires name/capitalization clearance, original permitted artwork, Apache modification/copyright records, tested platform packages, checksums, and an installable distribution commit.

## Final recommendation

**ADAPT latest stable `v2.5.stable`, but do not install its upstream Godot 4.7 binaries or open the current checkout interactively in a writable Redot editor project.** The exact latest source and package are now acquired and provenance-checked; 2.3.7 remains only a compatibility reference. The 4.7-to-Redot API bridge, editor updater, empty/mismatched live dependencies, missing Redot build/runtime proof, compatibility-sensitive resource identity, asset provenance, distribution layout, and public-branding rights are hard gates—not cleanup items.

The companion `source-of-truth.xlsx` records the evidence, decisions, QA results, and risks.
