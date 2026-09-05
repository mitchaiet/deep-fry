# Deep Fry

A JPEG compression audio effect. Deep Fry turns short blocks of audio into grayscale images, applies JPEG quantization, contrast, sharpening, and pixel reduction, then converts the result back into audio. Its live display pairs the input with either the processed JPEG samples or the final audio output.

![Deep Fry processing audio: input and final-output mosaics, tile inspectors, and effect controls](docs/deep-fry-ui.png)

**[Download the latest release](https://github.com/mitchaiet/deep-fry/releases/latest)** · [v0.2.0 release notes](https://github.com/mitchaiet/deep-fry/releases/tag/v0.2.0) · [Report an issue](https://github.com/mitchaiet/deep-fry/issues)

## Install

| Download | Includes | Requirements |
| --- | --- | --- |
| [macOS universal ZIP](https://github.com/mitchaiet/deep-fry/releases/download/v0.2.0/Deep-Fry-0.2.0-macOS-universal.zip) | VST3, Audio Unit, standalone app | macOS 11+, Apple Silicon or Intel |
| [Windows x64 ZIP (preview)](https://github.com/mitchaiet/deep-fry/releases/download/v0.2.0/Deep-Fry-0.2.0-Windows-x64.zip) | VST3, standalone EXE | Windows 10 version 1607+, 64-bit Intel or AMD |

**macOS:** Extract the complete ZIP, save your session and close your DAW, then open `Install.command`. It installs for your user account and backs up existing versions. The binaries are ad-hoc signed and **not Apple-notarized**, so macOS may require explicit approval. [Mac installation, security prompts, and removal](docs/installation.md)

**Windows:** Extract the complete ZIP, close your DAW, and copy the entire `VST3\Deep Fry.vst3` bundle into `C:\Program Files\Common Files\VST3`. Administrator permission may be needed. The standalone EXE runs from its extracted folder. [Windows installation and removal](docs/installation-windows.md)

Reopen your DAW and rescan plug-ins after installing. The Mac build targets macOS 11+. The Windows download is a preview; native Windows DAW and GUI compatibility remain unverified. See [validation results](docs/validation.md) for version-specific checks and tested configurations. Linux binaries are not currently provided.

### Ableton Live 11

In **Preferences → Plug-Ins**, turn on **Use VST3 Plug-In System Folders** and click **Rescan**. Find **Deep Fry** under **Plug-Ins** in the browser, then drag it onto an audio track or after an instrument. See Ableton's setup guides for [macOS](https://help.ableton.com/hc/en-us/articles/209068929-Using-AU-and-VST-plug-ins-on-macOS) and [Windows](https://help.ableton.com/hc/en-us/articles/209071729-Using-VST-plug-ins-on-Windows).

Choose **Meme**, play a loop, and lower **JPEG Quality** to introduce more artifacts. Increase **Fry** for contrast and sharpening distortion. For a before/after comparison, toggle **Bypass** and adjust **Output** to match the perceived volume of the original. **Deep fried** and **Lost cause** are more extreme starting points.

In the standalone app, open the audio settings to select your interface and input. Use headphones when processing a live microphone.

## Controls

| Control | Effect |
| --- | --- |
| **JPEG Quality** | 1–100. Lower values discard more image detail, creating block distortion and ringing. Even 100 passes through the pixel conversion. |
| **Fry** | 0–100%. Adds image contrast and sharpening before compression. |
| **Pixel Depth** | 2–8 bits. Reduces decoded pixel levels for coarse, stepped distortion. |
| **Mix** | Blends processed and latency-aligned original audio. |
| **Output** | −24 to +6 dB, after the mix. |
| **Bypass** | Returns the latency-aligned original at its original level. |
| **JPEG Wet / Final Out** | Chooses the processed JPEG samples or the final audible output for the large image. Final Out is the default. |
| **Colour / Gray** | Applies the same amplitude palette to both images. Colour is the default. |
| **L / R** | Chooses the displayed audio channel. R is disabled for captured mono audio. |
| **Freeze Image / Resume Live** | Holds or resumes the image history; audio keeps processing. Clicking a tile also freezes it for inspection. |
| **Save PNG** | Exports a paired input/result image with the selected channel, view, and palette. |

Four presets—**Clean-ish**, **Meme**, **Deep fried**, and **Lost cause**—set the five sound controls. Sound parameters support host automation and are saved with the DAW session. Mono and stereo are supported; stereo channels are processed independently.

## How it works

```text
64 audio samples → 8×8 grayscale image → contrast / sharpening
  → 2D DCT → JPEG quantization → inverse DCT
  → pixel reduction → 64 audio samples → mix / output
```

Each tile uses JPEG's standard luminance quantization table and IJG quality scaling. The processor runs the lossy image operations in memory. It does not write `.jpg` files: the file container and entropy coding do not change decoded pixels. This is an audio-oriented implementation, rather than a byte-identical libjpeg round trip.

Samples map symmetrically around gray 128, with an exact zero level at every pixel depth. Digital silence stays silent. The processed path clips input beyond ±1 during image conversion; dry and bypass preserve finite input. Tile boundaries are intentionally audible, and extreme contrast or sharpening can alias.

Latency is **64 samples**: 1.45 ms at 44.1 kHz, or 1.33 ms at 48 kHz. The host receives this latency, and dry, wet, and bypass use the same delay. The audio callback uses fixed storage without file I/O, allocation, or locks. Closing or freezing the editor does not interrupt processing.

## What the visualizer shows

The visual output is a **rolling mosaic of the audio waveform packed into pixels**. Both panels show matching sampled blocks from the selected **left, right, or mono channel**. Each pixel represents one sample's signed amplitude: its position above or below the waveform's zero line. The picture has no frequency axis or musical-note color coding.

### From samples to pixels

Every 64 consecutive audio samples form an **8×8 tile**. Samples 1–8 fill the first row from left to right, samples 9–16 fill the second, and so on. At 48 kHz, one tile contains about **1.33 ms** of audio. The codec processes each tile independently; the editor assembles selected tiles into the larger picture.

**01 / INPUT** shows the input tile after clipping to ±1 and rounding into pixel values. The mapping is `128 + round(clamp(sample, -1, 1) × 127)`. A repeating waveform can form bands or stripes as it wraps between rows; irregular sample values produce a more irregular texture.

The large panel has two views:

- **FINAL OUT**, the default, shows the actual samples sent back to the host **after Mix, Output gain, and Bypass**, including their smoothing. The processor pairs them with the input tile that produced them, accounting for its 64-sample latency. At 0% Mix, Output still changes the dry signal's level; Bypass returns the delayed original at its original level.
- **JPEG WET** shows the decoded samples after Fry's contrast/sharpening, JPEG quantization and reconstruction, and Pixel Depth reduction, before Mix, Output, and Bypass. This view can remain heavily distorted while the audible output is dry.

The final-output capture preserves the actual floating-point samples, including values beyond ±1. The picture limits them to ±1 for display, using `128 + clamp(sample, -1, 1) × 127`. The selected tile's output peak can reveal levels above that display range. INPUT has already been rounded to integer pixels, so its image is an 8-bit representation of the original waveform.

### One palette for both panels

**COLOUR / GRAY** switches both panels together so their amplitudes can be compared using the same scale. Colour is the default; the audio codec itself always operates on grayscale values.

| Sample amplitude | Pixel value | Gray appearance | Colour appearance |
| --- | --- | --- | --- |
| −1, negative full scale | 1 | Almost black | Near-black/purple |
| 0, the waveform's zero level | 128 | Mid-gray | Red-orange |
| +1, positive full scale | 255 | White | Pale cream |

Between those points, negative values run through purple and magenta; positive values run through orange and yellow. The red-orange center is the palette's zero point, not a clipping warning. The amplitude legend below the inspectors marks −1, 0, and +1. Changing the palette changes only the display.

Use **L / R** to inspect either stereo channel. Mono audio uses L and disables R. Both channels are captured together, so changing the channel or view also works on frozen history.

### How to read and inspect the mosaic

Each panel contains up to **128 tiles**, arranged **16 across by 8 down**, making a **128×64-pixel image** enlarged in the UI. Tiles fill from top-left to bottom-right. Once the history is full, the oldest tile appears at top-left and the newest at bottom-right.

The processor sends roughly 60 selected tiles per second to the display, and the editor redraws at about 30 frames per second. At 48 kHz, it captures one tile every 12 processed tiles: **62.5 captures per second**, representing about two seconds of sampled history. Audio between these captures is still processed normally. The mosaic contains snapshots spaced through recent audio, rather than a continuous recording of every sample. A frame is published after its corresponding final output has completed; this adds no audio latency.

**Click a tile in either panel** to freeze the history and outline the matching tile in both images. Two enlarged 8×8 inspectors show its input and selected result. The readout gives its position in the displayed history, its age relative to the newest captured tile, and its **final output peak in dBFS**. That peak always measures final output, even when viewing JPEG Wet, and turns red above 0 dBFS. While live, the inspectors follow the latest tile.

**FREEZE IMAGE** holds the captured history while audio and level meters continue. View, palette, and channel controls can still redraw that held data. **RESUME LIVE** resumes adding new captures and clears the tile selection; audio arriving while frozen is not replayed into the display. The live history resets when processing is prepared again, incoming capture positions restart, or the sample rate or channel count changes. If that happens while frozen, the held picture stays visible until new captures arrive after Resume Live.

### Save a visual snapshot

**SAVE PNG** exports a **1080×352** image containing the input and selected result at equal size, with channel, view, palette, tile count, and an amplitude legend. It captures the picture when the button is pressed, so audio and the live display can continue while you choose a filename. It is available after the first captured tile and also works while frozen. The export contains the image pair rather than audio or animation; it does not change the sound or Freeze setting.

### Controls, meters, and silence

- **JPEG Quality, Fry, and Pixel Depth** change newly arriving processed tiles. Lower quality quantizes image detail more coarsely, Fry increases contrast and edge emphasis, and lower Pixel Depth produces fewer amplitude/color levels. Older tiles retain their captured values.
- **Mix, Output, and Bypass** affect newly captured FINAL OUT tiles. JPEG WET stays available for inspecting the compression stage independently.
- **IN / OUT** show actual peak levels across the active audio channels, with meter decay. OUT reflects Mix, Output gain, and Bypass. A signal on only one stereo channel can move these meters while the other channel's picture remains flat.
- **DCT DETAIL** is the percentage of the inspected channel/tile's 64 quantized DCT coefficients that are nonzero, including its average-value coefficient. It follows the latest tile while live and the selected tile while frozen. It is measured before Pixel Depth reduction and varies with both the signal and compression settings; it does not measure JPEG file size or perceptual fidelity.
- **Silence** gradually fills both images with mid-gray in Gray mode or red-orange in Colour mode. Before any captured tile arrives, the editor shows a checkerboard. If the host stops sending audio blocks, the last picture stays visible; the playback-status label follows incoming frame activity.

These visual controls do not change the sound parameters and reset when the editor is reopened. The surrounding paper grain, stamps, and registration marks are static interface artwork. See the [visual style notes](docs/visual-style.md).

## Build from source

Requires a C++17 toolchain, Git, and **CMake 3.24+**. The first configuration downloads JUCE **8.0.13**, pinned to a commit and SHA-256 checksum. Clone the repository first:

```sh
git clone https://github.com/mitchaiet/deep-fry.git
cd deep-fry
```

**macOS:** Install Xcode command-line tools, then run `./scripts/build.sh`. It configures, builds, and runs CTest. Builds are universal by default; for a single-architecture development build, configure CMake with `-DCMAKE_OSX_ARCHITECTURES=arm64` or `-DCMAKE_OSX_ARCHITECTURES=x86_64`.

**Windows:** Install Visual Studio 2022 with **Desktop development with C++** and a Windows SDK, then run these commands in PowerShell from the repository root:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

Products are in `build/DeepFry_artefacts/Release/`. Follow the platform installation guide to copy the VST3 into your host's plug-in folder. On macOS, `./scripts/install-macos.sh` copies the VST3 and Audio Unit for your user account; it does not install the standalone app or create the release installer's backups.

For an **offline build**, download `Deep-Fry-0.2.0-source.tar.gz` from the [release](https://github.com/mitchaiet/deep-fry/releases/tag/v0.2.0). This shared source archive includes Deep Fry, the pinned JUCE source archive, license notices, and a source manifest. You still need your platform's compiler/SDK and CMake. On macOS, extract it and run `./scripts/build-offline.sh` from the extracted directory. See the [Windows offline build instructions](docs/installation-windows.md#offline-source-build) for Windows. GitHub's automatic “Source code” archives do not contain the vendored JUCE archive.

To build and test only the dependency-free codec:

```sh
cmake -S . -B build-codec -DDEEPFRY_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-codec --config Release
ctest --test-dir build-codec -C Release --output-on-failure
```

Integration tests cover processing timing, channel isolation, host buffer sizes, automation, saved state, and the live display. `DeepFryVerify --artifacts <directory>` also generates synthetic before/after audio and native editor screenshots. See [validation results and reproduction commands](docs/validation.md) for tested configurations and remaining limitations.

## Package a macOS release

Packaging requires **Python 3** in addition to the build tools. Python is not required to install the binary release or run the offline build script. Build and test the universal macOS Release configuration, then commit any source changes: release packaging requires a clean Git tree so its manifest identifies the exact source commit.

From the repository root, download the pinned JUCE source archive into the ignored `.context/` directory and package the release:

```sh
mkdir -p .context
curl --fail --location \
  https://codeload.github.com/juce-framework/JUCE/tar.gz/7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2 \
  --output .context/juce-8.0.13.tar.gz
./scripts/package-macos.sh --juce-archive .context/juce-8.0.13.tar.gz
cd dist
shasum -a 256 -c Deep-Fry-0.2.0-macOS-universal.zip.sha256
shasum -a 256 -c Deep-Fry-0.2.0-source.tar.gz.sha256
```

The packager verifies the JUCE archive's SHA-256 against the CMake pin and checks bundle versions, both architectures, the minimum macOS version, and code signatures. It writes a binary ZIP, complete source archive, and SHA-256 sidecars under `dist/`. The ZIP includes all three formats, installer, license notices, and a release manifest; the source archive includes vendored JUCE and an offline build script. It does not install the bundles. [Installer verification instructions](docs/installation.md#isolated-installer-check) use a separate directory.

## Package a Windows release

After a Windows Release build and successful CTest run, use PowerShell:

```powershell
./scripts/package-windows.ps1 -Artifacts build/DeepFry_artefacts/Release
```

The ZIP contains the complete VST3 bundle, portable standalone EXE, installation
instructions, license notices, and a manifest with architecture, version, DLL
imports, source commit, and file hashes. A SHA-256 sidecar accompanies it.
The source tree must be committed and clean. Pass `-SourceArchive` with the
matching complete source archive to record its checksum in the manifest.

The Windows release can also be cross-compiled on macOS with Clang, Microsoft's
SDK and static C++ runtime, and the unmodified pinned JUCE source. See the
[Windows cross-build instructions](docs/build-windows-cross.md) for its exact
toolchain, packaging command, and validation procedure.

## License

Deep Fry is licensed under the **GNU Affero General Public License v3.0 only**. See [LICENSE](LICENSE) and [COPYRIGHT](COPYRIGHT).

Dependency licenses and source provenance are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Deep Fry uses [JUCE](https://github.com/juce-framework/JUCE/tree/8.0.13); the JPEG quality scaling reference is [libjpeg-turbo's parameter implementation](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/main/src/jcparam.c). Impact is used when available as a system font and is not bundled.
