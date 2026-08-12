"""
Run the redotchestrator integration tests against an explicit Redot binary.

Copies the addon into the test project, imports the project, then runs every scene
under scenes/ and compares its output against the matching .out file. The runner
never downloads an engine: callers must provide the exact Redot build under test.

Usage:
    python3 run_integration_tests.py [options]

Options:
    -k, --filter SUBSTR   Only run scenes whose path (relative to scenes/) contains
                          SUBSTR. Handy for iterating on a single failing test.
    --no-color            Disable colored output (also auto-disabled when stdout is
                          not a TTY, e.g. in CI logs).
    --version X.Y         Compatibility API version used for fixture selection.
    --redot-binary PATH   Exact Redot executable to test (or set REDOT_BIN).
    -j, --jobs N          Number of scenes to run in parallel. Defaults to the CPU
                          count, capped at 4; use -j 1 to force sequential runs.
    -h, --help            Show the argparse-generated help and exit.

Exit code is 0 when all tests pass (or skip), and 1 if any test fails, crashes, or
hits an unknown directive.
"""

import argparse
import os
import time
import re
import shutil
import signal
import subprocess
import sys

from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

MAX_JOBS = 4
IMPORT_TIMEOUT_SECONDS = 120
SCENE_TIMEOUT_SECONDS = 30

scenes_dir = (Path(__file__).parent / "scenes").resolve()

use_color = sys.stdout.isatty()
GREEN  = "\033[32m"
RED    = "\033[31m"
YELLOW = "\033[33m"
RESET  = "\033[0m"

STATUS_COLORS = {
    "PASS":  GREEN,
    "FAIL":  RED,
    "CRASH": RED,
    "ERROR": RED,
    "SKIP":  YELLOW,
}

counts = {"PASS": 0, "FAIL": 0, "CRASH": 0, "ERROR": 0, "SKIP": 0}

def color(text, c):
    return f"{c}{text}{RESET}" if use_color else text

def format_result(status, elapsed, scene_file):
    label = color(f"{status:<6}", STATUS_COLORS[status])
    rel = scene_file.resolve().relative_to(scenes_dir)
    return f"{label}  ({elapsed:.2f}s)  {rel}"

def summary_part(n, label, c):
    text = f"{n} {label}"
    return color(text, c) if n else text

def print_summary(total_elapsed):
    total = sum(counts.values())
    print("-" * 80)
    parts = [
        summary_part(counts["PASS"],  "passed",  GREEN),
        summary_part(counts["FAIL"],  "failed",  RED),
        summary_part(counts["CRASH"], "crashed", RED),
        summary_part(counts["ERROR"], "errored", RED),
        summary_part(counts["SKIP"],  "skipped", YELLOW),
    ]
    print(", ".join(parts) + f"  ({total} total in {total_elapsed:.2f}s)")

def truncate(text, max_lines=30):
    lines = text.splitlines()
    if len(lines) <= max_lines:
        return text
    omitted = len(lines) - max_lines
    return "\n".join(lines[-max_lines:] + [f"... {omitted} more line(s) omitted"])

def process_output(value):
    if value is None:
        return ""
    if isinstance(value, bytes):
        return value.decode(errors="replace")
    return value

def parse_version(v):
    return tuple(int(x) for x in v.split(".")[:2])

def read_meta(scene_file):
    """Parse the optional sibling .meta file into a key/value dict.
    Returns an empty dict when no .meta file exists.
    """
    meta_file = scene_file.with_suffix(".meta")
    if not meta_file.exists():
        return {}

    meta = {}
    for line in meta_file.read_text().splitlines():
        # Allow full-line comments (starting with #) and blank lines.
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if ":" in line:
            key, value = line.split(":", 1)
            # Allow an inline trailing comment after the value.
            value = value.split("#", 1)[0]
            meta[key.strip()] = value.strip()
    return meta

def is_version_supported(version, scene_file):
    meta = read_meta(scene_file)
    if not meta:
        # If no meta exists, the test is acceptable to run on all versions
        return True

    current = parse_version(version)
    if "min_version" in meta and current < parse_version(meta["min_version"]):
        return False
    if "max_version" in meta and current > parse_version(meta["max_version"]):
        return False

    return True

def strip_backtrace(text):
    return "\n".join(
        line for line in text.splitlines()
        if not line.startswith("   OScript backtrace") and not line.startswith("       [")
    ).strip()

