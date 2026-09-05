#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Prepare local Clang/MSVC-ABI Windows build tools on macOS."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import urllib.request

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[1]
PINS = json.loads((HERE / 'pins.json').read_text())


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def download(url, target, expected):
    if target.exists():
        if digest(target) != expected:
            raise RuntimeError(f'Cached download has the wrong SHA-256: {target}')
        return
    target.parent.mkdir(parents=True, exist_ok=True)
    partial = target.with_name(target.name + '.partial')
    print(f'Downloading {target.name}', flush=True)
    try:
        with urllib.request.urlopen(url, timeout=120) as response, partial.open('wb') as output:
            shutil.copyfileobj(response, output)
        if digest(partial) != expected:
            raise RuntimeError(f'Download failed SHA-256 verification: {target.name}')
        partial.replace(target)
    finally:
        partial.unlink(missing_ok=True)


def extract(archive, root, expected_executable):
    if expected_executable.is_file():
        return
    with tarfile.open(archive) as source:
        source.extractall(root, filter='data')
    if not expected_executable.is_file():
        raise RuntimeError(f'Archive layout is missing {expected_executable}')


def install_wrappers(root, clang, windres, triple):
    destination = root / 'bin'
    destination.mkdir(parents=True, exist_ok=True)
    for source in (HERE / 'bin').iterdir():
        if source.is_file():
            shutil.copyfile(source, destination / source.name)
            (destination / source.name).chmod(0o755)
    config = {
        'clang': str(clang),
        'windres': str(windres),
        'lld_relative_path': f'rustc-{PINS["rust_version"]}-{triple}/rustc/lib/rustlib/{triple}/bin/rust-lld',
    }
    (root / 'wrapper-config.json').write_text(json.dumps(config, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tools-dir', type=Path, default=PROJECT / '.context/windows-msvc-tools')
    parser.add_argument('--clang', default=os.environ.get('DEEPFRY_CLANG', 'clang'))
    parser.add_argument('--windres', default=os.environ.get('DEEPFRY_WINDRES', 'x86_64-w64-mingw32-windres'))
    parser.add_argument('--accept-license', action='store_true', help='Accept Microsoft SDK/CRT license terms for xwin')
    args = parser.parse_args()
    if sys.version_info < (3, 12):
        parser.error('Python 3.12 or later is required')
    if platform.system() != 'Darwin' or platform.machine() not in PINS['hosts']:
        parser.error('This bootstrap supports macOS arm64 and x86_64 hosts')
    clang = shutil.which(args.clang)
    windres = shutil.which(args.windres)
    if not clang or not windres:
        parser.error('Install Clang and x86_64-w64-mingw32-windres, or provide --clang and --windres')
    version = subprocess.check_output([clang, '--version'], text=True)
    match = re.search(r'clang version (\d+)', version)
    if match is None or int(match.group(1)) < 21:
        parser.error('This recipe requires Clang 21 or later; the release used Apple Clang 21.0.0')
    root = args.tools_dir.expanduser().resolve()
    root.mkdir(parents=True, exist_ok=True)
    host = PINS['hosts'][platform.machine()]
    triple = host['triple']
    xwin_name = f'xwin-{PINS["xwin_version"]}-{triple}'
    rust_name = f'rustc-{PINS["rust_version"]}-{triple}'
    downloads = root / 'downloads'
    xwin_archive = downloads / f'{xwin_name}.tar.gz'
    rust_archive = downloads / f'{rust_name}.tar.xz'
    download(f'https://github.com/Jake-Shadle/xwin/releases/download/{PINS["xwin_version"]}/{xwin_archive.name}', xwin_archive, host['xwin_sha256'])
    download(f'https://static.rust-lang.org/dist/{rust_archive.name}', rust_archive, host['rust_sha256'])
    xwin = root / xwin_name / 'xwin'
    lld = root / rust_name / 'rustc/lib/rustlib' / triple / 'bin/rust-lld'
    extract(xwin_archive, root, xwin)
    extract(rust_archive, root, lld)
    manifest = downloads / 'VisualStudio.17.Release.chman'
    microsoft = PINS['microsoft']
    download(microsoft['channel_manifest_url'], manifest, microsoft['channel_manifest_sha256'])
    command = [str(xwin), '--arch', 'x86_64', '--variant', 'desktop', '--manifest', str(manifest),
               '--crt-version', microsoft['crt_xwin_selector'], '--sdk-version', microsoft['sdk_xwin_selector'],
               '--cache-dir', str(root / 'xwin-cache')]
    if args.accept_license:
        command.append('--accept-license')
    command.extend(['splat', '--output', str(root / 'sysroot')])
    subprocess.run(command, check=True)
    for relative in ('crt/include/vector', 'crt/lib/x86_64/libcmt.lib', 'sdk/include/ucrt/stdio.h',
                     'sdk/lib/ucrt/x86_64/libucrt.lib', 'sdk/lib/um/x86_64/kernel32.lib'):
        if not (root / 'sysroot' / relative).is_file():
            raise RuntimeError(f'Sysroot is incomplete: {relative}')
    install_wrappers(root, Path(clang).resolve(), Path(windres).resolve(), triple)
    (root / 'bootstrap-pins.json').write_text(json.dumps(PINS, indent=2) + '\n')
    print(f'Windows build tools ready: {root}')
    print('Set CMake DEEPFRY_WINDOWS_TOOLS_ROOT to that directory.')


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error)) from error
