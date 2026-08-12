#!/usr/bin/env python3
"""Validate and deterministically package a complete redotchestrator addon."""

from __future__ import annotations

import argparse
import configparser
import hashlib
import json
import os
import re
import stat
import tempfile
import zipfile
from pathlib import Path, PurePosixPath


ADDON_PREFIX = "addons/orchestrator/"
REQUIRED_DOCUMENTS = (
    "ASSET_LICENSES.md",
    "AUTHORS.md",
    "CHANGELOG.md",
    "LICENSE",
    "MODIFICATIONS.md",
    "README.md",
)
FORBIDDEN_SUFFIXES = (".exp", ".ilk", ".lib", ".pdb")
REMOVED_ARTWORK = {
    "Orchestrator_16x16.png",
    "Orchestrator_16x16.png.import",
    "Orchestrator_Logo.svg",
    "Orchestrator_Logo.svg.import",
    "Orchestrator_Logo_16x16.svg",
    "Orchestrator_Logo_16x16.svg.import",
}
EXPECTED_LIBRARY_FEATURES = {
    "linux.x86_64",
    "linux.debug.x86_64",
    "linux.arm64",
    "linux.debug.arm64",
    "macos",
    "macos.debug",
    "ios",
    "ios.debug",
    "windows.x86_64",
    "windows.debug.x86_64",
    "android.arm32",
    "android.debug.arm32",
    "android.arm64",
    "android.debug.arm64",
    "web.release.threads.wasm32",
    "web.release.nothreads.wasm32",
}
VERSION_RE = re.compile(r"^v[0-9]+\.[0-9]+(?:\.[0-9]+)?\.(?:stable|rc[0-9]*|dev[0-9]*)$")
REDOT_COMPATIBILITY_RE = re.compile(r"^v[0-9]+\.[0-9]+\.[0-9]+$")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def addon_from_distribution(distribution: Path) -> Path:
    distribution = distribution.resolve()
    addon = distribution / "addons" / "orchestrator"
    if not addon.is_dir():
        raise ValueError(f"Missing addon directory: {addon}")
    return addon


def descriptor_library_paths(addon: Path) -> list[Path]:
    descriptor = addon / "orchestrator.gdextension"
    if not descriptor.is_file():
        raise ValueError(f"Missing GDExtension descriptor: {descriptor}")

    config = configparser.ConfigParser(interpolation=None)
    config.optionxform = str
    config.read(descriptor, encoding="utf-8")
    if "libraries" not in config:
        raise ValueError("GDExtension descriptor has no [libraries] section")

    missing_features = EXPECTED_LIBRARY_FEATURES - set(config["libraries"])
    if missing_features:
        raise ValueError(
            "GDExtension descriptor is missing required library features: "
            + ", ".join(sorted(missing_features))
        )

    expected_prefix = "res://addons/orchestrator/"
    paths: list[Path] = []
    for feature, raw_path in config["libraries"].items():
        resource_path = raw_path.strip().strip('"')
        if not resource_path.startswith(expected_prefix):
            raise ValueError(f"Library {feature!r} escapes the addon: {resource_path}")
        relative = PurePosixPath(resource_path.removeprefix(expected_prefix))
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError(f"Unsafe descriptor library path: {resource_path}")
        paths.append(addon.joinpath(*relative.parts))
    return paths


def validate_distribution(distribution: Path) -> tuple[Path, list[Path]]:
    addon = addon_from_distribution(distribution)

    for document in REQUIRED_DOCUMENTS:
        if not (addon / document).is_file():
            raise ValueError(f"Missing redistribution document: {document}")

    for artwork in REMOVED_ARTWORK:
        if (addon / "icons" / artwork).exists():
            raise ValueError(f"Removed upstream artwork is present: {artwork}")

    logo = addon / "icons" / "Redotchestrator_Logo.svg"
    logo_small = addon / "icons" / "Redotchestrator_Logo_16x16.svg"
    if not logo.is_file() or not logo_small.is_file():
        raise ValueError("Original redotchestrator logo sources are missing")

    files: list[Path] = []
    for path in sorted(addon.rglob("*")):
        if path.is_symlink():
            raise ValueError(f"Symlinks are not allowed in release packages: {path}")
        if path.is_dir():
            continue
        if path.suffix.lower() in FORBIDDEN_SUFFIXES:
            raise ValueError(f"Development artifact is present: {path.name}")
        if ".godot" in path.relative_to(addon).parts:
            raise ValueError(f"Editor cache is present: {path}")
        files.append(path)

    if not files:
        raise ValueError("Addon contains no files")

    for library in descriptor_library_paths(addon):
        if not library.exists():
            raise ValueError(f"Descriptor library is missing: {library.relative_to(addon)}")
        if library.is_dir() and not any(path.is_file() for path in library.rglob("*")):
            raise ValueError(f"Descriptor library bundle is empty: {library.relative_to(addon)}")

    return addon, files


