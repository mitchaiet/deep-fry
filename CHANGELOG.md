# Changelog

## 0.2.0 — 2026-09-05

Make the live visualizer useful for comparing, inspecting, and saving the sound's image.

- Default to **Final Out**, showing captured audio after Mix, Output gain, and
  Bypass, aligned with its original input. Keep **JPEG Wet** for the decoded
  compression stage before those controls.
- Capture both stereo channels and add **L / R** inspection, with mono handling.
- Apply one selectable **Colour / Gray** palette to both images and add a
  signed-amplitude legend.
- Click either mosaic to freeze and inspect a matched 8×8 tile. Show its position,
  age relative to the latest capture, final output peak, and DCT detail.
- Preserve output samples above full scale in the capture; limit only the picture
  and highlight tile peaks above 0 dBFS.
- Export a stable, paired **1080×352 PNG** while audio and the display continue.
- Allow view, channel, and palette changes on frozen history. Reset live history
  when processing restarts or the audio configuration changes, including after a freeze.
- Regenerate Windows version resources when rebuilding after a version change.
- Document the new visual controls and provide updated macOS universal and
  Windows x64 preview packages with complete corresponding source.

The audio algorithm, 64-sample latency, sound parameter IDs, presets, and saved-state
format are unchanged. Visual preferences and captured history belong to the open
editor and are not saved with the DAW session. Mac bundles remain ad-hoc signed
and not Apple-notarized; Windows binaries remain unsigned. See
[validation results](docs/validation.md) for checks and platform limitations.

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
