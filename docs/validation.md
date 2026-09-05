# Release validation

## 0.2.0 — 2026-09-05

This release adds paired input/final-output capture, stereo inspection, a shared
palette, tile selection, and PNG export. The audio algorithm, parameter IDs,
presets, state format, and 64-sample latency are unchanged.

| Check | Result |
| --- | --- |
| macOS CTest codec and plugin integration suites | Passed |
| Exact captured output versus processed stereo audio | Passed for Mix, gain, parameter/host bypass, smoothing, automation, and irregular buffers |
| Capture timing, mono, invalid input, full FIFO, and prepare resets | Passed |
| Native editor at 1120×800 and 896×640 | Passed |
| Wet/final view, Colour/Gray, L/R, mono fallback, freeze and tile inspection | Passed |
| Stable paired 1080×352 snapshot and PNG encode/decode | Passed |
| Frozen history across a processing restart that overtakes its old timeline | Passed |
| pluginval 1.0.4, strictness 5, Apple Silicon VST3 | Passed |
| pluginval 1.0.4, strictness 5, Intel VST3 under Rosetta | Passed |
| Windows x64 CTest codec and plugin integration suites under Wine 11 | Passed |
| Windows pluginval 1.0.4, strictness 5, GUI tests skipped | Passed under Wine 11 |
| Windows incremental version-resource regeneration | Passed for both plugin and verification targets |

The final native artifact run passed **1,368,206 integration checks**. The editor
tests verify that visualization controls leave audio and saved sound parameters
unchanged. They also check that exported pixels remain stable
while new audio arrives and the view changes. Snapshot encoding is tested without
opening the operating system's Save dialog; that dialog has not been automated.
The stream-restart regression holds negative-amplitude history, prepares a new
positive-amplitude stream, advances beyond the old sample positions while frozen,
and compares the resumed image with a fresh editor receiving only new captures.

Nine native images were inspected: idle, live, compact, grayscale, JPEG wet,
right channel, selected tile, help, and paired PNG export. The current
[README screenshot](deep-fry-ui.png) comes from this run. Local screenshots and
synthetic before/after audio are in `.context/visualizer-020-preview/`.

The Mac VST3, Audio Unit, and standalone bundles contain arm64 and x86_64 slices,
target macOS 11.0+, and use ad-hoc signatures. They are not Apple-notarized.
Testing ran on macOS 26.2 using Xcode 26.6 and the unmodified pinned JUCE 8.0.13.
The packager checks bundle versions, architecture slices, minimum OS versions,
and final signatures before creating the download.

The Windows VST3 and standalone are unsigned PE32+ AMD64 binaries with version
0.2.0.0, a static compiler runtime, and only Windows system DLL imports. The VST3
includes valid `moduleinfo.json` metadata for 0.2.0. They use the same Clang/MSVC
cross toolchain described below and in [Windows cross builds](build-windows-cross.md).

**Windows remains a preview.** The 0.2.0 Windows checks ran under Wine and skipped
GUI tests. The earlier full GUI attempt timed out in pluginval; native editor
rendering also failed inside Wine's DirectWrite implementation. Native Windows
GUI and DAW compatibility remain unverified. Hosted GitHub Actions previously
could not start because of an account billing lock; no hosted validation is
claimed for this release.

The existing installer and source-packaging behavior was validated for 0.1.1 as
described below. This release does not change the installer. Release manifests
record the exact source commit and file hashes, and both binary packages link to
the same complete-source archive. No verification installs into the user's
plugin folders or closes an open DAW.

Not independently verified for 0.2.0: physical audio hardware, a live Ableton or
Logic session, Audio Unit host validation, native Windows playback/GUI, older
macOS versions, Linux plugin builds, or Apple's notarization workflow.

## Previous release: 0.1.1 — 2026-09-05

The macOS Release build contains universal `arm64` and `x86_64` binaries for VST3, Audio Unit, and standalone. The explicit deployment target is **macOS 11.0** for both architectures; runtime checks were performed on **macOS 26.2**. All three bundles pass `codesign --verify --deep --strict` with ad-hoc signatures. VST3 signing runs after its `moduleinfo.json` resource is generated.

| Check | Result |
| --- | --- |
| CTest `jpeg_codec` | Passed |
| CTest `plugin_integration` | Passed |
| Codec AddressSanitizer + UndefinedBehaviorSanitizer | Passed |
| Native editor creation, default/compact size, idle/live/help screenshots | Passed |
| Preset buttons, slider/host synchronization, bypass, display freeze | Passed |
| pluginval 1.0.4, strictness 5, Apple Silicon VST3 | Passed, exit 0 |
| pluginval 1.0.4, strictness 5, Intel VST3 under Rosetta | Passed, exit 0 |
| Release ZIP integrity, checksum, universal slices, signatures | Passed |
| Installer fresh install, update, and byte-for-byte backups in an isolated prefix | Passed |
| Installer rollback on replacement failure, interruption, and failed fresh install | Passed |
| Installer retains recovery copies when rollback itself is blocked | Passed |
| Complete source archive hashes, pinned JUCE, and offline configuration | Passed |

