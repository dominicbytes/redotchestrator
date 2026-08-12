from __future__ import annotations

import importlib.util
import json
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
            (addon / document).write_text(f"{document}\n", encoding="utf-8")
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

            self.assertEqual(PACKAGE_RELEASE.sha256(first), PACKAGE_RELEASE.sha256(second))
            self.assertEqual(entry["asset_size"], first.stat().st_size)
            self.assertEqual([entry], json.loads(manifest.read_text(encoding="utf-8")))
            with zipfile.ZipFile(first) as archive:
                self.assertTrue(all(name.startswith(PACKAGE_RELEASE.ADDON_PREFIX) for name in archive.namelist()))
                self.assertIn("addons/orchestrator/LICENSE", archive.namelist())

    def test_missing_descriptor_library_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            distribution = self.make_distribution(Path(temporary))
            (distribution / "addons" / "orchestrator" / "bin" / "plugin.so").unlink()
            with self.assertRaisesRegex(ValueError, "Descriptor library is missing"):
                PACKAGE_RELEASE.validate_distribution(distribution)

    def test_release_tag_comes_from_version_file(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            version_file = Path(temporary) / "VERSION"
            version_file.write_text(
                'version_major = 2\nversion_minor = 5\nversion_maintenance = 0\n'
                'version_status = "stable"\n',
                encoding="utf-8",
            )
            self.assertEqual("v2.5.stable", PACKAGE_RELEASE.expected_release_tag(version_file))

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
