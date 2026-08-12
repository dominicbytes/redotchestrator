# Release checklist

## Completed implementation

- [x] Base the port on exact upstream `v2.5.stable`.
- [x] Pin Redot 26.2 and redot-cpp 26.2 revisions.
- [x] Adapt GDExtension interfaces and lifecycle behavior to Redot 26.2.
- [x] Preserve serialized-resource and native compatibility identifiers.
- [x] Build clean Windows editor and release libraries.
- [x] Pass all 42 upstream-derived scene fixtures on the official Redot editor.
- [x] Load representative `.torch` resources in editor/runtime/export paths.
- [x] Harden the updater and pass its native security suite.
- [x] Replace restricted upstream/Godot-logo artwork with original artwork.
- [x] Record upstream attribution, downstream modifications, and asset provenance.
- [x] Pin GitHub Actions and the Emscripten SDK to immutable revisions.
- [x] Add all-platform CI artifact assembly and strict deterministic packaging.
- [x] Verify a Windows release-template run and exported executable.

## Required before public posting or release

- [ ] Obtain Redot trademark/brand clearance for `redotchestrator`.
- [ ] Confirm the intended `dominicbytes/redotchestrator` publication target and
  authorize the external GitHub actions.
- [ ] Run the first hosted Linux, macOS, Windows, Android, Web, and iOS matrix;
  fix any environment-specific failure before tagging.
- [ ] From the green commit, tag `v2.5.stable` and verify the deterministic ZIP,
  checksums, manifest, updater metadata, and clean installation from the exact
  release URL.
- [ ] If submitting to the Redot Asset Library, verify its current naming,
  layout, and capitalization requirements against the published release.
