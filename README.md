# Deep Fry

A JPEG compression audio effect. Deep Fry turns short blocks of audio into grayscale images, applies JPEG quantization, contrast, sharpening, and pixel reduction, then converts the result back into audio. Its live display shows the images being processed.

![Deep Fry processing audio: original image, live processed image, and effect controls](docs/deep-fry-ui.png)

**[Download the latest release](https://github.com/mitchaiet/deep-fry/releases/latest)** · [v0.1.1 release notes](https://github.com/mitchaiet/deep-fry/releases/tag/v0.1.1) · [Report an issue](https://github.com/mitchaiet/deep-fry/issues)

## Install

| Download | Includes | Requirements |
| --- | --- | --- |
| [macOS universal ZIP](https://github.com/mitchaiet/deep-fry/releases/download/v0.1.1/Deep-Fry-0.1.1-macOS-universal.zip) | VST3, Audio Unit, standalone app | macOS 11+, Apple Silicon or Intel |
| [Windows x64 ZIP](https://github.com/mitchaiet/deep-fry/releases/download/v0.1.1/Deep-Fry-0.1.1-Windows-x64.zip) | VST3, standalone EXE | Windows 10 version 1607+, 64-bit Intel or AMD |

**macOS:** Extract the complete ZIP, save your session and close your DAW, then open `Install.command`. It installs for your user account and backs up existing versions. The binaries are ad-hoc signed and **not Apple-notarized**, so macOS may require explicit approval. [Mac installation, security prompts, and removal](docs/installation.md)

**Windows:** Extract the complete ZIP, close your DAW, and copy the entire `VST3\Deep Fry.vst3` bundle into `C:\Program Files\Common Files\VST3`. Administrator permission may be needed. The standalone EXE runs from its extracted folder. [Windows installation and removal](docs/installation-windows.md)

Reopen your DAW and rescan plug-ins after installing. The Mac build targets macOS 11+ and has been tested on macOS 26.2; earlier supported versions have not been runtime-tested. See [validation results](docs/validation.md) for the tested configurations. Linux binaries are not currently provided.

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
| **Freeze** | Holds the image; audio keeps processing. |

Four presets—**Clean-ish**, **Meme**, **Deep fried**, and **Lost cause**—set the five sound controls. Sound parameters support host automation and are saved with the DAW session. Mono and stereo are supported; stereo channels are processed independently.

## How it works

```text
64 audio samples → 8×8 grayscale image → contrast / sharpening
  → 2D DCT → JPEG quantization → inverse DCT
  → pixel reduction → 64 audio samples → mix / output
```

Each tile uses JPEG's standard luminance quantization table and IJG quality scaling. The processor runs the lossy image operations in memory. It does not write `.jpg` files: the file container and entropy coding do not change decoded pixels. This is an audio-oriented implementation, rather than a byte-identical libjpeg round trip.

Samples map symmetrically around gray 128, with an exact zero level at every pixel depth. Digital silence stays silent. The processed path clips input beyond ±1 during image conversion; dry and bypass preserve finite input. Tile boundaries are intentionally audible, and extreme contrast or sharpening can alias.

The original thumbnail and processed mosaic show recent **left-channel or mono** tiles from the wet processing path, before Mix, Output, and Bypass. The large view uses a display palette; its changing pixels come from the audio processor. The coefficient meter counts retained DCT coefficients, not JPEG file size. The Impact wordmark and printed panels are described in the [visual style notes](docs/visual-style.md).

Latency is **64 samples**: 1.45 ms at 44.1 kHz, or 1.33 ms at 48 kHz. The host receives this latency, and dry, wet, and bypass use the same delay. The audio callback uses fixed storage without file I/O, allocation, or locks. Closing or freezing the editor does not interrupt processing.

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

For an **offline build**, download `Deep-Fry-0.1.1-source.tar.gz` from the [release](https://github.com/mitchaiet/deep-fry/releases/tag/v0.1.1). This shared source archive includes Deep Fry, the pinned JUCE source archive, license notices, and a source manifest. You still need your platform's compiler/SDK and CMake. On macOS, extract it and run `./scripts/build-offline.sh` from the extracted directory. See the [Windows offline build instructions](docs/installation-windows.md#offline-source-build) for Windows. GitHub's automatic “Source code” archives do not contain the vendored JUCE archive.

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
shasum -a 256 -c Deep-Fry-0.1.1-macOS-universal.zip.sha256
shasum -a 256 -c Deep-Fry-0.1.1-source.tar.gz.sha256
```

The packager verifies the JUCE archive's SHA-256 against the CMake pin and checks bundle versions, both architectures, the minimum macOS version, and code signatures. It writes a binary ZIP, complete source archive, and SHA-256 sidecars under `dist/`. The ZIP includes all three formats, installer, license notices, and a release manifest; the source archive includes vendored JUCE and an offline build script. It does not install the bundles. [Installer verification instructions](docs/installation.md#isolated-installer-check) use a separate directory.

## License

Deep Fry is licensed under the **GNU Affero General Public License v3.0 only**. See [LICENSE](LICENSE) and [COPYRIGHT](COPYRIGHT).

Dependency licenses and source provenance are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Deep Fry uses [JUCE](https://github.com/juce-framework/JUCE/tree/8.0.13); the JPEG quality scaling reference is [libjpeg-turbo's parameter implementation](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/main/src/jcparam.c). Impact is used when available as a system font and is not bundled.
