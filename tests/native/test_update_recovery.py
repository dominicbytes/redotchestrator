"""Exercise updater startup recovery in a disposable project using the current editor build."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("package_release", REPO / "tools/package_release.py")
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


@unittest.skipUnless(os.environ.get("REDOT_BIN"), "REDOT_BIN is required for startup recovery tests")
class UpdateRecoveryTests(unittest.TestCase):
    def test_linked_addons_refuses_startup_cleanup_and_recovery(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "project"
            root.mkdir()
            payload = Path(temporary) / "payload"
            shutil.copytree(REPO / "project/addons/orchestrator", payload / "orchestrator",
                            ignore=shutil.ignore_patterns("*.exp", "*.lib", "*.pdb"))
            (root / "project.godot").write_text('[application]\nconfig/name="Linked addons test"\n')
            markers = []
            for name in (".redotchestrator-update-stage", ".redotchestrator-update-backup"):
                directory = payload / name
                directory.mkdir()
                marker = directory / "preserve.txt"
                marker.write_text("user content")
                markers.append(marker)
            link = root / "addons"
            if os.name == "nt":
                subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(payload)],
                               capture_output=True, check=True, timeout=30)
            else:
                link.symlink_to(payload, target_is_directory=True)
            try:
                result = subprocess.run([os.environ["REDOT_BIN"], "--headless", "--audio-driver", "Dummy",
                                         "--quit-after", "600", "--path", str(root), "--import"],
                                        capture_output=True, text=True, timeout=120)
                self.assertEqual(0, result.returncode, result.stderr)
                self.assertIn("without links or junctions. Update manually.", result.stderr)
                self.assertTrue(all(marker.read_text() == "user content" for marker in markers))
                self.assertNotIn("ERROR:", result.stderr)
            finally:
                if os.name == "nt":
                    os.rmdir(link)
                else:
                    link.unlink()

    def test_recovery_retains_evidence_until_the_new_version_starts(self):
        current = PACKAGE.expected_release_tag(REPO / "VERSION")
        for state in ("new_started", "old_started", "orphan", "before_backup"):
            with self.subTest(state=state), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                addon = root / "addons/orchestrator"
                shutil.copytree(REPO / "project/addons/orchestrator", addon,
                                ignore=shutil.ignore_patterns("*.exp", "*.lib", "*.pdb"))
                (root / "project.godot").write_text('[application]\nconfig/name="Recovery test"\n')
                backup = root / "addons/.redotchestrator-update-backup"
                if state != "before_backup":
                    backup.mkdir()
                    (backup / "preserve.txt").write_text("previous addon")
                stage = root / "addons/.redotchestrator-update-stage-test"
                stage.mkdir()
                (stage / "staged.txt").write_text("staged addon")
                record = root / "addons/.redotchestrator-update-transaction.json"
                if state != "orphan":
                    record.write_text(json.dumps({"version": current if state == "new_started" else "v999.0.stable",
                                                  "stage": stage.name}))
                result = subprocess.run([os.environ["REDOT_BIN"], "--headless", "--audio-driver", "Dummy",
                                         "--quit-after", "600", "--path", str(root), "--import"],
                                        capture_output=True, text=True, timeout=120)
                self.assertEqual(0, result.returncode, result.stderr)
                preserve = state in ("old_started", "orphan")
                self.assertEqual(preserve, backup.exists())
                self.assertEqual(state == "old_started", record.exists())
                self.assertEqual(preserve, stage.exists())
                if state == "old_started":
                    self.assertIn("pending recovery", result.stderr)
                else:
                    self.assertEqual("", result.stderr)
                self.assertTrue((addon / "orchestrator.gdextension").is_file())


if __name__ == "__main__":
    unittest.main()
