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

    def test_known_263_shutdown_leaks_are_tolerated_on_import_only(self):
        stderr = (
            'WARNING: 2 RIDs of type "Canvas" were leaked.\n'
            '   at: _free_rids (servers/rendering/renderer_canvas_cull.cpp:2698)\n'
            "ERROR: 2 RID allocations of type 'N16RendererViewport8ViewportE' were leaked at exit.\n"
            "WARNING: ObjectDB instances leaked at exit (run with --verbose for details).\n"
            "   at: cleanup (core/object/object.cpp:2568)\n")
        result = subprocess.CompletedProcess([], 0, "", stderr)
        with patch.object(RUNNER, "engine_version", "26.3.stable.official.85e520394"), \
                patch.object(RUNNER.subprocess, "run", return_value=result), \
                contextlib.redirect_stdout(io.StringIO()):
            RUNNER.import_project()

    def test_known_leaks_do_not_hide_other_import_errors(self):
        stderr = ("ERROR: 2 RID allocations of type 'N16RendererViewport8ViewportE' were leaked at exit.\n"
                  "ERROR: resource failed\n")
        result = subprocess.CompletedProcess([], 0, "", stderr)
        diagnostics = io.StringIO()
        with patch.object(RUNNER, "engine_version", "26.3.stable.official.85e520394"), \
                patch.object(RUNNER.subprocess, "run", return_value=result), \
                contextlib.redirect_stdout(io.StringIO()), \
                contextlib.redirect_stderr(diagnostics), self.assertRaises(SystemExit) as failure:
            RUNNER.import_project()
        self.assertEqual(1, failure.exception.code)
        self.assertIn("ERROR: resource failed", diagnostics.getvalue())

    def test_leaks_are_not_tolerated_on_other_engines_or_after_a_crash(self):
        stderr = "ERROR: 2 RID allocations of type 'N16RendererViewport8ViewportE' were leaked at exit.\n"
        for version, code in (("26.2.stable.official.0", 0), ("26.3.stable.official.0", -1073741819)):
            result = subprocess.CompletedProcess([], code, "", stderr)
            with self.subTest(version=version, code=code), \
                    patch.object(RUNNER, "engine_version", version), \
                    patch.object(RUNNER.subprocess, "run", return_value=result), \
                    contextlib.redirect_stdout(io.StringIO()), \
                    contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                RUNNER.import_project()


if __name__ == "__main__":
    unittest.main()
