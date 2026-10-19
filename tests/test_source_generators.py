import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent


class GeneratorDeterminismTests(unittest.TestCase):
    def test_generated_sources_ignore_python_hash_seed(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            xml = root / "example.xml"
            xml.write_text('<class name="Example"></class>')
            api = root / "extension_api.json"
            api.write_text('{"classes": []}')
            templates = root / "templates" / "Node"
            templates.mkdir(parents=True)
            (templates / "example.ini").write_text('[meta]\ndescription = Example\n')
            (templates / "example.torch").write_text('example template')
            for generator in ("godot_docs", "godot_api", "script_templates"):
                outputs = []
                for seed in ("1", "2"):
                    target = root / f"{generator}-{seed}.cpp"
                    if generator == "godot_docs":
                        arguments = [str(target), str(xml)]
                    elif generator == "godot_api":
                        arguments = [str(api), str(root / seed)]
                        target = root / seed / "api_data.cpp"
                    else:
                        arguments = [str(templates.parent), str(target)]
                    subprocess.run(
                        [sys.executable, str(ROOT / "cmake/scripts" / f"generate_{generator}.py"),
                         *arguments],
                        env={**os.environ, "PYTHONHASHSEED": seed},
                        capture_output=True, text=True, check=True, timeout=30)
                    outputs.append(target.read_bytes())
                with self.subTest(generator=generator):
                    self.assertEqual(outputs[0], outputs[1])


if __name__ == "__main__":
    unittest.main()
