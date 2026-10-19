"""Bounded Redot runtime probe for the inflater used by the updater.

Run with REDOT_BIN set to each supported engine; the caller supplies a task-owned TEMP.
"""
import gzip
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


@unittest.skipUnless(os.environ.get("REDOT_BIN"), "REDOT_BIN is required for the runtime probe")
class UpdateInflateTests(unittest.TestCase):
    def test_inflate_is_bounded_and_crc_errors_fail(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "project.godot").write_text('[application]\nconfig/name="Update inflater test"\n')
            # Gzip contains the same raw DEFLATE bytes, CRC and size wrapper as ZIP extraction.
            good = gzip.compress(b"payload", mtime=0)
            (root / "good.gz").write_bytes(good)
            (root / "bomb.gz").write_bytes(gzip.compress(b"x" * 65536, mtime=0))
            bad = bytearray(good)
            bad[-8] ^= 1
            (root / "bad.gz").write_bytes(bad)
            (root / "probe.gd").write_text('''extends SceneTree
func _init():
    var good = FileAccess.get_file_as_bytes("res://good.gz")
    if good.decompress_dynamic(7, FileAccess.COMPRESSION_GZIP).get_string_from_utf8() != "payload":
        quit(1)
        return
    var bomb = FileAccess.get_file_as_bytes("res://bomb.gz")
    if not bomb.decompress_dynamic(16, FileAccess.COMPRESSION_GZIP).is_empty():
        quit(2)
        return
    var bad = FileAccess.get_file_as_bytes("res://bad.gz")
    if not bad.decompress_dynamic(7, FileAccess.COMPRESSION_GZIP).is_empty():
        quit(3)
        return
    print("UPDATE_INFLATE_OK")
    quit()
''')
            result = subprocess.run([os.environ["REDOT_BIN"], "--headless", "--audio-driver", "Dummy",
                                     "--quit-after", "3", "--path", str(root), "--script", "probe.gd"],
                                    capture_output=True, text=True, timeout=30)
            self.assertEqual(0, result.returncode)
            self.assertIn("UPDATE_INFLATE_OK", result.stdout)
            # The two deliberately invalid streams must produce the engine's explicit errors.
            self.assertEqual(2, result.stderr.count("ERROR: Decompression failed."))
            self.assertNotIn("SCRIPT ERROR", result.stderr)


if __name__ == "__main__":
    unittest.main()