def normalize_cpp_lines(text):
    text = text.replace("\\", "/")
    # MSVC diagnostics include the absolute checkout path and the owning C++
    # class, while GCC/Clang use the repository-relative file and bare method.
    # Preserve the source file and method while removing those toolchain-only
    # presentation differences.
    text = re.sub(r'\([^()\n]*?/(src/[^():\n]+\.cpp)(?::\d+)?\)', r'(\1)', text)
    text = re.sub(r'(\w+\.cpp):\d+', r'\1', text)
    text = re.sub(r'(?m)^(\s*at: )(?:[A-Za-z_]\w*::)+([~A-Za-z_]\w*)( \()', r'\1\2\3', text)
    return text

def validate_output(source, result, elapsed):
    source = source.resolve()
    out_file = source.with_suffix(".out")

    if not out_file.exists():
        return "SKIP", format_result("SKIP", elapsed, source) + \
            "\n  No .out file was defined, test will be skipped."

    lines = out_file.read_text().strip().splitlines()
    if len(lines) == 0:
        return "FAIL", format_result("FAIL", elapsed, source) + \
            "\n  The .out file is empty, test failed."

    # First line is the directive
    directive = lines[0].strip()
    expected = "\n".join(lines[1:]).strip()

    if directive == "OSCRIPT_TEST_PASS":
        stderr = result.stderr.strip()
        if stderr:
            return "FAIL", format_result("FAIL", elapsed, source) + \
                f"\nExpected empty stderr, but got:\n----------\n{stderr}\n"
        actual = result.stdout.strip()
    elif directive == "OSCRIPT_TEST_FAILURE":
        actual = result.stderr.strip()
        # Godot did not add backtrace support until Godot 4.5+
        if version == "4.4":
            actual = strip_backtrace(actual)
            expected = strip_backtrace(expected)

        actual = normalize_cpp_lines(actual)
        expected = normalize_cpp_lines(expected)

    else:
        return "ERROR", format_result("ERROR", elapsed, source) + \
            f"\n  Unknown directive '{directive}' in {out_file}"

    if actual == expected:
        return "PASS", format_result("PASS", elapsed, source)
    else:
        return "FAIL", format_result("FAIL", elapsed, source) + \
            f"\nExpected:\n----------\n{expected}\nOutput:\n----------\n{actual}\n"

def clean_godot_cache():
    godot_cache = Path(__file__).parent / ".godot"
    if godot_cache.exists():
        shutil.rmtree(godot_cache)

