"""Run graph/node regressions using existing Windows editor build objects.

Run from an MSVC developer shell after building windows-editor:
    python tests/native/c3_run_node_tests.py --redot-binary "$REDOT_BIN"
The test-only extension and project are created outside the repository.
"""
import argparse
import re
import shutil
import subprocess
import tempfile
from pathlib import Path


def run(command, *, cwd, timeout):
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True, timeout=timeout)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--redot-binary", required=True, type=Path, action="append")
    parser.add_argument("--build-dir", type=Path, default=Path("build/windows-editor"))
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    build = args.build_dir.resolve()
    ninja = (build / "build.ninja").read_text()
    block = re.search(r"build [^\n]*orchestration[\\/]graph.cpp.obj:[^\n]*\n((?:  [^\n]*\n)+)", ninja)[1]

    def setting(name):
        return re.search(r"^  " + name + r" = (.*)$", block, re.M)[1].replace("$:", ":").replace("$ ", " ")

    with tempfile.TemporaryDirectory(prefix="c3-node-tests-") as temporary:
        scratch = Path(temporary)
        compile_response = scratch / "compile.rsp"
        compile_response.write_text(
            "/nologo /c " + setting("DEFINES") + " " + setting("INCLUDES") + " " + setting("FLAGS")
            + ' /d1trimfile:"' + repo.as_posix() + '/" '
            + '"' + str(repo / "tests/native/c3_node_tests.cpp") + '"'
            + ' /Fo"' + str(scratch / "tests.obj") + '"'
        )
        run(["cl", "@" + str(compile_response)], cwd=build, timeout=120)
        objects = sorted((build / "CMakeFiles/orchestrator.dir").rglob("*.obj"))
        if not objects:
            raise RuntimeError("Build the editor preset first")
        bindings = build / "bin/libgodot-cpp.windows.editor.x86_64.lib"
        link_response = scratch / "link.rsp"
        libraries = "kernel32.lib user32.lib gdi32.lib winspool.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib"
        link_response.write_text(
            '/DLL /LTCG /INCREMENTAL:NO /OUT:"' + str(scratch / "c3_tests.dll") + '" '
            + '/PDB:"' + str(scratch / "c3_tests.pdb") + '" '
            + " ".join('"' + str(path) + '"' for path in [scratch / "tests.obj", *objects, bindings])
            + " " + libraries
        )
        run(["link", "@" + str(link_response)], cwd=build, timeout=300)
        addon = scratch / "addons/orchestrator"
        shutil.copytree(repo / "project/addons/orchestrator", addon,
                        ignore=shutil.ignore_patterns("*.gdextension", "*.dll", "*.pdb"))
        (scratch / "project.godot").write_text('config_version=5\n')
        (scratch / "c3.gdextension").write_text(
            '[configuration]\nentry_symbol = "c3_tests_library_init"\ncompatibility_minimum = "4.5"\n'
            '[libraries]\nwindows.editor.x86_64 = "res://c3_tests.dll"\n'
        )
        (scratch / "target.tscn").write_text('[gd_scene format=3]\n[node name="Target" type="Node2D"]\n')
        (scratch / "run.gd").write_text(
            'extends SceneTree\nfunc _initialize():\n'
            '\tvar suite = ClassDB.instantiate("C3NodeTests")\n'
            '\tif not suite.run():\n\t\tquit(1)\n\t\treturn\n'
            '\tprint("C3 native node regressions passed")\n\tquit(0)\n'
        )
        for engine_path in args.redot_binary:
            engine = engine_path.resolve()
            command = [str(engine), "--headless", "--audio-driver", "Dummy", "--path", str(scratch)]
            run(command + ["--editor", "--import", "--quit-after", "600"], cwd=scratch, timeout=120)
            result = run(command + ["--script", "res://run.gd", "--quit-after", "2"], cwd=scratch, timeout=60)
            if result.stderr.strip() or "C3 native node regressions passed" not in result.stdout:
                raise RuntimeError(result.stdout + result.stderr)
            print("C3 native node regressions passed")


if __name__ == "__main__":
    main()
