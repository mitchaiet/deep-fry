# Changelog

## 0.1.1 — 2026-09-05

First public release of Deep Fry.

- Release the project under AGPLv3, with full license and dependency notices.
- Provide universal macOS VST3, Audio Unit, and standalone downloads, plus a
  complete corresponding-source archive with pinned JUCE and offline build scripts.
- Provide Windows x64 VST3 and standalone downloads with a static compiler runtime.
- Set the macOS deployment target explicitly to 11.0. The previous preview
  inherited 26.2 from the local build machine. Runtime validation remains on 26.2.
- Include installation instructions, update backups, checksums, and build metadata.
- Sign the VST3 after manifest generation so all final bundle resources are sealed.
- Add license and source links to the editor's help panel.
- Add automated Linux codec, universal macOS, and Windows x64 build checks.
- Document the controls, signal path, and Ableton Live setup in the public README.

The audio algorithm, parameter IDs, presets, and saved-state format are unchanged
from 0.1.0. Mac bundles are ad-hoc signed and are not Apple-notarized;
Windows binaries are unsigned.

## 0.1.0 — 2026-09-05

Initial personal preview: 8×8 audio-image tiles, JPEG DCT quantization,
contrast/sharpening, pixel-depth reduction, latency-aligned dry/wet processing,
four presets, and the native Impact-based live image display.

The preview binaries require macOS 26.2. Its release has been supplemented with
the AGPLv3 license and complete corresponding source for archival downloads.