def import_project():
    try:
        result = subprocess.run(
            [
                redot_path,
                "--no-header",
                "--headless",
                "--path",
                Path(__file__).parent,
                "--import",
                "--quiet"],
            capture_output=True,
            text=True,
            timeout=IMPORT_TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired as error:
        print(f"Project import timed out after {IMPORT_TIMEOUT_SECONDS} seconds.", file=sys.stderr)
        stdout = process_output(error.stdout).strip()
        stderr = process_output(error.stderr).strip()
        if stdout:
            print("---------- stdout ----------", file=sys.stderr)
            print(stdout, file=sys.stderr)
        if stderr:
            print("---------- stderr ----------", file=sys.stderr)
            print(stderr, file=sys.stderr)
        sys.exit(1)

    if result.returncode != 0:
        # A segfault surfaces as a negative return code (e.g. -11 for SIGSEGV).
        # Surface whatever the binary emitted so the crash can be diagnosed
        # instead of failing silently on a discarded DEVNULL.
        signal_name = ""
        if result.returncode < 0:
            signal_name = f" ({signal.Signals(-result.returncode).name})"
        print(f"Project import failed with exit code {result.returncode}"
              f"{signal_name}", file=sys.stderr)
        if result.stdout.strip():
            print("---------- stdout ----------", file=sys.stderr)
            print(result.stdout.rstrip(), file=sys.stderr)
        if result.stderr.strip():
            print("---------- stderr ----------", file=sys.stderr)
            print(result.stderr.rstrip(), file=sys.stderr)
        sys.exit(1)

def run_scene(scene_file):
    meta = read_meta(scene_file)

    # A scene may opt into a fixed timestep so that physics frames are deterministic.
    # Without --fixed-fps the main loop free-runs against the wall clock, so an unpredictable number of idle
    # (aka _process) frames elapse before the first physics (_physics_process) tick.
    # This makes physics lifecycle output impossible to pin down if an .out record needs an explicit order.
    frame_args = []
    if "fixed_fps" in meta:
        frame_args += ["--fixed-fps", meta["fixed_fps"]]

    # Number of main-loop iterations before Redot force-quits.
    # Defaults to 2.
    # Physics scenes typically pair this with fixed_fps and quit_after: 1 to capture a single deterministic frame.
    frame_args += ["--quit-after", meta.get("quit_after", "2")]

    start = time.monotonic()
    try:
        result = subprocess.run(
            [
                redot_path,
                "--no-header",
                "--headless",
                "--path",
                Path(__file__).parent,
                *frame_args,
                "--scene",
                str(scene_file)],
            capture_output=True,
            check=True,
            text=True,
            timeout=SCENE_TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired as error:
        elapsed = time.monotonic() - start
        text = format_result("CRASH", elapsed, scene_file)
        text += f"\n  timed out after {SCENE_TIMEOUT_SECONDS} seconds"
        stdout = process_output(error.stdout).strip()
        stderr = process_output(error.stderr).strip()
        if stdout:
            text += "\n" + truncate(stdout)
        if stderr:
            text += "\n" + truncate(stderr)
        return "CRASH", text + "\n"
    except subprocess.CalledProcessError as e:
        elapsed = time.monotonic() - start
        text = format_result("CRASH", elapsed, scene_file)
        text += f"\n  exit code {e.returncode}"
        if e.stdout.strip():
            text += "\n" + truncate(e.stdout.strip())
        if e.stderr.strip():
            text += "\n" + truncate(e.stderr.strip())
        return "CRASH", text + "\n"

    elapsed = time.monotonic() - start
    return validate_output(scene_file, result, elapsed)

def test_scenes(version, name_filter, jobs):
    scenes = []
    for scene_file in sorted(scenes_dir.rglob("*.tscn")):
        rel = scene_file.relative_to(scenes_dir)
        if name_filter and name_filter not in str(rel):
            continue
        if not is_version_supported(version, scene_file.resolve()):
            continue
        scenes.append(scene_file)

    with ThreadPoolExecutor(max_workers=jobs) as executor:
        # map() preserves submission order, so output stays deterministic
        # regardless of which scene finishes first.
        for status, text in executor.map(run_scene, scenes):
            counts[status] += 1
            print(text)

def atomic_copy(src, dst):
    src = Path(src)
    dst = Path(dst)
    for source_file in src.rglob("*"):
        if source_file.is_dir():
            continue
        dest_file = dst / source_file.relative_to(src)
        dest_file.parent.mkdir(parents=True, exist_ok=True)
        if dest_file.exists():
            dest_file.unlink()
        shutil.copy2(source_file, dest_file)
def update_libraries():
    atomic_copy(
    # shutil.copytree(
        Path(__file__).parent / "../project/addons/orchestrator",
        Path(__file__).parent / "addons/orchestrator")
      #  dirs_exist_ok=True)

def get_minimum_godot_version():
    gdext_path = Path(__file__).parent / "../project/addons/orchestrator/orchestrator.gdextension"
    content = gdext_path.read_text()
    match = re.search(r'compatibility_minimum="([^"]+)"', content)
    if not match:
        raise RuntimeError("Could not find compatibility_minimum in .gdextension file")
    return match.group(1)

def parse_args():
    parser = argparse.ArgumentParser(
        description="Run redotchestrator integration tests against Redot.")
    parser.add_argument(
        "-k", "--filter", dest="filter", default=None,
        help="Only run scenes whose path (relative to scenes/) contains this substring.")
    parser.add_argument(
        "--no-color", action="store_true",
        help="Disable colored output.")
    parser.add_argument(
        "--version", dest="version", default=None,
        help="Compatibility API version for fixture selection "
             "(defaults to the .gdextension minimum).")
    parser.add_argument(
        "--redot-binary", dest="redot_binary", default=None,
        help="Path to the exact Redot binary under test "
             "(also settable via REDOT_BIN).")
    default_jobs = min(os.cpu_count() or 1, MAX_JOBS)
    parser.add_argument(
        "-j", "--jobs", type=int, default=default_jobs,
        help=f"Number of scenes to run in parallel "
             f"(default: {default_jobs}, capped at {MAX_JOBS}).")
    return parser.parse_args()

def main():
    global version, redot_path, use_color

    args = parse_args()
    use_color = sys.stdout.isatty() and not args.no_color

    binary = args.redot_binary or os.environ.get("REDOT_BIN")
    if not binary:
        print("Redot binary required: pass --redot-binary or set REDOT_BIN.",
              file=sys.stderr)
        sys.exit(2)

    redot_path = Path(binary).expanduser().resolve()
    if not redot_path.is_file():
        print(f"Redot binary not found: {redot_path}", file=sys.stderr)
        sys.exit(1)

    version = args.version or get_minimum_godot_version()
    print(f"Using Redot binary {redot_path}")

    update_libraries()
    clean_godot_cache()
    import_project()

    jobs = max(1, min(args.jobs, MAX_JOBS))

    run_start = time.monotonic()
    test_scenes(version, args.filter, jobs)
    total_elapsed = time.monotonic() - run_start

    print_summary(total_elapsed)

    sys.exit(1 if counts["FAIL"] or counts["CRASH"] or counts["ERROR"] else 0)

if __name__ == "__main__":
    main()
