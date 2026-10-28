import argparse
import io
from pathlib import Path
import tarfile
import urllib.request
import zipfile


def download(url):
    with urllib.request.urlopen(url) as response:
        return io.BytesIO(response.read())


def install_binary(data, name, destination):
    if name == "go-irl":
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
            content = archive.extractfile(matches[0]).read()
    path = destination / name
    path.write_bytes(content)
    path.chmod(0o755)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("architecture", choices=("amd64", "arm64"))
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    args.destination.mkdir(parents=True, exist_ok=True)
    go_arch = "x64" if args.architecture == "amd64" else "arm64"
    go_url = f"https://github.com/ddj4747/go-irl/releases/download/v1.1.1/go-irl-linux-{go_arch}.zip"
    media_url = f"https://github.com/bluenviron/mediamtx/releases/download/v1.20.1/mediamtx_v1.20.1_linux_{args.architecture}.tar.gz"
    install_binary(download(go_url), "go-irl", args.destination)
    install_binary(download(media_url), "mediamtx", args.destination)


if __name__ == "__main__":
    main()
