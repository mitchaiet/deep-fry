# Deep Fry 0.3.1 validation

This release restores Impact buttons and headings, thick borders, offset
shadows, chunky sliders, and hover/pressed feedback. Stereo visualization,
JPEG export, and readable Plex captions remain available. The audio algorithm, automatable parameter IDs,
presets, state format, and 64-sample latency are unchanged.

## Checks

| Check | Result |
| --- | --- |
| macOS codec CTest | 99,811 checks passed |
| macOS processor/export integration CTest | 600,060 checks passed |
| Native editor and artifact run | 1,384,754 checks passed |
| Stereo/solo sample mapping, right tile inspection, mono fallback | Passed |
| Genuine JPEG encode/decode, dimensions, bounded error, write preservation | Passed |
| View/Palette async selection and unchanged audio/state | Passed |
| Mac VST3 pluginval 1.0.4, strictness 5 | Final separate runs passed on arm64 and x86_64 under Rosetta |
| Windows CTest under Wine 11 | Both suites passed; 99,811 codec and 600,060 integration checks |
| Windows pluginval 1.0.4, strictness 5, GUI skipped | Passed under Wine 11 |
| Mac and Windows version/font embedding checks | 0.3.1 and all three complete TTF payloads verified |

Native editor PNGs and the actual JPEG export were inspected, including
stereo, L/R solo, selected right tile, idle, help, effect OFF, open menus, and
896×640 minimum-size layouts, button hover, pressed, and selected-OFF hover states. The README screenshot shows this release.

Testing ran on macOS 26.2 with the pinned, unmodified JUCE 8.0.13. The VST3, AU,
and standalone Mac bundles contain arm64 and x86_64 slices, target macOS 11+,
and pass strict ad-hoc signature verification. Windows VST3 and standalone are
PE32+ AMD64 binaries with version 0.3.1.0, static compiler runtime and system-DLL
imports. The VST3 manifest reports 0.3.1 with unchanged plugin class identifiers.

Local evidence is in `.context/preview-031-final/`, `.context/artifacts-031-final.log`,
`.context/ctest-031-macos.log`, `.context/pluginval-031-*.log`, and
`.context/windows-cross/*031*`. These private artifacts are excluded
from source and release packages.

## What is exercised

The JPEG checks inspect file markers, decode the result, verify dimensions and
bounded color error, and confirm that rejected writes preserve existing files.
They cover `.jpg` / `.jpeg` spelling, suffix normalization, exact approved paths,
invalid images, unsupported dimensions, missing parents, directory collisions,
and temporary-file cleanup. Snapshot ownership is checked while processing
continues and view settings change.

The native stereo regression processes 80 distinct captures with different left
and right amplitudes. It compares every displayed stereo tile sample against
its matching solo history, including the 16 captures hidden by the stereo
view's 64-tile limit. Clicking a historical right-lane tile must show that tile's
right input and output in the inspector. Mono uses the full image, disables R,
and preserves the Stereo preference when stereo processing returns.

Existing checks cover exact source/output alignment, dry/wet/bypass latency,
stereo isolation, automation, saved state, FIFO publication, frozen history
across stream restarts, image controls preserving callback audio, and menu
selection surviving an intervening display timer refresh.

## Reproduction

From a configured macOS build:

```sh
cmake --build build-public --config Release --parallel 4
ctest --test-dir build-public -C Release --output-on-failure
build-public/DeepFryVerify_artefacts/Release/DeepFryVerify --artifacts .context/preview-031
pluginval --strictness-level 5 --validate "build-public/DeepFry_artefacts/Release/VST3/Deep Fry.vst3"
```

Repeat pluginval with `arch -arm64` and `arch -x86_64` on an Apple Silicon Mac
with Rosetta installed. See [Windows cross-build instructions](build-windows-cross.md)
for the isolated Wine prefix and Windows build configuration.

## Limits

The first two Mac pluginval runs were started concurrently. Both completed the
test suite and then reported a segmentation fault during shutdown (exit 9).
The same binary subsequently exited 0 in a normal arm64 run and under LLDB.
The final build passed separate arm64 and x86_64 validations using those initial
runs' exact random seeds, exiting 0. No deterministic cause or font-lifetime
fault was identified, and no font workaround was added. The initial shutdown
failures remain unexplained; they are not counted as successful validation.


Native Save/Replace dialogs are not automated by the verification executable;
the tested encoder and file-writing helper are the same ones used by the editor.
Dialog callbacks use editor-safe pointers, and the overwrite confirmation is
owned by the editor so it closes when the editor is destroyed.

Windows remains a preview: Wine processing checks do not establish native
Windows GUI, audio-hardware, or DAW compatibility. GUI tests are skipped under
Wine because the previous full run timed out and its native rendering attempt
failed in Wine's DirectWrite implementation. Bundled font rendering on a real
Windows installation remains unverified.

This version has not been independently tested in Ableton/Logic, through
physical audio hardware, in an Audio Unit host, or on older supported macOS
versions. Mac downloads use ad-hoc signatures and are not Apple-notarized;
Windows downloads are unsigned. Historical checks are in [validation.md](validation.md).
