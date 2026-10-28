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
| redot-cpp commit/gitlink | `9e7161bd7a3572a36ffa93109e73169861e9f834` |
| Runtime version tuple | `26.2.0 stable official` |
| GDExtension compatibility minimum | `4.5.2` |
| redot-cpp `extension_api.json` SHA-256 | `453a0cc128bb58333a001f7f43573a5961d973fb7b151af43139869f22d5915c` |
| redot-cpp `gdextension_interface.h` SHA-256 | `4cd695e86b92e2bf4e60bbe19ce137faf41205da1cf94f29e069afec0f7bf320` |

`.gitmodules` points the binding to `https://github.com/dominicbytes/redot-cpp`
and the optional engine reference to the official Redot-Engine organization;
the superproject gitlinks above are the authority. The engine checkout is an
optional source/reference input; normal GDExtension builds use redot-cpp.

## Original 26.2 Windows verification

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

The baseline Linux editor job downloads the official Redot 26.2 archive at:

```text
https://github.com/Redot-Engine/redot-engine/releases/download/redot-26.2-stable/Redot_v26.2-stable_linux_x64.zip
```

Its required SHA-256 is
`f474d890806c41af15513cf5a8600243e241882e11b68dbb95660e3465b5b1e4`.
Every external GitHub Action is pinned to a full commit SHA, and the Emscripten
SDK is pinned to commit `d49219d03a41cd12f95a33ba84273c20d41fd350`
(`4.0.11`).

## Release identity

- VERSION tag: `v2.5.200.stable`
- Archive: `redotchestrator-v2.5.200.stable-plugin.zip`
- Archive root: `addons/orchestrator/`
- Manifest asset: `release_manifests.json`
- Minimum compatibility field: `v26.2.0`
- Inclusive maximum compatibility field: `v26.3.0`

The release workflow rejects a tag that does not match `VERSION`, an incomplete
descriptor library set, removed artwork, symlinks, editor caches, and native
development artifacts.

## First hosted matrix evidence

- Repository: `https://github.com/dominicbytes/redotchestrator`
- Default branch: `redot-26.2`
- Verified source commit: `1ee46ff31a6a3e93ebb8c1d82897ee9c3d77c543`
- GitHub Actions run: `31560191202` (19 jobs succeeded; tag-only release skipped)
- Plugin artifact: `9127825011`, GitHub digest `sha256:9b3b82d45a6333caa2914f8141d1d76978903dbb355d91cafe8d8c239b4144a6`
- Demo artifact: `9127825442`, GitHub digest `sha256:0b8fd0db1f44dd6da8b6b344811fd4ded1856803d40e6e497e19b31d89b6699a`
- Package generated from the downloaded combined plugin artifact: 53,367,796
  bytes, SHA-256 `768d6805f0ffa1edac64cfe1403d99f506ac65e2110be284c0114fc85ae9dec2`

## Redot 26.3 runtime lock

- Official engine: `redot-26.3-stable`, commit
  `85e5203945af15755dd2f66cb6fdf7545fd88c0b`.
- Official Windows console version: `26.3.stable.official.85e520394`.
- Compatibility API header on both 26.2 and 26.3: Godot `4.5.2`, single precision.
- The binding gitlink remains `9e7161bd7a3572a36ffa93109e73169861e9f834`.
  The owner fork fixes the Array initializer-list constructor without changing
  the 26.2 API or interface header.
- `cmake/redot-forward-compat.cmake` compiles a build-tree copy of the pinned
  Variant implementation with a guarded cleanup table lookup for newer types.
  It does not alter the dependency checkout or regenerate the API.
- `patches/extension_api.json` and `patches/gdextension_interface.json` remain
  upstream Godot 4.7 reference inputs, not Redot binding inputs.

The extra CI job downloads the official Linux x64 archive:

```text
https://github.com/Redot-Engine/redot-engine/releases/download/redot-26.3-stable/Redot_v26.3-stable_linux_x64.zip
```

SHA-256: `7d5155b80f4262f0a394c5040e539bb330cb63dd147bedd9172d83ec2efdbd91`.
The downloaded bytes were also verified against official `SHA512-SUMS.txt`:
`d370f50a85efa1c62549743c9c55f56fdac3f4ea66f353bfa3135d4c4fe68ef47849db7ce5d37b6ab9b09c5aa6ee748e10f44dc2c55485f3e799f5dfe2344166`.
The release API's asset SHA-256 agrees with the computed digest. All added
external action references use full commit SHAs.

API comparison: 13 classes and eight methods on existing classes were added;
none were removed and no existing method hashes or virtual signatures changed.
The interface header only adds `GDEXTENSION_VARIANT_TYPE_STRUCT` (39), moving
`VARIANT_MAX` to 40. Existing builtin sizes, member offsets and native structure
layouts are unchanged. Script language/instance, editor, graph, loader/saver,
and debugger class APIs are unchanged. See ADR-0023 for the resulting strategy.

## Shared Windows build verification

Both editor and release targets were rebuilt with MSVC `19.51.36248.0`, CMake
`4.3.1-msvc1`, Visual Studio bundled Ninja, and Python `3.14.7`. The build applies
`/d1trimfile:<REPO>/` to the plugin and bindings; ASCII and UTF-16LE scans of both
DLLs found no local path strings.

| Working-tree output | SHA-256 |
|---|---|
| Windows editor DLL | `178f2e52648d2793d913602eb4869f2cf3197683a665124be1d9c536f1b27558` |
| Windows release DLL | `6004dda0a91c84628d55a89198a4916a41857a0afb58e48453c2f29cb0554724` |

The official 26.3 template archive SHA-256 is
`a05e5d34d8c09b6ac1bcfe9cdca8452dedf5efe42c6b63f7ae5e62c1d04ef410`.
A Windows release export made by the corrected editor completed with exit 0;
the exported executable, using that official template and the shared release
DLL, ran headless with Dummy audio for 120 frames and exited 0 without stderr.
The official editor also wrote a runnable export, then crashed on shutdown;
that export command remains a failed editor result.
