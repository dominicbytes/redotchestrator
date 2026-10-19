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
MAX_ASSET_SIZE = 1024 * 1024 * 1024
MAX_EXTRACTED_SIZE = 1024 * 1024 * 1024
MAX_ENTRIES = 16384
REQUIRED_DOCUMENTS = (
    "ASSET_LICENSES.md",
    "AUTHORS.md",
    "CHANGELOG.md",
    "LICENSE",
    "MODIFICATIONS.md",
    "README.md",
    "THIRD_PARTY_LICENSES/redot-cpp-LICENSE.md",
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


def safe_archive_path(path: str) -> bool:
    if len(path) > 4096 or not path.startswith(ADDON_PREFIX):
        return False
    for segment in path.removesuffix("/").split("/"):
        if (not segment or segment in (".", "..") or len(segment) > 255
                or segment.startswith(" ") or segment.endswith((" ", "."))
                or any(ord(c) < 32 or ord(c) > 126 or c in '<>:"\\|?*' for c in segment)):
            return False
        base = segment.split(".")[0].lower()
        if base in {"con", "prn", "aux", "nul"} or re.fullmatch(r"(?:com|lpt)[1-9]", base):
            return False
    return True


def compatibility_version(value: str) -> tuple[int, ...]:
    if len(value) > 32 or not REDOT_COMPATIBILITY_RE.fullmatch(value):
        raise ValueError("Unsupported Redot compatibility version")
    components = tuple(map(int, value[1:].split(".")))
    if components[0] == 0 or any(c > 0xFFFFFFFF for c in components):
        raise ValueError("Unsupported Redot compatibility version")
    return components


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def is_link(path: Path) -> bool:
    try:
        info = path.lstat()
    except FileNotFoundError:
        return False
    # Check reparse attributes as well as symlinks without requiring Python 3.12's is_junction().
    return stat.S_ISLNK(info.st_mode) or bool(getattr(info, "st_file_attributes", 0) & 0x400)


def addon_from_distribution(distribution: Path) -> Path:
    addon = distribution / "addons" / "orchestrator"
    if any(is_link(path) for path in (distribution, addon.parent, addon)):
        raise ValueError("Linked distribution directories are not allowed")
    if not addon.is_dir():
        raise ValueError(f"Missing addon directory: {addon}")
    return addon


def descriptor_library_paths(addon: Path) -> list[tuple[str, Path]]:
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
    paths: list[tuple[str, Path]] = []
    for feature, raw_path in config["libraries"].items():
        value = raw_path.strip()
        if len(value) < 2 or value[0] != '"' or value[-1] != '"' or '"' in value[1:-1]:
            raise ValueError("Descriptor library paths must be quoted strings")
        resource_path = value[1:-1]
        if not resource_path.startswith(expected_prefix):
            raise ValueError(f"Library {feature!r} escapes the addon: {resource_path}")
        suffix = resource_path.removeprefix(expected_prefix)
        relative = PurePosixPath(suffix)
        if not safe_archive_path(ADDON_PREFIX + suffix) or suffix.endswith("/"):
            raise ValueError(f"Unsafe descriptor library path: {resource_path}")
        library = addon.joinpath(*relative.parts)
        if not library.resolve().is_relative_to(addon.resolve()):
            raise ValueError("Descriptor library escapes the addon")
        paths.append((feature, library))
    return paths


def validate_distribution(distribution: Path) -> tuple[Path, list[Path]]:
    addon = addon_from_distribution(distribution)

    for document in REQUIRED_DOCUMENTS:
        if not (addon / document).is_file() or not (addon / document).stat().st_size:
            raise ValueError(f"Missing redistribution document: {document}")

    for artwork in REMOVED_ARTWORK:
        if (addon / "icons" / artwork).exists():
            raise ValueError(f"Removed upstream artwork is present: {artwork}")

    logo = addon / "icons" / "Redotchestrator_Logo.svg"
    logo_small = addon / "icons" / "Redotchestrator_Logo_16x16.svg"
    if not logo.is_file() or not logo_small.is_file():
        raise ValueError("Original redotchestrator logo sources are missing")

    files: list[Path] = []
    seen: set[str] = set()
    extracted_size = 0
    for path in sorted(addon.rglob("*")):
        if is_link(path):
            raise ValueError("Links are not allowed in release packages")
        member = ADDON_PREFIX + path.relative_to(addon).as_posix()
        if not safe_archive_path(member) or member.lower() in seen:
            raise ValueError("Unsafe or case-colliding archive path")
        seen.add(member.lower())
        if path.is_dir():
            continue
        if not path.is_file():
            raise ValueError("Only regular files are allowed in release packages")
        if path.suffix.lower() in FORBIDDEN_SUFFIXES:
            raise ValueError(f"Development artifact is present: {path.name}")
        if ".godot" in path.relative_to(addon).parts:
            raise ValueError(f"Editor cache is present: {path}")
        files.append(path)
        extracted_size += path.stat().st_size
        if extracted_size > MAX_EXTRACTED_SIZE or len(files) > MAX_ENTRIES:
            raise ValueError("Distribution exceeds updater limits")

    if not files:
        raise ValueError("Addon contains no files")

    emitted = {path.relative_to(addon).as_posix() for path in files}
    for feature, library in descriptor_library_paths(addon):
        relative = library.relative_to(addon).as_posix()
        bundle = feature in {"ios", "ios.debug"} and relative.endswith(".xcframework")
        if relative not in emitted and not (bundle and any(name.startswith(relative + "/") for name in emitted)):
            raise ValueError(f"Descriptor library is missing or has mismatched case: {relative}")
        if not library.exists():
            raise ValueError(f"Descriptor library is missing: {library.relative_to(addon)}")
        if library.is_file() and not library.stat().st_size:
            raise ValueError("Descriptor library is empty")
        if library.is_dir() and not any(path.is_file() and path.stat().st_size for path in library.rglob("*")):
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
            extracted_size = 0
            for source in files:
                relative = source.relative_to(addon).as_posix()
                info = zipfile.ZipInfo(f"{ADDON_PREFIX}{relative}", (1980, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED if source.stat().st_size else zipfile.ZIP_STORED
                info.create_system = 3
                mode = stat.S_IFREG | (0o755 if os.access(source, os.X_OK) else 0o644)
                info.external_attr = mode << 16
                info._compresslevel = 9
                with source.open("rb") as stream, archive.open(info, "w") as member:
                    for chunk in iter(lambda: stream.read(64 * 1024), b""):
                        extracted_size += len(chunk)
                        if extracted_size > MAX_EXTRACTED_SIZE:
                            raise ValueError("Distribution exceeds updater extracted-size limit")
                        member.write(chunk)
        if temporary.stat().st_size > MAX_ASSET_SIZE:
            raise ValueError("Archive exceeds updater size limit")
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
    redot_compatibility_maximum: str | None = None,
) -> dict[str, object]:
    if (len(version) > 64 or not VERSION_RE.fullmatch(version)
            or any(int(c) > 0x7FFFFFFFFFFFFFFF for c in re.findall(r"[0-9]+", version))):
        raise ValueError(f"Unsupported release version: {version}")
    minimum = compatibility_version(redot_compatibility)
    if redot_compatibility_maximum is not None:
        maximum = compatibility_version(redot_compatibility_maximum)
        if minimum[0] != maximum[0] or minimum > maximum:
            raise ValueError("Invalid Redot compatibility range")
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
    if redot_compatibility_maximum is not None:
        entry["redot_compatibility_maximum"] = redot_compatibility_maximum
    write_manifest(manifest, entry)
    return entry


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("distribution", type=Path, help="Directory containing addons/orchestrator")
    parser.add_argument("output", type=Path, help="Destination plugin ZIP")
    parser.add_argument("--version", required=True, help="Release tag, for example v2.5.200.stable")
    parser.add_argument("--redot-compatibility", required=True, help="Minimum compatible Redot release")
    parser.add_argument("--redot-compatibility-maximum", help="Inclusive tested Redot upper bound")
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
            args.redot_compatibility_maximum,
        )
    except (OSError, ValueError, configparser.Error, json.JSONDecodeError) as error:
        print(f"release packaging failed: {error}", file=os.sys.stderr)
        return 1
    print(json.dumps(entry, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
