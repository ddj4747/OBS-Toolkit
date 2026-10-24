"""Format the project with clang-format and gersemi."""

import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SKIP_DIRS = {".git", ".deps", "build", "out", "node_modules"}
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp", ".m", ".mm"}


def project_files():
	sources = []
	cmake_files = []
	for path in ROOT.rglob("*"):
		if not path.is_file():
			continue
		if any(part in SKIP_DIRS for part in path.relative_to(ROOT).parts):
			continue
		if path.suffix.lower() in SOURCE_SUFFIXES:
			sources.append(path)
		elif path.name == "CMakeLists.txt" or path.suffix == ".cmake":
			cmake_files.append(path)
	return sources, cmake_files


def version_of(output):
	match = re.search(r"(\d+)\.(\d+)\.(\d+)", output)
	if not match:
		return None
	return tuple(int(part) for part in match.groups())


def require_clang_format():
	executable = shutil.which("clang-format-22") or shutil.which("clang-format")
	if not executable:
		sys.exit("clang-format 22.1.x was not found")

	output = subprocess.check_output([executable, "--version"], text=True)
	version = version_of(output)
	if version is None or version < (22, 1, 0) or version >= (22, 2, 0):
		found = ".".join(map(str, version)) if version else output.strip()
		sys.exit(f"clang-format 22.1.x is required (found {found})")
	return executable


def require_gersemi():
	executable = shutil.which("gersemi")
	if not executable:
		sys.exit("gersemi 0.12.0 or newer was not found")

	output = subprocess.check_output([executable, "--version"], text=True)
	version = version_of(output)
	if version is None or version < (0, 12, 0):
		found = ".".join(map(str, version)) if version else output.strip()
		sys.exit(f"gersemi 0.12.0 or newer is required (found {found})")
	return executable


def run_formatter(command, files):
	for start in range(0, len(files), 50):
		chunk = [str(path) for path in files[start : start + 50]]
		subprocess.run([*command, *chunk], cwd=ROOT, check=True)


def main():
	sources, cmake_files = project_files()
	run_formatter(
		[require_clang_format(), "-style=file", "-fallback-style=none", "-i"],
		sources,
	)
	run_formatter([require_gersemi(), "-i"], cmake_files)
	print(f"Formatted {len(sources)} source files and {len(cmake_files)} CMake files.")


if __name__ == "__main__":
	main()
