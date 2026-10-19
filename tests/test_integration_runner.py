import contextlib
import importlib.util
import io
import subprocess
import unittest
from pathlib import Path
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location(
    "integration_runner", Path(__file__).with_name("run_integration_tests.py"))
RUNNER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUNNER)
RUNNER.redot_path = Path("redot-test")


class ImportResultTests(unittest.TestCase):
    def test_clean_import_is_bounded_and_uses_dummy_audio(self):
        result = subprocess.CompletedProcess([], 0, "", "")
        with patch.object(RUNNER.subprocess, "run", return_value=result) as execute:
            RUNNER.import_project()
        command = execute.call_args.args[0]
        self.assertGreaterEqual(int(command[command.index("--quit-after") + 1]), 2)
        self.assertEqual("Dummy", command[command.index("--audio-driver") + 1])
        self.assertGreater(execute.call_args.kwargs["timeout"], 0)

    def test_exit_zero_with_import_error_is_rejected(self):
        result = subprocess.CompletedProcess([], 0, "", "ERROR: resource failed\n")
        diagnostics = io.StringIO()
        with patch.object(RUNNER.subprocess, "run", return_value=result), \
                contextlib.redirect_stderr(diagnostics), self.assertRaises(SystemExit) as failure:
            RUNNER.import_project()
        self.assertEqual(1, failure.exception.code)
        self.assertIn("ERROR: resource failed", diagnostics.getvalue())

    def test_signed_windows_crash_is_reported_without_signal_lookup_failure(self):
        result = subprocess.CompletedProcess([], -1073741819, "", "shutdown crash\n")
        diagnostics = io.StringIO()
        with patch.object(RUNNER.subprocess, "run", return_value=result), \
                contextlib.redirect_stderr(diagnostics), self.assertRaises(SystemExit) as failure:
            RUNNER.import_project()
        self.assertEqual(1, failure.exception.code)
        self.assertIn("-1073741819", diagnostics.getvalue())


if __name__ == "__main__":
    unittest.main()
