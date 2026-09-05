#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Package an existing Windows cross build; requires pefile and PE binutils."""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import tarfile
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BINARIES = ("VST3/Deep Fry.vst3/Contents/x86_64-win/Deep Fry.vst3", "Standalone/Deep Fry.exe")


def run(*command):
    return subprocess.check_output(command, text=True).strip()


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inspect_binary(path, version):
    import pefile

    pe = pefile.PE(str(path))
    try:
        if pe.FILE_HEADER.Machine != 0x8664 or pe.OPTIONAL_HEADER.Magic != 0x20B:
            raise ValueError(f"Expected AMD64 PE32+ binary: {path}")
        info = pe.VS_FIXEDFILEINFO[0]
        binary_version = ".".join(str(n) for n in (info.ProductVersionMS >> 16,
            info.ProductVersionMS & 0xffff, info.ProductVersionLS >> 16, info.ProductVersionLS & 0xffff))
        if binary_version not in (version, version + ".0"):
            raise ValueError(f"Product version {binary_version} does not match {version}: {path}")
        if pe.OPTIONAL_HEADER.DATA_DIRECTORY[4].VirtualAddress or pe.OPTIONAL_HEADER.DATA_DIRECTORY[4].Size:
            raise ValueError(f"Expected an unsigned binary; update signing metadata: {path}")
        imports = sorted({entry.dll.decode("ascii").lower() for entry in pe.DIRECTORY_ENTRY_IMPORT})
        if hasattr(pe, "DIRECTORY_ENTRY_DELAY_IMPORT"):
            imports = sorted(set(imports) | {entry.dll.decode("ascii").lower() for entry in pe.DIRECTORY_ENTRY_DELAY_IMPORT})
    finally:
        pe.close()
    dump = run("x86_64-w64-mingw32-objdump", "-p", str(path))
    dumped_imports = {name.lower() for name in re.findall(r"DLL Name:\s*(\S+)", dump)}
    if not imports or set(imports) != dumped_imports:
        raise ValueError(f"PE and objdump DLL inspection disagree: {path}")
    runtime_dlls = [name for name in imports if re.match(
        r"^(libstdc\+\+|libgcc|libwinpthread|winpthread|libssp|msvcp\d|msvcr\d|vcruntime\d|concrt\d|ucrtbased)", name)]
    if runtime_dlls:
        raise ValueError(f"Expected static compiler runtime; external DLLs: {runtime_dlls}")
    return {"architecture": "x64", "pe_format": "PE32+", "version": binary_version,
            "authenticode": "unsigned", "imported_dlls": imports}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifacts", type=Path, default=ROOT / "build-windows-msvc/DeepFry_artefacts/Release")
    parser.add_argument("--output", type=Path, default=ROOT / "dist")
    parser.add_argument("--source-archive", type=Path)
    args = parser.parse_args()
    if run("git", "-C", str(ROOT), "status", "--porcelain"):
        raise ValueError("Commit all source changes before packaging a release.")
    commit = run("git", "-C", str(ROOT), "rev-parse", "HEAD")
    if not re.fullmatch(r"[a-f0-9]{40}", commit):
        raise ValueError("Cannot read a complete Git commit ID.")
    cmake = (ROOT / "CMakeLists.txt").read_text()
    version = re.search(r"project\(DeepFry VERSION (\d+\.\d+\.\d+) ", cmake)[1]
    juce_commit = re.search(r"codeload.github.com/juce-framework/JUCE/tar.gz/([a-f0-9]{40})", cmake)[1]
    juce_sha = re.search(r"URL_HASH SHA256=([a-f0-9]{64})", cmake)[1]
    artifacts = args.artifacts.resolve()
    compiler_files = list((artifacts.parents[1] / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake"))
    if len(compiler_files) != 1:
        raise ValueError("Expected one CMake compiler record for the supplied build directory.")
    compiler_data = compiler_files[0].read_text()
    compiler_id = re.search(r'set\(CMAKE_CXX_COMPILER_ID "([^"]+)"\)', compiler_data)[1]
    if compiler_id in ("Clang", "AppleClang") and 'set(CMAKE_CXX_SIMULATE_ID "MSVC")' in compiler_data:
        compiler_family = "Clang (MSVC ABI)"
    else:
        raise ValueError("Expected a Windows Clang/MSVC-ABI cross build.")
    compiler_version = re.search(r'set\(CMAKE_CXX_COMPILER_VERSION "([^"]+)"\)', compiler_data)[1]
    binary_info = {relative: inspect_binary(artifacts / relative, version) for relative in BINARIES}
    module_info = json.loads((artifacts / "VST3/Deep Fry.vst3/Contents/Resources/moduleinfo.json").read_text())
    if module_info.get("Version") != version:
        raise ValueError("VST3 moduleinfo.json version does not match the project.")
    source_name = f"Deep-Fry-{version}-source.tar.gz"
    source = {"file": source_name, "source_commit": commit,
        "release_url": f"https://github.com/mitchaiet/deep-fry/releases/tag/v{version}",
        "download_url": f"https://github.com/mitchaiet/deep-fry/releases/download/v{version}/{source_name}"}
    if args.source_archive:
        if args.source_archive.name != source_name:
            raise ValueError(f"Expected corresponding source archive named {source_name}.")
        with tarfile.open(args.source_archive, "r:gz") as archive:
            manifest = json.load(archive.extractfile(f"Deep-Fry-{version}-source/SOURCE-MANIFEST.json"))
        if manifest.get("source_commit") != commit or manifest.get("source_tree_dirty") is not False or manifest.get("version") != version:
            raise ValueError("Corresponding source does not match the current clean Git commit/version.")
        source["sha256"] = sha256(args.source_archive)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    package_name = f"Deep-Fry-{version}-Windows-x64"
    with tempfile.TemporaryDirectory(prefix=".deep-fry-cross-package-", dir=output) as temporary:
        stage = Path(temporary)
        package = stage / package_name
        (package / "Standalone").mkdir(parents=True)
        shutil.copytree(artifacts / "VST3/Deep Fry.vst3", package / "VST3/Deep Fry.vst3")
        shutil.copy2(artifacts / BINARIES[1], package / BINARIES[1])
        shutil.copytree(ROOT / "packaging/LICENSES", package / "LICENSES")
        if not any((package / "LICENSES").iterdir()):
            raise ValueError("Missing dependency license notices.")
        for name in ("LICENSE", "CHANGELOG.md", "COPYRIGHT", "THIRD_PARTY_NOTICES.md"):
            (package / name).write_text((ROOT / name).read_text().replace("packaging/LICENSES/", "LICENSES/"))
        (package / "README.txt").write_text((ROOT / "packaging/README-Windows.txt").read_text().replace("@VERSION@", version))
        for path in package.rglob("*"):
            if path.is_file() and (path.suffix.lower() in (".pdb", ".ilk", ".exp", ".lib") or path.name == ".DS_Store"):
                path.unlink()
        manifest = {"project": "Deep Fry", "version": version, "license": "AGPL-3.0-only",
            "source_repository": "https://github.com/mitchaiet/deep-fry", "source_commit": commit,
            "source_tree_dirty": False, "platform": "Windows", "architectures": ["x64"],
            "minimum_windows": "10 version 1607", "code_signed": False, "authenticode": "unsigned",
            "compiler": {"family": compiler_family, "version": compiler_version},
            "compiler_runtime": "static", "corresponding_source": source,
            "juce": {"commit": juce_commit, "archive_sha256": juce_sha}, "binaries": binary_info,
            "files_sha256": {p.relative_to(package).as_posix(): sha256(p) for p in sorted(package.rglob("*")) if p.is_file()}}
        (package / "RELEASE-MANIFEST.json").write_text(json.dumps(manifest, indent=2) + "\n")
        zip_path = stage / f"{package_name}.zip"
        with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for path in sorted(package.rglob("*")):
                if path.is_file():
                    archive.write(path, path.relative_to(stage).as_posix())
        with zipfile.ZipFile(zip_path) as archive:
            if archive.testzip() is not None:
                raise ValueError("ZIP integrity verification failed.")
        checksum = stage / f"{zip_path.name}.sha256"
        checksum.write_text(f"{sha256(zip_path)}  {zip_path.name}\n")
        zip_path.replace(output / zip_path.name)
        checksum.replace(output / checksum.name)
    print(f"Created {output / (package_name + '.zip')}")


if __name__ == "__main__":
    main()
