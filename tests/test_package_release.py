from __future__ import annotations

import importlib.util
import json
import os
import subprocess
from unittest.mock import patch
import tempfile
import unittest
import zipfile
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "tools" / "package_release.py"
SPEC = importlib.util.spec_from_file_location("package_release", SCRIPT)
PACKAGE_RELEASE = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(PACKAGE_RELEASE)


class PackageReleaseTests(unittest.TestCase):
    def make_distribution(self, root: Path) -> Path:
        distribution = root / "distribution"
        addon = distribution / "addons" / "orchestrator"
        (addon / "icons").mkdir(parents=True)
        (addon / "bin" / "ios.xcframework").mkdir(parents=True)
        for document in PACKAGE_RELEASE.REQUIRED_DOCUMENTS:
            (addon / document).parent.mkdir(parents=True, exist_ok=True)
            (addon / document).write_text(f"{document}\n", encoding="utf-8")
        (addon / "THIRD_PARTY_LICENSES/redot-cpp-LICENSE.md").write_bytes(
            (SCRIPT.parents[1] / "extern/godot-cpp/LICENSE.md").read_bytes())
        (addon / "icons" / "Redotchestrator_Logo.svg").write_text("<svg/>\n", encoding="utf-8")
        (addon / "icons" / "Redotchestrator_Logo_16x16.svg").write_text("<svg/>\n", encoding="utf-8")
        (addon / "bin" / "plugin.so").write_bytes(b"linux")
        (addon / "bin" / "ios.xcframework" / "plugin").write_bytes(b"ios")
        library_lines = []
        for feature in sorted(PACKAGE_RELEASE.EXPECTED_LIBRARY_FEATURES):
            library = "bin/ios.xcframework" if feature.startswith("ios") else "bin/plugin.so"
            library_lines.append(f'{feature}="res://addons/orchestrator/{library}"')
        (addon / "orchestrator.gdextension").write_text(
            "[configuration]\nentry_symbol=\"extension_library_init\"\n\n"
            "[libraries]\n" + "\n".join(library_lines) + "\n",
            encoding="utf-8",
        )
        return distribution

    def test_package_is_deterministic_and_updates_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            distribution = self.make_distribution(root)
            manifest = root / "release_manifests.json"
            first = root / "one" / "redotchestrator-v2.5.stable-plugin.zip"
            second = root / "two" / "redotchestrator-v2.5.stable-plugin.zip"

            entry = PACKAGE_RELEASE.package_release(distribution, first, "v2.5.stable", "v26.2.0", manifest)
            PACKAGE_RELEASE.package_release(distribution, second, "v2.5.stable", "v26.2.0", manifest)

            native = os.environ.get("UPDATE_SECURITY_TEST_BINARY")
            if native:
                subprocess.run([native, "--archive", str(first)], check=True, timeout=30)
            self.assertEqual(PACKAGE_RELEASE.sha256(first), PACKAGE_RELEASE.sha256(second))
            self.assertEqual(entry["asset_size"], first.stat().st_size)
            self.assertEqual([entry], json.loads(manifest.read_text(encoding="utf-8")))
            with zipfile.ZipFile(first) as archive:
                self.assertTrue(all(name.startswith(PACKAGE_RELEASE.ADDON_PREFIX) for name in archive.namelist()))
                self.assertIn("addons/orchestrator/LICENSE", archive.namelist())
                self.assertEqual(
                    (SCRIPT.parents[1] / "extern/godot-cpp/LICENSE.md").read_bytes(),
                    archive.read("addons/orchestrator/THIRD_PARTY_LICENSES/redot-cpp-LICENSE.md"))

    def test_compatibility_range_is_recorded_and_validated(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            distribution = self.make_distribution(root)
            archive = root / "redotchestrator-v2.5.200.stable-plugin.zip"
            manifest = root / "release_manifests.json"
            entry = PACKAGE_RELEASE.package_release(
                distribution, archive, "v2.5.200.stable", "v26.2.0", manifest, "v26.3.0")
            self.assertEqual("v26.3.0", entry["redot_compatibility_maximum"])
            self.assertEqual([entry], json.loads(manifest.read_text()))
            for maximum in ("", "26.3", "v26.1.0", "v27.0.0"):
                with self.subTest(maximum=maximum), self.assertRaises(ValueError):
                    PACKAGE_RELEASE.package_release(
                        distribution, archive, "v2.5.200.stable", "v26.2.0", manifest, maximum)

    def test_missing_descriptor_library_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            distribution = self.make_distribution(Path(temporary))
            (distribution / "addons" / "orchestrator" / "bin" / "plugin.so").unlink()
            with self.assertRaisesRegex(ValueError, "Descriptor library is missing"):
                PACKAGE_RELEASE.validate_distribution(distribution)

    def test_desktop_library_directory_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            distribution = self.make_distribution(Path(temporary))
            library = distribution / "addons/orchestrator/bin/plugin.so"
            library.unlink()
            library.mkdir()
            (library / "not-a-library.txt").write_bytes(b"payload")
            with self.assertRaisesRegex(ValueError, "Descriptor library is missing"):
                PACKAGE_RELEASE.validate_distribution(distribution)

    def test_only_ios_xcframework_features_allow_bundles(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            distribution = self.make_distribution(Path(temporary))
            addon = distribution / "addons/orchestrator"
            PACKAGE_RELEASE.validate_distribution(distribution)
            descriptor = addon / "orchestrator.gdextension"
            original = descriptor.read_text()
            for feature in ("macos", "ios.extra"):
                with self.subTest(feature=feature):
                    lines = [line for line in original.splitlines() if not line.startswith(feature + "=")]
                    descriptor.write_text("\n".join(lines) + f'\n{feature}="res://addons/orchestrator/bin/ios.xcframework"\n')
                    with self.assertRaises(ValueError):
                        PACKAGE_RELEASE.validate_distribution(distribution)
            descriptor.write_text(original.replace("bin/ios.xcframework", "bin/ios.bundle"))
            (addon / "bin/ios.xcframework").rename(addon / "bin/ios.bundle")
            with self.assertRaises(ValueError):
                PACKAGE_RELEASE.validate_distribution(distribution)

    def test_descriptor_backslash_and_noncanonical_paths_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            distribution = self.make_distribution(Path(temporary))
            addon = distribution / "addons/orchestrator"
            descriptor = addon / "orchestrator.gdextension"
            original = descriptor.read_text()
            for suffix in ("../../external.dll", "..\\..\\external.dll", "bin//plugin.so",
                           "bin/./plugin.so", "bin/CON.dll", "bin/plugin.so.", "bin/drive:part"):
                with self.subTest(suffix=suffix):
                    descriptor.write_text(original.replace("bin/plugin.so", suffix))
                    with self.assertRaises(ValueError):
                        PACKAGE_RELEASE.validate_distribution(distribution)

    def test_missing_bindings_license_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            distribution = self.make_distribution(Path(temporary))
            (distribution / "addons/orchestrator/THIRD_PARTY_LICENSES/redot-cpp-LICENSE.md").unlink()
            with self.assertRaisesRegex(ValueError, "Missing redistribution document"):
                PACKAGE_RELEASE.validate_distribution(distribution)

    def test_unsafe_archive_names_match_native_consumer(self) -> None:
        suffixes = ("valid.txt", "icons/Logo 16x16.svg", "", "../escape", "a//b", "a/./b",
                    "a\\b", "CON.txt", "lpt9", "foo.", " foo", "foo ", "a:b", "a?b",
                    "caf\u00e9.txt", "a\x00b", "a" * 256, "a/" * 2050)
        paths = [PACKAGE_RELEASE.ADDON_PREFIX + suffix for suffix in suffixes]
        expected = [True, True, True] + [False] * (len(paths) - 3)
        self.assertEqual(expected, [PACKAGE_RELEASE.safe_archive_path(path) for path in paths])
        native = os.environ.get("UPDATE_SECURITY_TEST_BINARY")
        if native:
            # NUL is valid input to the predicate but cannot be passed in an argv element.
            result = subprocess.run([native, "--paths"], input="\n".join(paths) + "\n",
                                    text=True, capture_output=True, check=True, timeout=30)
            self.assertEqual(expected, [line == "1" for line in result.stdout.splitlines()])

    def test_metadata_limits_match_consumer(self) -> None:
        native = os.environ.get("UPDATE_SECURITY_TEST_BINARY")
        tags = ["v2.5.stable", "v2.5.1.rc2", "v2.5.dev", "v2.5.beta",
                "v9223372036854775808.2.stable", "v2.5.rc9223372036854775808",
                "v" + "1" * 65 + ".2.stable"]
        if native:
            result = subprocess.run([native, "--tags"], input="\n".join(tags) + "\n",
                                    text=True, capture_output=True, check=True, timeout=30)
            self.assertEqual([True, True, True, False, False, False, False],
                             [line == "1" for line in result.stdout.splitlines()])
        for value in ("v0.2.0", "v4294967296.2.0", "v26.2." + "0" * 33):
            with self.subTest(value=value), self.assertRaises(ValueError):
                PACKAGE_RELEASE.compatibility_version(value)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            distribution = self.make_distribution(root)
            for version in ("v" + "1" * 65 + ".2.stable", "v9223372036854775808.2.stable", "v2.5.rc9223372036854775808"):
                with self.subTest(version=version), self.assertRaises(ValueError):
                    PACKAGE_RELEASE.package_release(distribution, root / ("redotchestrator-" + version + "-plugin.zip"),
                                                    version, "v26.2.0", root / "manifest.json")

    def test_size_and_entry_budgets_are_enforced(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            distribution = self.make_distribution(root)
            for limit in ("MAX_EXTRACTED_SIZE", "MAX_ENTRIES", "MAX_ASSET_SIZE"):
                with self.subTest(limit=limit), patch.object(PACKAGE_RELEASE, limit, 1):
                    with self.assertRaisesRegex(ValueError, "limit"):
                        PACKAGE_RELEASE.package_release(distribution, root / "redotchestrator-v2.5.stable-plugin.zip",
                                                        "v2.5.stable", "v26.2.0", root / "manifest.json")
                    self.assertFalse((root / "manifest.json").exists())

    def test_linked_addon_root_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            distribution = self.make_distribution(root)
            addon = distribution / "addons/orchestrator"
            target = distribution / "addons/original"
            addon.rename(target)
            try:
                if os.name == "nt":
                    subprocess.run(["cmd", "/c", "mklink", "/J", str(addon), str(target)],
                                   check=True, capture_output=True, timeout=30)
                else:
                    addon.symlink_to(target, target_is_directory=True)
                with self.assertRaisesRegex(ValueError, "Linked"):
                    PACKAGE_RELEASE.validate_distribution(distribution)
                native = os.environ.get("UPDATE_SECURITY_TEST_BINARY")
                if native:
                    subprocess.run([native, "--remove-tree", str(addon)], check=True, timeout=30)
                    self.assertTrue((target / "orchestrator.gdextension").is_file())
            finally:
                if PACKAGE_RELEASE.is_link(addon):
                    if os.name == "nt":
                        os.rmdir(addon)
                    else:
                        addon.unlink()

    def test_unicode_source_and_empty_library_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            distribution = self.make_distribution(Path(temporary))
            addon = distribution / "addons/orchestrator"
            bad = addon / "caf\u00e9.txt"
            bad.write_bytes(b"asset")
            with self.assertRaisesRegex(ValueError, "Unsafe"):
                PACKAGE_RELEASE.validate_distribution(distribution)
            bad.unlink()
            (addon / "bin/plugin.so").write_bytes(b"")
            with self.assertRaisesRegex(ValueError, "library is empty"):
                PACKAGE_RELEASE.validate_distribution(distribution)

    def test_case_collisions_are_rejected_even_on_case_insensitive_hosts(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            distribution = self.make_distribution(Path(temporary))
            addon = distribution / "addons/orchestrator"
            aliases = [addon / "bin/plugin.so", addon / "bin/PLUGIN.SO"]
            # On case-sensitive hosts the alias must exist as a file of its own.
            if not aliases[1].exists():
                aliases[1].write_bytes(aliases[0].read_bytes())
            with patch.object(Path, "rglob", side_effect=lambda *args, **kwargs: iter(aliases)):
                with self.assertRaisesRegex(ValueError, "case-colliding"):
                    PACKAGE_RELEASE.validate_distribution(distribution)

    def test_package_completeness_contract_matches_native(self) -> None:
        native = os.environ.get("UPDATE_SECURITY_TEST_BINARY")
        if not native:
            self.skipTest("UPDATE_SECURITY_TEST_BINARY is required for contract comparison")
        result = subprocess.run([native, "--contract"], text=True, capture_output=True, check=True, timeout=30)
        actual = set(result.stdout.splitlines())
        expected = {"feature:" + key for key in PACKAGE_RELEASE.EXPECTED_LIBRARY_FEATURES}
        expected.update("document:" + name for name in PACKAGE_RELEASE.REQUIRED_DOCUMENTS)
        expected.update({f"limit:asset:{PACKAGE_RELEASE.MAX_ASSET_SIZE}",
                         f"limit:expanded:{PACKAGE_RELEASE.MAX_EXTRACTED_SIZE}",
                         f"limit:entries:{PACKAGE_RELEASE.MAX_ENTRIES}"})
        self.assertEqual(expected, actual)

    def test_build_action_copies_the_pinned_bindings_license(self) -> None:
        action = (SCRIPT.parents[1] / ".github/actions/orchestrator-build/action.yml").read_text()
        self.assertIn("cp '${{ github.workspace }}/extern/godot-cpp/LICENSE.md' "
                      "'${{ github.workspace }}/project/addons/orchestrator/THIRD_PARTY_LICENSES/redot-cpp-LICENSE.md'", action)

    def test_release_tag_comes_from_version_file(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            version_file = Path(temporary) / "VERSION"
            version_file.write_text(
                'version_major = 2\nversion_minor = 5\nversion_maintenance = 0\n'
                'version_status = "stable"\n',
                encoding="utf-8",
            )
            self.assertEqual("v2.5.stable", PACKAGE_RELEASE.expected_release_tag(version_file))

    def test_repository_version_uses_the_downstream_release_line(self) -> None:
        version_file = Path(__file__).resolve().parents[1] / "VERSION"
        self.assertEqual("v2.5.200.stable", PACKAGE_RELEASE.expected_release_tag(version_file))

    def test_invalid_redot_compatibility_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            distribution = self.make_distribution(root)
            with self.assertRaisesRegex(ValueError, "Unsupported Redot compatibility version"):
                PACKAGE_RELEASE.package_release(
                    distribution,
                    root / "redotchestrator-v2.5.stable-plugin.zip",
                    "v2.5.stable",
                    "26.2",
                    root / "release_manifests.json",
                )


if __name__ == "__main__":
    unittest.main()
