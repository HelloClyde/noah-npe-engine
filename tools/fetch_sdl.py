"""Fetch the exact SDL2 source used by both native platform packages."""
import hashlib
from pathlib import Path
import tarfile
import urllib.request

VERSION = "2.32.10"
SHA256 = "03f9d7c191a837525c9cda6406af2f2e48be02b5e7eb03d949cc9f1e9ca41c8b"
ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "vendor" / f"SDL-{VERSION}"

def main():
    archive = ROOT / "vendor" / f"SDL-{VERSION}.tar.gz"
    archive.parent.mkdir(parents=True, exist_ok=True)
    if not archive.exists():
        url = f"https://codeload.github.com/libsdl-org/SDL/tar.gz/refs/tags/release-{VERSION}"
        with urllib.request.urlopen(url, timeout=120) as response:
            archive.write_bytes(response.read())
    if hashlib.sha256(archive.read_bytes()).hexdigest() != SHA256:
        raise SystemExit("SDL archive checksum mismatch; remove the archive and retry")
    if not DEST.exists():
        with tarfile.open(archive) as source:
            # SDL includes links; accept only links contained in its pinned root.
            members = source.getmembers()
            for item in members:
                relative = Path(item.name)
                if (relative.is_absolute() or ".." in relative.parts or
                    relative.parts[0] != f"SDL-release-{VERSION}" or
                    not (item.isfile() or item.isdir() or item.issym() or item.islnk())):
                    raise SystemExit(f"Unexpected archive member: {item.name}")
                if item.issym() or item.islnk():
                    base = archive.parent / item.name
                    target = (base.parent / item.linkname if item.issym() else
                              archive.parent / item.linkname).resolve()
                    root = (archive.parent / f"SDL-release-{VERSION}").resolve()
                    if not target.is_relative_to(root):
                        raise SystemExit(f"SDL link escapes source directory: {item.name}")
            source.extractall(archive.parent, members=members, filter="data")
        (archive.parent / f"SDL-release-{VERSION}").rename(DEST)
    print(DEST)

if __name__ == "__main__":
    main()
