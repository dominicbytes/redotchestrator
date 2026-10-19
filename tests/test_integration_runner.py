import contextlib
import importlib.util
import io
import subprocess
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
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

    def test_unknown_rid_leak_types_are_not_tolerated(self):
        stderr = "ERROR: 1 RID allocations of type 'PhysicsServer3D' were leaked at exit.\n"
        result = subprocess.CompletedProcess([], 0, "", stderr)
        with patch.object(RUNNER, "engine_version", "26.3.stable.official.85e520394"), \
                patch.object(RUNNER.subprocess, "run", return_value=result), \
                contextlib.redirect_stdout(io.StringIO()), \
                contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            RUNNER.import_project()

    def test_leaks_are_not_tolerated_on_other_engines_or_after_a_crash(self):
        stderr = "ERROR: 2 RID allocations of type 'N16RendererViewport8ViewportE' were leaked at exit.\n"
        for version, code in (("26.2.stable.official.0", 0), ("26.3.stable.custom_build.0", 0),
                              ("26.3.stable.official.0", -1073741819)):
            result = subprocess.CompletedProcess([], code, "", stderr)
            with self.subTest(version=version, code=code), \
                    patch.object(RUNNER, "engine_version", version), \
                    patch.object(RUNNER.subprocess, "run", return_value=result), \
                    contextlib.redirect_stdout(io.StringIO()), \
                    contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                RUNNER.import_project()


class SceneSelectionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.manifest = self.root / "integration_scenes.txt"
        self.manifest.write_text("features/f_example.tscn\n")
        self.scene = self.root / "features/f_example.tscn"
        self.scene.parent.mkdir()
        self.scene.write_text("")
        self.scene.with_suffix(".out").write_text("OSCRIPT_TEST_PASS\n")
        for attribute, value in [("scenes_dir", self.root),
                                 ("SCENE_MANIFEST", self.manifest),
                                 ("counts", dict.fromkeys(RUNNER.counts, 0))]:
            replacement = patch.object(RUNNER, attribute, value)
            replacement.start()
            self.addCleanup(replacement.stop)
        self.execution = patch.object(RUNNER, "run_scene", return_value=("PASS", "passed"))
        self.execute = self.execution.start()
        self.addCleanup(self.execution.stop)
        self.output = io.StringIO()
        capture = contextlib.redirect_stdout(self.output)
        capture.__enter__()
        self.addCleanup(capture.__exit__, None, None, None)

    def run_scenes(self, version="4.5", name_filter=None):
        RUNNER.test_scenes(version, name_filter, 1)

    def test_missing_output_fails_before_any_scene_runs(self):
        self.scene.with_suffix(".out").unlink()
        self.run_scenes()
        self.assertEqual(1, RUNNER.counts["ERROR"])
        self.execute.assert_not_called()
        self.assertIn("Missing expected output", self.output.getvalue())

    def test_output_deleted_after_preflight_is_an_error(self):
        self.scene.with_suffix(".out").unlink()
        status, _ = RUNNER.validate_output(
            self.scene, subprocess.CompletedProcess([], 0, "", ""), 0)
        self.assertEqual("ERROR", status)

    def test_empty_discovery_fails(self):
        self.scene.unlink()
        self.manifest.write_text("")
        self.run_scenes()
        self.assertEqual(1, RUNNER.counts["ERROR"])
        self.execute.assert_not_called()

    def test_empty_filter_selection_fails(self):
        self.run_scenes(name_filter="no-match")
        self.assertEqual(1, RUNNER.counts["ERROR"])
        self.execute.assert_not_called()

    def test_main_exits_nonzero_for_empty_selection(self):
        args = SimpleNamespace(redot_binary=str(self.scene), no_color=True,
                               version="4.5", skip_import=True, jobs=1,
                               filter="no-match")
        with patch.object(RUNNER, "parse_args", return_value=args), \
                patch.object(RUNNER, "update_libraries"), \
                self.assertRaises(SystemExit) as failure:
            RUNNER.main()
        self.assertEqual(1, failure.exception.code)

    def test_deleted_fixture_fails_inventory_check(self):
        self.scene.unlink()
        self.run_scenes()
        self.assertEqual(1, RUNNER.counts["ERROR"])
        self.execute.assert_not_called()
        self.assertIn("missing: features/f_example.tscn", self.output.getvalue())

    def test_unlisted_fixture_fails_inventory_check(self):
        self.scene.with_name("f_unlisted.tscn").write_text("")
        self.run_scenes()
        self.assertEqual(1, RUNNER.counts["ERROR"])
        self.assertIn("unlisted: features/f_unlisted.tscn", self.output.getvalue())

    def test_version_exclusion_is_reported_and_counted(self):
        excluded = self.scene.with_name("f_new_api.tscn")
        excluded.write_text("")
        excluded.with_suffix(".out").write_text("OSCRIPT_TEST_PASS\n")
        excluded.with_suffix(".meta").write_text("min_version: 4.6\n")
        self.manifest.write_text("features/f_example.tscn\nfeatures/f_new_api.tscn\n")
        self.run_scenes()
        self.assertEqual(1, RUNNER.counts["PASS"])
        self.assertEqual(1, RUNNER.counts["SKIP"])
        self.assertIn("min_version=4.6", self.output.getvalue())

    def test_all_version_excluded_is_an_error(self):
        self.scene.with_suffix(".meta").write_text("max_version: 4.4\n")
        self.run_scenes()
        self.assertEqual(1, RUNNER.counts["SKIP"])
        self.assertEqual(1, RUNNER.counts["ERROR"])
        self.execute.assert_not_called()

    def test_applicable_fixture_runs(self):
        self.run_scenes()
        self.assertEqual(1, RUNNER.counts["PASS"])
        self.assertEqual(0, RUNNER.counts["ERROR"])
        self.execute.assert_called_once_with(self.scene)


if __name__ == "__main__":
    unittest.main()
