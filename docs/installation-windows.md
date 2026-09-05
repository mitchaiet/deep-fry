# Installing Deep Fry on Windows

Download `Deep-Fry-0.1.1-Windows-x64.zip` and its `.sha256` file from the [latest release](https://github.com/mitchaiet/deep-fry/releases/latest). The ZIP includes the **VST3 plug-in** and a **portable standalone EXE** for 64-bit Intel or AMD systems running **Windows 10 version 1607 or later**. The VST3 needs a compatible 64-bit audio host. Audio Unit is available in the [macOS package](installation.md).

## Install the VST3

1. Right-click the ZIP and choose **Extract All**. Open the extracted `Deep-Fry-0.1.1-Windows-x64` folder.
2. Save your work and close your DAW.
3. Copy the entire `VST3\Deep Fry.vst3` folder into `C:\Program Files\Common Files\VST3`. Create the destination folder if needed. Windows may request administrator permission for this copy.
4. Reopen your DAW and rescan its plug-ins.

The final path should be:

```text
C:\Program Files\Common Files\VST3\Deep Fry.vst3\
```

`Deep Fry.vst3` is a bundle directory. Keep its contents together, including the nested binary and resources. Use the regular `Program Files` path, not `Program Files (x86)`. This is the standard 64-bit VST3 location used by Live. [Ableton's Windows plug-in guide](https://help.ableton.com/hc/en-us/articles/209071729-Using-VST-plug-ins-on-Windows)

This Windows package uses manual installation and does not create automatic backups. When updating, close the DAW and move the existing `Deep Fry.vst3` bundle to a backup folder outside the VST3 scan path before copying in the new one.

## Ableton Live 11

Open **Options → Preferences → Plug-Ins**, enable **Use VST3 Plug-In System Folders**, and click **Rescan**. Under **Plug-Ins** in the browser, find **Deep Fry** and drag it onto an audio track or after an instrument. Open its window, choose **Meme**, and play audio. [Ableton's setup instructions](https://help.ableton.com/hc/en-us/articles/209071729-Using-VST-plug-ins-on-Windows)

If Deep Fry does not appear, confirm the exact bundle location above, restart Live, and wait for scanning to finish. If needed, hold **Alt** while clicking **Rescan** to perform a full rescan. [Ableton's missing plug-in troubleshooting](https://help.ableton.com/hc/en-us/articles/115000349184-VST-AU-plug-in-doesn-t-appear-in-Live-s-Browser)

The VST3 uses your DAW's audio driver, including ASIO when selected in Live. There is no separate Deep Fry audio-device selection while running inside a DAW.

## Standalone app

Open `Standalone\Deep Fry.exe` from the extracted release folder. You can keep this folder in a permanent location of your choice; it does not belong in the VST3 plug-in folder.

In the app's audio-device settings, choose your input and output device. The Windows standalone supports Windows audio devices through WASAPI or DirectSound. Start at a low monitoring volume and use headphones if processing a live microphone.

The Windows executables are **not Authenticode-signed**, so Windows or SmartScreen may show an unknown-publisher warning. Download from this project's release page and verify the checksum. If your device's security policy blocks the app, [report the exact message](https://github.com/mitchaiet/deep-fry/issues) or build from source; no security-setting changes are part of installation.

## Verify the download

Put the ZIP and its `.sha256` file in the same folder. Open PowerShell in that folder and run:

```powershell
$expected = ((Get-Content "Deep-Fry-0.1.1-Windows-x64.zip.sha256") -split '\s+')[0]
$actual = (Get-FileHash "Deep-Fry-0.1.1-Windows-x64.zip" -Algorithm SHA256).Hash
if ($actual -ne $expected) { throw "Checksum mismatch. Download both files again." }
"Checksum OK"
```

The expected result is `Checksum OK`. This checks the archive against the published download; it does not provide an Authenticode publisher signature.

## Remove or restore

To uninstall, close your DAW and delete `C:\Program Files\Common Files\VST3\Deep Fry.vst3`. Delete your extracted standalone folder if you no longer want the app. DAW sessions and their saved effect settings are not removed.

To restore an older version, close the DAW, move the current bundle aside, and copy your backed-up whole `Deep Fry.vst3` bundle into the VST3 folder. Reopen and rescan the DAW.

## Offline source build

Install **Visual Studio 2022**, its **Desktop development with C++** workload and a Windows SDK, plus **CMake 3.24+**. Download the shared `Deep-Fry-0.1.1-source.tar.gz` asset from the [release](https://github.com/mitchaiet/deep-fry/releases/tag/v0.1.1). The build uses its included JUCE archive without downloading dependencies.

From PowerShell in the folder containing the source download:

```powershell
cmake -E tar xzf .\Deep-Fry-0.1.1-source.tar.gz
Set-Location .\Deep-Fry-0.1.1-source
Push-Location .\vendor
cmake -E tar xzf .\juce-8.0.13.tar.gz
Pop-Location
$juceSource = (Resolve-Path ".\vendor\JUCE-7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2").Path
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 "-DFETCHCONTENT_SOURCE_DIR_JUCE=$juceSource" -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

The VST3 and standalone EXE are written under `build\DeepFry_artefacts\Release`. For a normal online build from Git, see [the main build instructions](../README.md#build-from-source).
