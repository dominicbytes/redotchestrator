# redotchestrator

`redotchestrator` is an unofficial Redot 26.2 port of Crater Crash Studios'
[Orchestrator](https://github.com/CraterCrash/godot-orchestrator) visual-scripting
plugin. This fork starts from the latest stable upstream release, `v2.5.stable`,
and preserves its graph, resource, runtime, debugger, dialogue, and editor
workflows while replacing the Godot 4.7 integration with Redot 26.2 bindings.

## Status

The source port is implemented and locally validated on Windows against the
official Redot 26.2 editor and release template. The first hosted GitHub Actions
matrix also passed on commit `1ee46ff31a6a3e93ebb8c1d82897ee9c3d77c543`:
all Linux, macOS, Windows, Android, Web, and iOS targets built successfully and
the combined plugin and demo artifacts were assembled.

| Item | Locked value or result |
|---|---|
| Upstream baseline | `v2.5.stable` / `775cc4549657199b385f0a16e4c5523aba8b5500` |
| Redot baseline | `redot-26.2-stable` / `4f5b14abade2239104847d03d8f9056e4467cfcd` |
| redot-cpp baseline | `598ec78e86b2c240a023f6de13daba70f7de8610` |
| GDExtension compatibility minimum | `4.5.2` |
| Windows integration suite | 42 passed, 0 failed, 0 crashed, 0 errored |
| Native updater-security suite | Passed |
| Windows release/export smoke | Passed; exported process exited 0 |
| Hosted all-platform matrix | Passed; 19 jobs succeeded in [run 31560191202](https://github.com/dominicbytes/redotchestrator/actions/runs/31560191202) |
| Hosted universal plugin artifact | Passed strict local package validation after download |

The source fork is public at
[`dominicbytes/redotchestrator`](https://github.com/dominicbytes/redotchestrator).
A tagged release still needs confirmation that the selected name may use the
Redot trademark. The project does not include the Redot logo and does not claim
affiliation or endorsement. See [BLOCKERS.md](BLOCKERS.md).

## Install

For an end-user installation, use a release ZIP produced by this repository's
tag workflow. Extract it into a Redot 26.2 project so the descriptor is at:

```text
res://addons/orchestrator/orchestrator.gdextension
```

The release ZIP deliberately retains `addons/orchestrator`, `OScript`, `.os`,
`.torch`, and the `orchestrator/` settings namespace. Those are compatibility
identifiers used by existing resources; the public product name is
`redotchestrator`.

Do not substitute binaries from the upstream Godot release. They target Godot
4.7 and are not Redot 26.2 builds.

## Build from source

Clone with submodules, then use the preset for the intended target. CMake 3.20+
and a C++20 compiler are required.

```powershell
git clone --recurse-submodules https://github.com/dominicbytes/redotchestrator.git
cd redotchestrator
cmake --preset windows-editor
cmake --build build/windows-editor --target orchestrator --parallel 4
```

Run Windows commands from an x64 Visual Studio developer prompt. Equivalent
presets exist for Linux, macOS, Android, Web, and iOS. The immutable engine and
binding revisions are recorded in [UPSTREAM_LOCK.md](UPSTREAM_LOCK.md).

## Verify

Set `REDOT_BIN` to the exact Redot 26.2 editor, then run the integration suite:

```powershell
python tests/run_integration_tests.py --redot-binary "$env:REDOT_BIN" --no-color
```

The test runner never downloads or falls back to Godot. It bounds both import
and per-scene execution. Native updater checks are enabled with
`REDOTCHESTRATOR_BUILD_TESTS=ON`, and deterministic release-package checks live
in `tests/test_package_release.py`.

## Updates and releases

The editor updater accepts only assets from
`dominicbytes/redotchestrator` whose tag, filename, size, GitHub-provided digest,
and controlled manifest SHA-256 all agree. ZIP entries must remain under
`addons/orchestrator`; unsafe, duplicate, ambiguous, or oversized paths are
rejected. Installation uses a staging directory, backup, atomic directory
activation, and rollback on activation failure.

Release tags must match `VERSION` (currently `v2.5.stable`). The release script
builds a deterministic `redotchestrator-v2.5.stable-plugin.zip`, verifies every
library referenced by the descriptor, rejects development artifacts and removed
artwork, and emits the manifest consumed by the updater.

## Provenance and license

The upstream source remains Apache-2.0 with its original authorship preserved.
Downstream changes and original artwork are Copyright 2026 Dominic Bytes and
licensed under Apache-2.0. See [MODIFICATIONS.md](MODIFICATIONS.md),
[ASSET_LICENSES.md](ASSET_LICENSES.md), [AUTHORS.md](AUTHORS.md), and
[LICENSE](LICENSE).

`redotchestrator` is not affiliated with, sponsored by, or endorsed by Crater Crash Studios LLC. Apache-2.0 does not grant
trademark rights.

## Notes

I vibe coded this in GPT Sol 5.6. Use at your own risk. Actual programmers are welcome to submit PR's and feedback.

## About Dominic Bytes

Greetings! I am Dominic Bytes, the synth walker. I hail from the distant future. Where brains occupy robot bodies, time travel is a trip to the corner store, and the neon glow of our attire is powered by the light of our souls. Join me on a 1.21 gigawatt powered journey of chill vibes with gaming, anime, movies, and more!

- [Website](https://dominicbytes.carrd.co/)
- [X](https://x.com/DominicBytes)
- [Twitch](https://www.twitch.tv/dominicbytes)
- [YouTube](http://www.youtube.com/@DominicBytes)
- [Kick](https://kick.com/dominicbytes)
