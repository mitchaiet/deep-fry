#!/usr/bin/env python3
"""Create a repeatable corresponding-source archive from an explicit allowlist."""

import argparse
import gzip
import hashlib
import io
import json
import re
import subprocess
import tarfile
from pathlib import Path


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--juce-archive", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--allow-dirty", action="store_true", help="For local checks only")
    args = parser.parse_args()
    project = Path(__file__).resolve().parent.parent
    cmake = (project / "CMakeLists.txt").read_text()
    version = re.search(r"project\(DeepFry VERSION (\d+\.\d+\.\d+) ", cmake).group(1)
    juce_commit = re.search(r"codeload.github.com/juce-framework/JUCE/tar.gz/([a-f0-9]{40})", cmake).group(1)
    juce_sha = re.search(r"URL_HASH SHA256=([a-f0-9]{64})", cmake).group(1)
    juce_version = re.search(r"# JUCE (\d+\.\d+\.\d+)", cmake).group(1)
    upstream_data = args.juce_archive.read_bytes()
    if sha256(upstream_data) != juce_sha:
        parser.error("JUCE archive SHA-256 does not match the immutable CMake pin")

    def git(*arguments):
        return subprocess.check_output(["git", "-C", str(project), *arguments], text=True).strip()

    commit = git("rev-parse", "HEAD")
    epoch = int(git("show", "-s", "--format=%ct", "HEAD"))
    dirty = bool(git("status", "--porcelain"))
    if dirty and not args.allow_dirty:
        parser.error("Commit source changes before packaging a release (or use --allow-dirty for a local check)")

    required = ("CMakeLists.txt", "README.md", "LICENSE", "COPYRIGHT", "CHANGELOG.md", "THIRD_PARTY_NOTICES.md", ".gitignore")
    roots = ("Source", "Tests", "docs", "scripts", "packaging", ".github")
    files = {}
    for relative in required:
        path = project / relative
        if not path.is_file() or path.is_symlink():
            parser.error(f"Missing regular source file: {relative}")
        files[relative] = (path.read_bytes(), 0o644)
    candidates = subprocess.check_output(
        ["git", "-C", str(project), "ls-files", "--cached", "--others", "--exclude-standard", "-z"]
    ).decode().split("\0")
    for name in sorted(set(candidates) - {""}):
        relative = Path(name)
        if relative.parts[0] not in roots:
            continue
        if relative.name == ".DS_Store" or "__pycache__" in relative.parts or relative.suffix == ".pyc":
            continue
        path = project / relative
        if path.is_symlink():
            parser.error(f"Unexpected symlink in source allowlist: {relative}")
        if path.is_file():
            files[relative.as_posix()] = (path.read_bytes(), 0o755 if path.stat().st_mode & 0o111 else 0o644)
    vendor_name = f"vendor/juce-{juce_version}.tar.gz"
    files[vendor_name] = (upstream_data, 0o644)
    manifest = {
        "project": "Deep Fry",
        "version": version,
        "license": "AGPL-3.0-only",
        "source_repository": "https://github.com/mitchaiet/deep-fry",
        "source_commit": commit,
        "source_tree_dirty": dirty,
        "juce": {"version": juce_version, "commit": juce_commit, "archive": vendor_name, "sha256": juce_sha},
        "build": "./scripts/build-offline.sh",
        "files_sha256": {name: sha256(contents) for name, (contents, _) in sorted(files.items())},
    }
    files["SOURCE-MANIFEST.json"] = ((json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode(), 0o644)
    archive_name = f"Deep-Fry-{version}-source"
    args.output.mkdir(parents=True, exist_ok=True)
    archive_path = args.output / f"{archive_name}.tar.gz"
    # Fixed timestamps, ordering, ownership and gzip header make the source
    # archive byte-for-byte repeatable for an unchanged committed source tree.
    with archive_path.open("wb") as output:
        with gzip.GzipFile(filename="", mode="wb", fileobj=output, mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode="w", format=tarfile.PAX_FORMAT) as archive:
                for name, (data, mode) in sorted(files.items()):
                    info = tarfile.TarInfo(f"{archive_name}/{name}")
                    info.size, info.mode, info.mtime = len(data), mode, epoch
                    info.uid = info.gid = 0
                    info.uname = info.gname = ""
                    archive.addfile(info, io.BytesIO(data))
    (args.output / f"{archive_name}.tar.gz.sha256").write_text(
        f"{sha256(archive_path.read_bytes())}  {archive_path.name}\n"
    )
    print(f"Created {archive_path}")


if __name__ == "__main__":
    main()
