"""Exercise the compiled updater helpers with owned link and concurrent-run fixtures."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


@unittest.skipUnless(os.environ.get("UPDATE_SECURITY_TEST_BINARY"), "UPDATE_SECURITY_TEST_BINARY is required")
class UpdateFilesystemTests(unittest.TestCase):
    def test_intermediate_link_refuses_cleanup_and_extraction(self):
        binary = os.environ["UPDATE_SECURITY_TEST_BINARY"]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            victim = root / "victim"
            stage = victim / "stage"
            stage.mkdir(parents=True)
            marker = stage / "preserve.txt"
            marker.write_text("user content")
            link = root / "addons"
            if os.name == "nt":
                subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(victim)],
                               capture_output=True, check=True, timeout=30)
            else:
                link.symlink_to(victim, target_is_directory=True)
            try:
                for operation in ("--remove-tree", "--extract-file"):
                    with self.subTest(operation=operation):
                        result = subprocess.run([binary, operation, str(link / "stage")],
                                                capture_output=True, timeout=30)
                        self.assertEqual(1, result.returncode, result.stderr)
                        self.assertEqual("user content", marker.read_text())
                        self.assertFalse((stage / "exclusive.txt").exists())
                subprocess.run([binary, "--extract-file", str(stage)], check=True, timeout=30)
            finally:
                if os.name == "nt":
                    os.rmdir(link)
                else:
                    link.unlink()

    def test_preexisting_directory_survives_concurrent_native_runs(self):
        binary = os.environ["UPDATE_SECURITY_TEST_BINARY"]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            preexisting = root / "redotchestrator-security-test"
            preexisting.mkdir()
            marker = preexisting / "preserve.txt"
            marker.write_text("user content")
            environment = dict(os.environ, TEMP=temporary, TMP=temporary, TMPDIR=temporary)
            processes = [subprocess.Popen([binary], env=environment, stdout=subprocess.PIPE,
                                          stderr=subprocess.PIPE) for _ in range(4)]
            try:
                for process in processes:
                    output, errors = process.communicate(timeout=30)
                    self.assertEqual(0, process.returncode, errors or output)
                self.assertEqual("user content", marker.read_text())
                self.assertEqual([preexisting], list(root.iterdir()))
            finally:
                for process in processes:
                    if process.poll() is None:
                        process.kill()
                        process.communicate()


if __name__ == "__main__":
    unittest.main()
