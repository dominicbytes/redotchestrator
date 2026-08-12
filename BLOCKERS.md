# Blockers and external release gates

There are no unresolved local implementation blockers for creating the source
fork. The Redot 26.2 port builds, loads, executes its upstream integration
fixtures, exports, and passes its native updater-security tests on Windows.

## External gates before public release

### RELEASE-NAME: Redot trademark clearance

- The user-selected public name is exactly `redotchestrator`.
- Redot's published terms identify the Redot name and logo as trademarks and do
  not provide a blanket trademark grant through the engine's source license.
- This fork uses original artwork, contains no Redot logo, and includes a clear
  non-affiliation notice, but those measures do not themselves authorize the
  name.
- Clear this gate with written permission or a demonstrably applicable Redot
  brand-use policy before making the repository public under this name.

### RELEASE-CI: First remote all-platform run

- Windows editor/release binaries, the editor suite, and an exported executable
  are locally verified.
- Linux, macOS, Android, Web, and iOS jobs are configured with immutable action
  pins and exact Redot/redot-cpp inputs, but those hosted environments do not
  exist until the fork is posted.
- The release job cannot publish a ZIP unless every platform artifact is present
  and the strict package validator finds every descriptor library.
- Clear this operational gate by pushing the fork, observing one green matrix,
  and tagging `v2.5.stable` only after the green run.

### RELEASE-AUTHORIZATION: External publication

Creating a GitHub fork, pushing commits, or publishing a release is an external
state change. Perform it only after the repository owner explicitly confirms
the target and authorizes publication.

## Resolved preflight gates

| Former gate | Resolution |
|---|---|
| Unsafe upstream updater | Replaced with fork-only metadata, exact digest/size/tag checks, contained extraction, staging, backup, and rollback. |
| Missing Redot inputs | Redot engine and redot-cpp are pinned to exact 26.2 commits; official test binaries/templates are checksum recorded. |
| Missing native binaries | Windows editor and release builds succeed; CI builds all descriptor targets before packaging. |
| Godot 4.7 API mismatch | Adapted to Redot 26.2's 4.5.2 GDExtension compatibility API and validated through 42 scenes. |
| Restricted artwork | Removed upstream logos and Godot-logo project icons; added original SVG artwork and an asset inventory. |
| Source-only package | Added deterministic strict packaging, manifest generation, rights-file inclusion, and tag-gated release automation. |
| Unbounded/latest test downloads | Runner now requires an explicit Redot binary and enforces import/scene timeouts; CI downloads a checksum-pinned Redot 26.2 editor. |
