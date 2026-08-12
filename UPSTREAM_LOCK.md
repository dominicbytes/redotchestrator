# Immutable source and toolchain lock

## Source lineage

| Field | Locked value |
|---|---|
| Downstream name | `redotchestrator` |
| Downstream branch | `redot-26.2` |
| Upstream repository | `https://github.com/CraterCrash/godot-orchestrator` |
| Upstream release | `v2.5.stable` |
| Upstream source commit | `775cc4549657199b385f0a16e4c5523aba8b5500` |
| Upstream release ZIP SHA-256 | `3e4bab7811b89002627495973e1aa0fdf502bb8cec718a2693cce354d203efd7` |
| Compatibility reference only | `v2.3.7.stable` / `a869d57801dfec9a282ee59f7487c29bc04dcc82` |
| Source license | Apache-2.0; upstream artwork rights treated separately |

The repository retains the full upstream history and tags. The 2.3 line is not
the product baseline; it was used only to compare older 4.5-compatible code.

## Redot dependencies

| Input | Exact value |
|---|---|
| Redot release | `redot-26.2-stable` |
| Redot engine commit/gitlink | `4f5b14abade2239104847d03d8f9056e4467cfcd` |
| redot-cpp commit/gitlink | `598ec78e86b2c240a023f6de13daba70f7de8610` |
| Runtime version tuple | `26.2.0 stable official` |
| GDExtension compatibility minimum | `4.5.2` |
| redot-cpp `extension_api.json` SHA-256 | `453a0cc128bb58333a001f7f43573a5961d973fb7b151af43139869f22d5915c` |
| redot-cpp `gdextension_interface.h` SHA-256 | `4cd695e86b92e2bf4e60bbe19ce137faf41205da1cf94f29e069afec0f7bf320` |

`.gitmodules` points both submodules to the official Redot-Engine organization;
the superproject gitlinks above are the authority. The engine checkout is an
optional source/reference input; normal GDExtension builds use redot-cpp.

## Locally verified Windows toolchain

| Input or output | Version or SHA-256 |
|---|---|
| Official Redot editor console executable | `5633d02a28a73514084df6a60ffe01fabdbbb9ac5e28fdfd590ed47277f51989` |
| Official Redot editor executable | `10dfbefc273536f0c65903d6429e531c30133820bf0fa842f2d6e6fb81db1d42` |
| Official Redot release template | `723097ce13cc7c28202ce67337f167d15482ea57147be33ea1c5d7818a45b137` |
| MSVC | `19.51.36248.0` (Visual Studio Build Tools 2026) |
| CMake | `4.3.1-msvc1` |
| Ninja | Visual Studio bundled Ninja |
| Python | `3.14.0` |
| Windows editor DLL | `b8e0b3d73cf7e065739e6f0bbec9d821334cdbb583b37b514cf9f22b9f398de8` |
| Windows release DLL | `413eaeee5b63e65654647e22500d81260b0cf68263f43ee96656c773b7bfc13d` |
| Exported Windows release DLL | Same as the source release DLL |
| Exported Windows demo executable | `9b518341f2ebf8b12ac2f61700033fad6653b3da7fe592bf1a96c14850a62d34` |

## CI acquisition lock

The Linux editor job downloads only the official Redot 26.2 archive at:

```text
https://github.com/Redot-Engine/redot-engine/releases/download/redot-26.2-stable/Redot_v26.2-stable_linux_x64.zip
```

Its required SHA-256 is
`f474d890806c41af15513cf5a8600243e241882e11b68dbb95660e3465b5b1e4`.
Every external GitHub Action is pinned to a full commit SHA, and the Emscripten
SDK is pinned to commit `d49219d03a41cd12f95a33ba84273c20d41fd350`
(`4.0.11`).

## Release identity

- VERSION tag: `v2.5.stable`
- Archive: `redotchestrator-v2.5.stable-plugin.zip`
- Archive root: `addons/orchestrator/`
- Manifest asset: `release_manifests.json`
- Required compatibility field: `v26.2.0`

The release workflow rejects a tag that does not match `VERSION`, an incomplete
descriptor library set, removed artwork, symlinks, editor caches, and native
development artifacts.
