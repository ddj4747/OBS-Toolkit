import os
import sys
import subprocess

env_file_path = ".env"
default_env_content = """
# ==============================
# Project Environment Variables
# ==============================
#
# This file defines environment variables used by the build system
# (CMake, Conan, deployment tools, and platform-specific scripts).
#
# Fill in the values below and save this file.
#

# ------------------------------------------------
# DISABLE_DEBUG
# ------------------------------------------------
# Controls whether the Debug configuration is built.
#
# true / 1 / yes / on  -> Debug disabled
# false / 0 / no / off -> Debug enabled
#
# Example:
#   DISABLE_DEBUG=false
#
DISABLE_DEBUG=

# ------------------------------------------------
# BUILD_TESTS
# ------------------------------------------------
# Controls whether test targets are built.
#
# true / 1 / yes / on  -> Build tests
# false / 0 / no / off -> Skip tests
#
# Notes:
# - On Desktop: controls C++ test targets.
# - On Android: controls mobile test APK targets.
#
# Example:
#   BUILD_TESTS=true
#
BUILD_TESTS=true

# ------------------------------------------------
# QT_DIR
# ------------------------------------------------
# Full path to your Qt installation for desktop builds.
#
# Example (Windows, Qt 6.8.3, MSVC 2022 64-bit):
#   QT_DIR=C:\\Qt\\6.8.3\\msvc2022_64
#
# Example (Linux):
#   QT_DIR=/opt/Qt/6.8.3/gcc_64
#
QT_DIR=

"""

def pause():
    if os.environ.get("CI"):
        return
    input("Press Enter to continue...")


if not os.path.exists(env_file_path):
    with open(env_file_path, "w") as f:
        f.write(default_env_content)
    print(f"Fill env properties in: \"{env_file_path}\".")
    pause()
    sys.exit(-1)

# Load .env
with open(env_file_path, "r") as f:
    for line in f:
        line = line.strip()
        if line and not line.startswith("#"):
            key, _, value = line.partition("=")
            os.environ[key.strip()] = value.strip()

def check_env_var(name: str):
    if not os.environ.get(name):
        print(f"{name} is not set in \"{env_file_path}\"! Fill it before running this script.")
        pause()
        sys.exit(-1)

check_env_var("DISABLE_DEBUG")
check_env_var("QT_DIR")
check_env_var("BUILD_TESTS")

platform = sys.platform

if platform == "win32":
    cppstd = "20"
    extra_flags = ""
elif platform.startswith("linux"):
    cppstd = "gnu20"
    extra_flags = "-c tools.system.package_manager:mode=install -c tools.system.package_manager:sudo=True"
elif platform == "darwin":
    cppstd = "20"
    extra_flags = ""
else:
    cppstd = "20"
    extra_flags = ""

common_generator_flags = "-g CMakeToolchain -g CMakeDeps"
common_build_missing = "--build=missing"

def run_conan_install(build_type: str):
    cmd_parts = [
        "conan",
        "install",
        ".",
        "-c", "tools.cmake.cmaketoolchain:generator=Ninja",
        common_build_missing,
        common_generator_flags,
        f"-s compiler.cppstd={cppstd}",
        f"-s build_type={build_type}"
    ]

    if platform == "win32":
        cmd_parts.append("-s compiler.runtime=dynamic")

    if extra_flags:
        cmd_parts.append(extra_flags)

    cmd = " ".join(cmd_parts)
    print(f"Running: {cmd}")

    result = subprocess.run(cmd, shell=True, env=os.environ)
    if result.returncode != 0:
        print(f"conan install failed for build_type={build_type} (exit {result.returncode})")
        sys.exit(result.returncode)


run_conan_install("Release")

disable_debug = os.environ.get("DISABLE_DEBUG", "").lower()
if disable_debug not in ("true", "1", "yes", "on"):
    run_conan_install("Debug")