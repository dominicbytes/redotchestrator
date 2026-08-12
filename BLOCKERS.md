# External release gates

There are no unresolved technical blockers for the source fork. The Redot 26.2
port builds, loads, executes its upstream integration fixtures, exports, passes
its native updater-security tests on Windows, and completed its first hosted
all-platform matrix with 19 successful jobs.

## External gates before public release

### RELEASE-NAME: Redot trademark clearance

- The user-selected public name is exactly `redotchestrator`.
- Redot's published terms identify the Redot name and logo as trademarks and do
  not provide a blanket trademark grant through the engine's source license.
- This fork uses original artwork, contains no Redot logo, and includes a clear
  non-affiliation notice, but those measures do not themselves authorize the
  name.
- Clear this gate with written permission or a demonstrably applicable Redot
  brand-use policy before publishing or promoting a tagged release.

### RELEASE-AUTHORIZATION: Tagged release

The source fork is public, but no tag or GitHub release has been created. Create
`v2.5.stable` and publish its deterministic package only after the repository
owner explicitly requests the release and the name-clearance gate is resolved.

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
| Public downstream fork | Created `dominicbytes/redotchestrator` as a true fork and made `redot-26.2` the default branch. |
| First hosted all-platform run | [Run 31560191202](https://github.com/dominicbytes/redotchestrator/actions/runs/31560191202) completed with 19 successful jobs and uploaded combined plugin/demo artifacts. |