Host validation exercised opening/closing the editor while processing, programs, state restoration, automation, and bus layouts. Audio and automation tests ran at 44.1, 48, and 96 kHz with buffers of 64, 128, 256, 512, and 1024 samples. Additional integration tests covered 0-, 1-, and 17-sample buffers, exact dry/bypass timing, 64-sample latency, stereo isolation, malformed state, and visualization/audio consistency.

The 0.1.1 screenshots were rendered from the real native editor while the actual processor handled an original synthetic groove; the screenshot currently in this directory shows 0.2.0. The generated `.context/preview/demo-dry.wav` and `demo-deep-fried.wav` files are eight-second, stereo, 48 kHz, 16-bit WAVs, aligned after compensating the plugin delay. Neither file contains PCM full-scale clipping; they retain the preset's natural loudness difference.

The Impact interface has visual checks at 1120×800 and 896×640, including idle and help states. Both sizes were inspected for clipping, font rendering, and readable control values. The public release adds copyright, license, warranty, and source information inside the help panel. Its screenshots and regenerated audio are in `.context/public-preview/`. The processor and saved parameter IDs are unchanged. The release VST3 passed pluginval strictness 5 on both architectures; logs are `.context/pluginval-public-arm64.log` and `.context/pluginval-public-x86_64.log`. The native verification run passed 1,244,111 checks.

The packaged ZIP was extracted and installed twice with `Install.command --non-interactive --prefix` into a temporary directory. Installed bundles and update backups matched every source file's hash and mode, passed strict signature verification, and contained both architectures. Five additional failure tests verified rollback after the first replacement and interruptions after old/new bundle renames, retention of recoverable copies when restoration is blocked, and cleanup after a failed fresh installation. These checks did not alter the user's plugin folders.

The public packager was also tested with version 0.1.1: fresh installation,
update backups, forced failure after the second old-bundle rename, and symlink
refusal all passed in isolated directories. The complete-source archive was
verified file by file; all 4,424 JUCE files match the pinned upstream archive.
An extracted source archive configured the plugin without downloading JUCE and
built and tested the codec offline.

The Windows x64 package was built on macOS with Apple Clang 21.0.0 targeting
the MSVC ABI, LLD 20.1.7, Microsoft's CRT 14.44.35220 and SDK 10.0.26100.15,
and the unmodified pinned JUCE source. Both PE32+ binaries carry version
0.1.1.0 and import only Windows system DLLs; the compiler runtime is static.
The VST3 bundle includes its generated `moduleinfo.json`. Build wrappers and
pinned tool downloads are documented in [Windows cross builds](build-windows-cross.md).

Both Windows CTest suites passed under Wine 11.0 on macOS. Windows pluginval
1.0.4 also passed strictness 5 with `--skip-gui-tests`, covering processing,
automation, state, programs, and bus layouts. Its full GUI run timed out during
the Editor test after 30 seconds; that run is not counted as a pass. A separate
editor-rendering run crashed inside Wine's `dwrite` implementation (DirectWrite).
The Windows download is a preview with native Windows GUI/DAW validation still
pending. These are Wine checks, not native Windows validation.

The native Windows GitHub Actions workflow builds the VST3, standalone, and
verification targets, runs both CTest suites, checks the package, and retains
the ZIP and checksum. The initial hosted runs were blocked before execution
because GitHub reported an account billing lock; no hosted Windows test success
is claimed. Local Windows build and validation details are recorded in the
release notes. Windows standalone audio uses WASAPI/DirectSound; the VST3 uses
the host's audio driver. The workflow also defines Linux codec and universal
macOS jobs. See
[the build workflow](../.github/workflows/build.yml) and
[run history](https://github.com/mitchaiet/deep-fry/actions/workflows/build.yml).

Reproduce the checks from the repository root:

```sh
./scripts/build.sh
build/DeepFryVerify_artefacts/Release/DeepFryVerify --artifacts .context/preview
pluginval --strictness-level 5 --validate "build/DeepFry_artefacts/Release/VST3/Deep Fry.vst3"
```

The macOS build used Xcode 26.6, CMake 3.28.2, SDK 26.5, and a macOS 11.0 deployment target. The initial 0.1.0 preview inherited a macOS 26.2 target from the toolchain and remains available as an archived preview. `.context/` contains local verification artifacts and is intentionally excluded from the repository and release.

Not independently verified: playback through physical audio hardware, an
Ableton/Logic session, Audio Unit host validation, a Windows DAW session, Linux
plugin builds, older macOS releases, or notarized distribution. Steinberg's
separate SDK validator was not run. The checks above do not guarantee
compatibility with every DAW or audio interface.
