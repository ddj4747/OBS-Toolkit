import os
import shutil
import sys
import subprocess
import urllib.request
import zipfile
import tarfile

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
# SETUP_PORTABLE_OBS
# ------------------------------------------------
# If enabled, downloads portable OBS on Windows/macOS.
# On Linux, installs OBS Studio via the native package manager:
#   - Arch: pacman
#   - Fedora: dnf (enables RPM Fusion Free if needed)
#   - Debian/Ubuntu: apt + OBS PPA
#
SETUP_PORTABLE_OBS=

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
check_env_var("SETUP_PORTABLE_OBS")

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

common_build_missing = "--build=missing"


def _resolve_obs_location():
    if sys.platform == "win32":
        return os.path.abspath("obs-portable")

    if sys.platform == "darwin":
        return os.path.abspath("obs-portable")

    if sys.platform.startswith("linux"):
        obs_bin = shutil.which("obs")
        if obs_bin:
            return obs_bin
        return "/usr/bin/obs"

    return os.path.abspath("obs-portable")


obs_dir = _resolve_obs_location()

def _read_os_release():
    info = {}
    try:
        with open("/etc/os-release", encoding="utf-8") as release_file:
            for line in release_file:
                line = line.strip()
                if not line or "=" not in line:
                    continue
                key, value = line.split("=", 1)
                info[key] = value.strip().strip('"')
    except OSError:
        pass
    return info


def _linux_distro_family():
    release = _read_os_release()
    distro_id = release.get("ID", "").lower()
    id_like = [part.lower() for part in release.get("ID_LIKE", "").split()]
    return distro_id, id_like, release


def _install_obs_fedora(release):
    version_id = release.get("VERSION_ID", "")
    if subprocess.run(["rpm", "-q", "obs-studio"], capture_output=True).returncode == 0:
        print("OBS Studio is already installed.")
        return

    package_available = subprocess.run(
        ["dnf", "list", "obs-studio"],
        capture_output=True,
    ).returncode == 0

    if not package_available:
        if not version_id:
            raise RuntimeError("Could not determine Fedora version from /etc/os-release.")
        rpmfusion = (
            "https://download1.rpmfusion.org/free/fedora/"
            f"rpmfusion-free-release-{version_id}.noarch.rpm"
        )
        print(f"Enabling RPM Fusion Free for Fedora {version_id}...")
        subprocess.check_call(["sudo", "dnf", "install", "-y", rpmfusion])

    subprocess.check_call(["sudo", "dnf", "install", "-y", "obs-studio"])


def _install_obs_linux():
    distro_id, id_like, release = _linux_distro_family()

    arch_like = distro_id in (
        "arch",
        "archlinux",
        "endeavouros",
        "manjaro",
        "cachyos",
        "garuda",
    ) or "arch" in id_like
    fedora_like = distro_id == "fedora" or "fedora" in id_like
    debian_like = distro_id in (
        "ubuntu",
        "debian",
        "linuxmint",
        "pop",
    ) or "debian" in id_like or "ubuntu" in id_like

    if arch_like:
        print("Arch-based Linux detected. Installing OBS Studio via pacman...")
        if subprocess.run(["pacman", "-Q", "obs-studio"], capture_output=True).returncode == 0:
            print("OBS Studio is already installed.")
            return
        subprocess.check_call(["sudo", "pacman", "-Sy", "--needed", "--noconfirm", "obs-studio"])
        return

    if fedora_like:
        print("Fedora detected. Installing OBS Studio via dnf...")
        _install_obs_fedora(release)
        return

    if debian_like:
        print("Debian/Ubuntu detected. Installing OBS Studio via apt...")
        subprocess.check_call(["sudo", "add-apt-repository", "-y", "ppa:obsproject/obs-studio"])
        subprocess.check_call(["sudo", "apt", "update"])
        subprocess.check_call(["sudo", "apt", "install", "-y", "obs-studio"])
        return

    raise RuntimeError(
        f"Unsupported Linux distro '{distro_id}'. Install obs-studio manually."
    )

def setup_portable_obs():
    obs_dir = "./obs-portable"
    obs_version = "30.1.2"

    if os.path.exists(obs_dir):
        print(f"Portable OBS already exists at: {obs_dir}")
        return

    if sys.platform.startswith("linux"):
        print("Linux detected. Portable OBS binary is unavailable.")
        try:
            _install_obs_linux()
            print("OBS Studio installed natively on your system.")
        except (subprocess.CalledProcessError, RuntimeError) as error:
            print(f"Failed to install OBS Studio: {error}")
            sys.exit(1)
        return

    print(f"Portable OBS not found. Installing to: {obs_dir}...")
    os.makedirs(obs_dir, exist_ok=True)

    mac_arch = platform.machine()
    mac_dmg = f"OBS-Studio-{obs_version}-macOS-Intel.dmg" if mac_arch == "x86_64" else f"OBS-Studio-{obs_version}-macOS-Apple.dmg"

    urls = {
        "win32": f"https://github.com/obsproject/obs-studio/releases/download/{obs_version}/OBS-Studio-{obs_version}-Full-x64.zip",
        "darwin": f"https://github.com/obsproject/obs-studio/releases/download/{obs_version}/{mac_dmg}"
    }

    url = urls.get(sys.platform)
    if not url:
        print(f"Portable OBS download not supported automatically on platform: {sys.platform}")
        return

    archive_name = url.split("/")[-1]
    archive_path = os.path.join(obs_dir, archive_name)

    try:
        print(f"Downloading OBS from {url}...")
        urllib.request.urlretrieve(url, archive_path)

        print("Extracting archive...")
        if archive_name.endswith(".zip"):
            with zipfile.ZipFile(archive_path, 'r') as zip_ref:
                zip_ref.extractall(obs_dir)
        elif archive_name.endswith(".tar.gz"):
            with tarfile.open(archive_path, 'r:gz') as tar_ref:
                tar_ref.extractall(obs_dir)

        os.remove(archive_path)

        bin_dir = os.path.join(obs_dir, "bin", "64bit")
        if sys.platform == "win32" and os.path.exists(bin_dir):
            portable_marker = os.path.join(bin_dir, "obs_portable_mode.txt")
            with open(portable_marker, "w") as file:
                file.write("")
            print("OBS Portable Mode configured successfully.")

    except Exception as e:
        print(f"Failed to download/extract OBS: {e}")
        if os.path.exists(obs_dir):
            shutil.rmtree(obs_dir)
        sys.exit(-1)

def run_conan_install(build_type: str):
    cmd_parts = [
        "conan",
        "install",
        ".",
        "-c", "tools.cmake.cmaketoolchain:generator=Ninja",
        "-c", f"user.plugin:obs_location={obs_dir}",
        common_build_missing,
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


shutil.rmtree("build", ignore_errors=True)

enable_setup_portable_obs = os.environ.get("SETUP_PORTABLE_OBS", "").lower()
if enable_setup_portable_obs in ("true", "1", "yes", "on"):
    setup_portable_obs()

obs_dir = _resolve_obs_location()
print(f"Using OBS location: {obs_dir}")

run_conan_install("Release")

disable_debug = os.environ.get("DISABLE_DEBUG", "").lower()
if disable_debug not in ("true", "1", "yes", "on"):
    run_conan_install("Debug")