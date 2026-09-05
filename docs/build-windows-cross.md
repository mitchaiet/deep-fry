# Build Windows x64 from macOS

The usual Windows build uses Visual Studio 2022 with its C++ desktop workload;
see [the README](../README.md). This advanced recipe reproduces the alternative
macOS build route used for the Windows release. It uses Clang's MSVC frontend
and ABI with unmodified JUCE, Microsoft headers and libraries, and LLD. This
project maintains the cross-build wrappers; the recipe is not a claim that
JUCE supports every cross-host configuration.

## Requirements

- macOS with Clang 21 or later, Python 3.12 or later, CMake, and a build tool
  such as Make or Ninja. The release used Apple Clang 21.0.0 on an Apple Silicon
  Mac; the Intel Mac downloads are pinned but that host has not been tested.
- `x86_64-w64-mingw32-windres` on PATH, or its path supplied through `--windres`.
  The release used GNU Binutils 2.46.0.20260210. Only its resource writer is
  used; Microsoft SDK headers are preprocessed by Clang, and no MinGW runtime
  or C++ headers enter the build.
- Wine for running the VST3 manifest helper and tests. The release used Wine
  11.0. Set a separate workspace prefix as shown below.
- Network access for initial downloads and several GB of free space. The
  prepared SDK/CRT tree is about 630 MiB; the extracted Rust distribution is
  about 360 MiB, plus download caches and build outputs.

## Prepare and build

Run from the repository root:

```sh
python3 scripts/windows-cross/bootstrap.py
```

The bootstrap downloads xwin 0.6.5 and Rust 1.89.0 (which supplies LLD 20.1.7),
checks their SHA-256 hashes, and uses an immutable Microsoft channel manifest
with pinned CRT 14.44.35220 and SDK 10.0.26100.15 packages. The upstream xwin
utility verifies package downloads against Microsoft's manifests. It prompts
for Microsoft's license terms; `--accept-license` is available when those
terms have already been accepted. Downloaded tools, SDK headers, and development
libraries stay local. The binary packages contain linked release CRT code; they
do not redistribute the downloaded SDK or tool files.

[The pin file](../scripts/windows-cross/pins.json) records versions, immutable
manifest URL, and digests. Compiler and windres executables come from the host;
use `--clang` and `--windres` to select explicit paths. This reproduces the
build procedure and dependency versions, not byte-identical output across
compiler versions or hosts. The bootstrap stores machine-specific paths in
an ignored local configuration file. It does not install global tools.

```sh
mkdir -p "$PWD/.context"
export WINEPREFIX="$PWD/.context/wine-deep-fry-build"
export WINEDEBUG=-all
export WINEDLLOVERRIDES='mscoree,mshtml='

cmake -S . -B build-windows-cross \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/windows-clang-cl-toolchain.cmake" \
  -DDEEPFRY_WINDOWS_TOOLS_ROOT="$PWD/.context/windows-msvc-tools" \
  -DCMAKE_BUILD_TYPE=Release \
  "-DCMAKE_CROSSCOMPILING_EMULATOR=$(command -v wine)"
cmake --build build-windows-cross --config Release --parallel 4
ctest --test-dir build-windows-cross -C Release --output-on-failure
```

For a tools directory elsewhere, pass `--tools-dir` to the bootstrap and use
that same absolute path for `DEEPFRY_WINDOWS_TOOLS_ROOT`. The environment above
must remain set while building and testing. Wine initializes only the selected
prefix. JUCE is downloaded at the revision pinned in `CMakeLists.txt`; an
already verified JUCE source tree can instead be selected with
`-DFETCHCONTENT_SOURCE_DIR_JUCE=/absolute/path/to/juce`.

The VST3 bundle and standalone executable are under
`build-windows-cross/DeepFry_artefacts/Release/`. CMake embeds the Windows
manifest through LLD, so Microsoft's `mt.exe` is not needed. The release uses
static release CRT libraries (`/MT`); do not ship Debug outputs.

The two CTest suites exercise codec behavior and processor integration under
Wine. These checks do not replace testing the plugin in a native Windows DAW
with real audio hardware.

## Package the release

Packaging additionally requires the Python `pefile` package and
`x86_64-w64-mingw32-objdump` on PATH. Install `pefile` into your Python environment,
or use a local virtual environment:

```sh
python3 -m venv .context/package-venv
.context/package-venv/bin/python -m pip install pefile
```

Commit the intended source changes first. Both package scripts require a clean
Git checkout so the binary and complete source archives identify the same
commit. With the Release artifacts built from that source:

```sh
curl --fail --location \
  https://codeload.github.com/juce-framework/JUCE/tar.gz/7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2 \
  --output .context/juce-8.0.13.tar.gz
python3 scripts/package-source.py \
  --juce-archive .context/juce-8.0.13.tar.gz --output dist
.context/package-venv/bin/python scripts/package-windows-cross.py \
  --artifacts build-windows-cross/DeepFry_artefacts/Release \
  --source-archive dist/Deep-Fry-0.1.1-source.tar.gz
```

Adjust the source archive filename for a later project version. Outputs are
`dist/Deep-Fry-0.1.1-Windows-x64.zip`, the complete source archive (including
pinned JUCE), and their SHA-256 files. The Windows packager checks PE
architecture, embedded version, imports, VST3 metadata, and source commit,
then includes licenses and installation instructions. Publish the matching
complete source archive alongside the binary ZIP. Native Windows build and
PowerShell packaging commands remain in [the README](../README.md).

References: [xwin's pinned usage instructions](https://github.com/Jake-Shadle/xwin/tree/0.6.5),
[Microsoft's license directory](https://visualstudio.microsoft.com/license-terms/),
and [third-party notices](../THIRD_PARTY_NOTICES.md).
