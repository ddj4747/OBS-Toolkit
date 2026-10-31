import argparse
import io
import platform
import sys
import tarfile
import urllib.request
import zipfile
from pathlib import Path


RELEASES = {
    "go-irl": ("ddj4747/go-irl", "v1.1.1"),
    "mediamtx": ("bluenviron/mediamtx", "v1.20.1"),
}


def download(url):
    with urllib.request.urlopen(url) as response:
        return io.BytesIO(response.read())


def install_binary(data, name, destination, archive_name):
    if archive_name.endswith(".zip"):
        with zipfile.ZipFile(data) as archive:
            matches = [entry for entry in archive.namelist() if Path(entry).name == name]
            if len(matches) != 1:
                raise RuntimeError(f"Expected one {name} binary in archive")
            content = archive.read(matches[0])
    else:
        with tarfile.open(fileobj=data, mode="r:gz") as archive:
            matches = [entry for entry in archive.getmembers() if entry.isfile() and Path(entry.name).name == name]
            if len(matches) != 1:
                raise RuntimeError(f"Expected one {name} binary in archive")
            with archive.extractfile(matches[0]) as binary:
                content = binary.read()
    path = destination / name
    path.write_bytes(content)
    if not name.endswith(".exe"):
        path.chmod(0o755)
    print(f"Installed {name} to {path}")


def install_dependencies(destination, target_platform=None, architecture=None):
    target_platform = target_platform or sys.platform
    machine = (architecture or platform.machine()).lower()
    arch = {"x86_64": "amd64", "amd64": "amd64", "aarch64": "arm64", "arm64": "arm64"}.get(machine)
    if target_platform == "win32":
        system, arch = "windows", "amd64"
    elif target_platform == "darwin":
        system, arch = "darwin", "arm64"
    elif target_platform.startswith("linux") and arch:
        system = "linux"
    else:
        raise ValueError(f"Unsupported platform or architecture: {target_platform}, {machine}")

    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    for name, (repository, version) in RELEASES.items():
        if name == "go-irl":
            go_system = "macos" if system == "darwin" else system
            go_arch = "x64" if arch == "amd64" else "arm64"
            asset = f"go-irl-{go_system}-{go_arch}.zip"
        else:
            extension = "zip" if system == "windows" else "tar.gz"
            asset = f"mediamtx_{version}_{system}_{arch}.{extension}"
        url = f"https://github.com/{repository}/releases/download/{version}/{asset}"
        executable = name + (".exe" if system == "windows" else "")
        install_binary(download(url), executable, destination, asset)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("architecture", choices=("amd64", "arm64"))
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    install_dependencies(args.destination, target_platform="linux", architecture=args.architecture)


if __name__ == "__main__":
    main()
