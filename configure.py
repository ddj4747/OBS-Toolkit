import os
import shutil
import sys
import subprocess
import platform
import tarfile
import tempfile
import urllib.request

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

common_build_missing = "--build=missing"

obs_dir = os.environ["OBS_STUDIO_PATH"]

if not os.path.exists(obs_dir):
    print(f"OBS_STUDIO_PATH points to a path that does not exist: \"{obs_dir}\"")
    pause()
    sys.exit(-1)


def install_cloudeflared():
    release_link = 'https://github.com/cloudflare/cloudflared/releases/tag/2026.8.2'
    install_directory = './.deps/'
    executable_name = 'cloudeflared' + ('.exe' if current_platform == "win32" else '')

    version = release_link.rstrip('/').split('/')[-1]
    base_url = f'https://github.com/cloudflare/cloudflared/releases/download/{version}'
    machine = platform.machine().lower()

    if current_platform == "win32":
        if machine not in ('amd64', 'x86_64'):
            print(f'Unsupported architecture for cloudflared: {machine}')
            sys.exit(-1)
        asset = 'cloudflared-windows-amd64.exe'
        is_archive = False
    elif current_platform == "darwin":
        asset = (
            'cloudflared-darwin-arm64.tgz'
            if machine == 'arm64'
            else 'cloudflared-darwin-amd64.tgz'
        )
        is_archive = True
    elif current_platform.startswith('linux'):
        if machine in ('x86_64', 'amd64'):
            asset = 'cloudflared-linux-amd64'
        elif machine in ('aarch64', 'arm64'):
            asset = 'cloudflared-linux-arm64'
        else:
            print(f'Unsupported architecture for cloudflared: {machine}')
            sys.exit(-1)
        is_archive = False
    else:
        print(f'Unsupported platform for cloudflared: {current_platform}')
        sys.exit(-1)

    os.makedirs(install_directory, exist_ok=True)
    dest_path = os.path.join(install_directory, executable_name)
    download_url = f'{base_url}/{asset}'

    print(f'Downloading cloudflared from {download_url}')

    try:
        with tempfile.TemporaryDirectory() as tmpdir:
            download_path = os.path.join(tmpdir, asset)
            urllib.request.urlretrieve(download_url, download_path)

            if is_archive:
                with tarfile.open(download_path, 'r:gz') as tar:
                    tar.extractall(tmpdir)

                extracted = None
                for root, _, files in os.walk(tmpdir):
                    for name in files:
                        if name == 'cloudflared':
                            extracted = os.path.join(root, name)
                            break
                    if extracted:
                        break

                if not extracted:
                    print('Failed to find cloudflared binary in archive')
                    sys.exit(-1)

                shutil.copy2(extracted, dest_path)
            else:
                shutil.copy2(download_path, dest_path)

        if current_platform != 'win32':
            os.chmod(dest_path, 0o755)

        print(f'Installed cloudflared to {dest_path}')
    except Exception as e:
        print(f'Failed to install cloudflared: {e}')
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

install_cloudeflared()
run_conan_install("Release")

disable_debug = os.environ.get("DISABLE_DEBUG", "").lower()
if disable_debug not in ("true", "1", "yes", "on"):
    run_conan_install("Debug")