def deterministic_zip(addon: Path, files: list[Path], output: Path) -> None:
    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(f".{output.name}.tmp")
    if temporary.exists():
        temporary.unlink()

    try:
        with zipfile.ZipFile(
            temporary,
            mode="w",
            compression=zipfile.ZIP_DEFLATED,
            compresslevel=9,
        ) as archive:
            for source in files:
                relative = source.relative_to(addon).as_posix()
                info = zipfile.ZipInfo(f"{ADDON_PREFIX}{relative}", (1980, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.create_system = 3
                mode = stat.S_IFREG | (0o755 if os.access(source, os.X_OK) else 0o644)
                info.external_attr = mode << 16
                with source.open("rb") as stream:
                    archive.writestr(info, stream.read(), compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)
        os.replace(temporary, output)
    finally:
        if temporary.exists():
            temporary.unlink()


def read_manifest(path: Path) -> list[dict[str, object]]:
    if not path.exists():
        return []
    parsed = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(parsed, list) or not all(isinstance(item, dict) for item in parsed):
        raise ValueError("Release manifest must be a JSON array of objects")
    return parsed


def write_manifest(path: Path, entry: dict[str, object]) -> None:
    releases = [item for item in read_manifest(path) if item.get("version") != entry["version"]]
    releases.insert(0, entry)
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        mode="w",
        encoding="utf-8",
        newline="\n",
        dir=path.parent,
        prefix=f".{path.name}.",
        suffix=".tmp",
        delete=False,
    ) as stream:
        json.dump(releases, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
        temporary = Path(stream.name)
    os.replace(temporary, path)


def expected_release_tag(version_file: Path) -> str:
    values: dict[str, str] = {}
    for raw_line in version_file.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        key, separator, value = line.partition("=")
        if not separator:
            raise ValueError(f"Invalid VERSION line: {raw_line}")
        values[key.strip()] = value.strip().strip('"')
    try:
        major = int(values["version_major"])
        minor = int(values["version_minor"])
        maintenance = int(values["version_maintenance"])
        status = values["version_status"]
    except (KeyError, ValueError) as error:
        raise ValueError("VERSION is missing a valid release component") from error
    number = f"{major}.{minor}" if maintenance == 0 else f"{major}.{minor}.{maintenance}"
    return f"v{number}.{status}"


def package_release(
    distribution: Path,
    output: Path,
    version: str,
    redot_compatibility: str,
    manifest: Path,
) -> dict[str, object]:
    if not VERSION_RE.fullmatch(version):
        raise ValueError(f"Unsupported release version: {version}")
    if not REDOT_COMPATIBILITY_RE.fullmatch(redot_compatibility):
        raise ValueError(f"Unsupported Redot compatibility version: {redot_compatibility}")
    expected_name = f"redotchestrator-{version}-plugin.zip"
    if output.name != expected_name:
        raise ValueError(f"Release archive must be named {expected_name}")

    addon, files = validate_distribution(distribution)
    deterministic_zip(addon, files, output)
    entry: dict[str, object] = {
        "version": version,
        "redot_compatibility": redot_compatibility,
        "asset_name": output.name,
        "sha256": sha256(output),
        "asset_size": output.stat().st_size,
        "blog_url": f"https://github.com/dominicbytes/redotchestrator/releases/tag/{version}",
    }
    write_manifest(manifest, entry)
    return entry


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("distribution", type=Path, help="Directory containing addons/orchestrator")
    parser.add_argument("output", type=Path, help="Destination plugin ZIP")
    parser.add_argument("--version", required=True, help="Release tag, for example v2.5.stable")
    parser.add_argument("--redot-compatibility", required=True, help="Minimum compatible Redot release")
    parser.add_argument("--manifest", required=True, type=Path, help="Manifest JSON to update")
    parser.add_argument("--version-file", type=Path, help="VERSION file that must match the release tag")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.version_file and expected_release_tag(args.version_file) != args.version:
            raise ValueError(
                f"Release tag {args.version} does not match "
                f"{expected_release_tag(args.version_file)} from {args.version_file}"
            )
        entry = package_release(
            args.distribution,
            args.output,
            args.version,
            args.redot_compatibility,
            args.manifest,
        )
    except (OSError, ValueError, configparser.Error, json.JSONDecodeError) as error:
        print(f"release packaging failed: {error}", file=os.sys.stderr)
        return 1
    print(json.dumps(entry, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
