#!/usr/bin/env python3

import os
import shutil
import sys
import subprocess
import platform
import tempfile
import urllib.request
import zipfile

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

# ------------------------------------------------
# OBS_STUDIO_PATH
# ------------------------------------------------
# Full path to your OBS Studio installation directory.
#
# Example (Windows):
#   OBS_STUDIO_PATH=C:\\Program Files\\obs-studio
#
# Example (Linux):
#   OBS_STUDIO_PATH=/usr/bin/obs
#
# Example (macOS):
#   OBS_STUDIO_PATH=/Applications/OBS.app
#
OBS_STUDIO_PATH=

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
check_env_var("OBS_STUDIO_PATH")

current_platform = sys.platform

if current_platform == "win32":
    cppstd = "20"
    extra_flags = ""
elif current_platform.startswith("linux"):
    cppstd = "gnu20"
    extra_flags = "-c tools.system.package_manager:mode=install -c tools.system.package_manager:sudo=True"
elif current_platform == "darwin":
    cppstd = "20"
    extra_flags = ""
else:
    cppstd = "20"
    extra_flags = ""

required_packages = []
package_update_cmd = None
package_install_cmd = None

if current_platform.startswith("linux"):
    os_ids = set()

    try:
        with open("/etc/os-release", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                key, _, value = line.partition("=")
                value = value.strip().strip("\"'")
                if key == "ID":
                    os_ids.add(value)
                elif key == "ID_LIKE":
                    os_ids.update(value.split())
    except OSError:
        pass

    if os_ids & {"fedora", "rhel", "centos"}:
        required_packages = [
            "gcc-c++",
            "cmake",
            "ninja-build",
            "pkgconf-pkg-config",
            "obs-studio",
            "obs-studio-devel",
            "qt6-qtbase-devel",
            "ffmpeg-free-devel",
            "simde-devel",
        ]
        package_install_cmd = ["sudo", "dnf", "install", "-y"]
    elif os_ids & {"debian", "ubuntu"}:
        required_packages = [
            "build-essential",
            "cmake",
            "ninja-build",
            "pkg-config",
            "obs-studio",
            "libobs-dev",
            "qt6-base-dev",
            "libavcodec-dev",
            "libavformat-dev",
            "libavutil-dev",
            "libswscale-dev",
            "libsimde-dev",
        ]
        package_update_cmd = ["sudo", "apt-get", "update"]
        package_install_cmd = ["sudo", "apt-get", "install", "-y"]

common_build_missing = "--build=missing"

obs_dir = os.environ["OBS_STUDIO_PATH"]

if not os.path.exists(obs_dir):
    print(f"OBS_STUDIO_PATH points to a path that does not exist: \"{obs_dir}\"")
    pause()
    sys.exit(-1)


def install_required_packages():
    if not required_packages:
        return

    if package_update_cmd:
        print("Refreshing package lists...")
        result = subprocess.run(package_update_cmd)
        if result.returncode != 0:
            print(f"Failed to refresh package lists (exit {result.returncode})")
            pause()
            sys.exit(result.returncode)

    cmd = package_install_cmd + required_packages
    print(f"Installing required packages: {' '.join(required_packages)}")

    result = subprocess.run(cmd)
    if result.returncode != 0:
        print(f"Failed to install required packages (exit {result.returncode})")
        pause()
        sys.exit(result.returncode)


def install_go_irl():
    release_link = 'https://github.com/e04/go-irl/releases/tag/v2.4.0'
    install_directory = './.deps/'
    executable_name = 'go-irl' + ('.exe' if current_platform == "win32" else '')

    version = release_link.rstrip('/').split('/')[-1]
    base_url = f'https://github.com/e04/go-irl/releases/download/{version}'
    machine = platform.machine().lower()

    if current_platform == "win32":
        asset = 'go-irl-windows-x64.zip'
    elif current_platform == "darwin":
        asset = 'go-irl-macos-arm64.zip'
    elif current_platform.startswith('linux') and machine in ('x86_64', 'amd64'):
        asset = 'go-irl-linux-x64.zip'
    elif current_platform.startswith('linux') and machine in ('aarch64', 'arm64'):
        asset = 'go-irl-linux-arm64.zip'
    else:
        print(f'Unsupported platform for go-irl: {current_platform}')
        sys.exit(-1)

    os.makedirs(install_directory, exist_ok=True)
    dest_path = os.path.join(install_directory, executable_name)
    download_url = f'{base_url}/{asset}'

    try:
        with tempfile.TemporaryDirectory() as tmpdir:
            download_path = os.path.join(tmpdir, asset)
            urllib.request.urlretrieve(download_url, download_path)

            with zipfile.ZipFile(download_path) as archive:
                archive.extractall(tmpdir)

            extracted = None

            for root, _, files in os.walk(tmpdir):
                for name in files:
                    if name == executable_name:
                        extracted = os.path.join(root, name)
                        break

                if extracted:
                    break

            if not extracted:
                print('Failed to find go-irl binary in archive')
                sys.exit(-1)

            shutil.copy2(extracted, dest_path)

        if current_platform != 'win32':
            os.chmod(dest_path, 0o755)

        print(f'Installed go-irl to {dest_path}')
    except Exception as e:
        print(f'Failed to install go-irl: {e}')
        sys.exit(-1)


def run_conan_install(build_type: str):
    cmd_parts = [
        "conan",
        "install",
        ".",
        "-c", f'user.plugin:obs_location="{obs_dir}"',
        common_build_missing,
        f"-s compiler.cppstd={cppstd}",
        f"-s build_type={build_type}"
    ]

    if current_platform == "win32":
        cmd_parts.append("-s compiler.runtime=dynamic")

    if extra_flags:
        cmd_parts.append(extra_flags)

    cmd = " ".join(cmd_parts)
    print(f"Running: {cmd}")

    result = subprocess.run(cmd, shell=True, env=os.environ)
    if result.returncode != 0:
        print(f"conan install failed for build_type={build_type} (exit {result.returncode})")
        sys.exit(result.returncode)


shutil.rmtree("build", ignore_errors=True)

print(f"Using OBS location: {obs_dir}")

install_required_packages()
install_go_irl()
run_conan_install("Release")

disable_debug = os.environ.get("DISABLE_DEBUG", "").lower()
if disable_debug not in ("true", "1", "yes", "on"):
    run_conan_install("Debug")
