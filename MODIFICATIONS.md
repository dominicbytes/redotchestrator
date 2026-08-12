# redotchestrator modification notice

`redotchestrator` is a downstream derivative of Godot Orchestrator v2.5 stable,
based on upstream commit `775cc4549657199b385f0a16e4c5523aba8b5500` from
<https://github.com/CraterCrash/godot-orchestrator>.

The upstream work is Copyright 2023-present Crater Crash Studios LLC and its
contributors and is licensed under Apache-2.0. The original authorship record is
retained in `AUTHORS.md`, and the complete license is retained in `LICENSE`.

Downstream modifications and original redotchestrator artwork are Copyright
2026 Dominic Bytes and are licensed under Apache-2.0.

Material downstream changes include:

- porting the latest stable 2.5 source to Redot 26.2 and its Godot 4.5.2
  compatibility API using the pinned Redot and redot-cpp revisions recorded in
  `UPSTREAM_LOCK.md`;
- adapting GDExtension interfaces, script-language lifecycle, resource formats,
  parser ownership, and Windows VM recursion behavior for Redot 26.2;
- replacing upstream release binaries with builds produced from the pinned Redot
  bindings;
- restricting the editor updater to controlled redotchestrator release assets,
  explicit Redot compatibility metadata, exact names, sizes, and SHA-256 digests,
  with contained staging, backup, rollback, and atomic installation;
- replacing upstream and Godot-logo artwork with original redotchestrator
  artwork and recording all redistributed visual assets in `ASSET_LICENSES.md`;
  and
- making CI, integration tests, artifact assembly, and release manifests
  Redot-specific and reproducible from immutable dependency inputs.

Serialized-resource and API compatibility identifiers such as `OScript`,
`.torch`, `.os`, the `orchestrator/` settings namespace, and
`res://addons/orchestrator` intentionally remain unchanged so existing projects
and upstream fixtures do not require a destructive migration.

## Trademark and affiliation

redotchestrator is an unofficial community-maintained fork. It is not affiliated
with, sponsored by, or endorsed by the Redot Engine project or Crater Crash
Studios LLC. Redot Engine identifies the Redot Engine name and logo as
trademarks of the Redot community. This project does not include or reproduce
the Redot Engine logo. The Apache-2.0 license does not grant trademark rights.
