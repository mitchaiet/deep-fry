# Release validation — 0.1.1, 2026-09-05

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

The screenshot in this directory is rendered from the real native editor while the actual processor handles an original synthetic groove. The generated `.context/preview/demo-dry.wav` and `demo-deep-fried.wav` files are eight-second, stereo, 48 kHz, 16-bit WAVs, aligned after compensating the plugin delay. Neither file contains PCM full-scale clipping; they retain the preset's natural loudness difference.

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
