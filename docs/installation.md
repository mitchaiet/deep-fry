# Installing Deep Fry on macOS

For a Windows PC, use the [Windows installation guide](installation-windows.md).

Download the ZIP and its `.sha256` file from the [latest release](https://github.com/mitchaiet/deep-fry/releases/latest). Version 0.1.1 includes universal **Apple Silicon and Intel** builds of the VST3, Audio Unit, and standalone app. It is built for **macOS 11 or later** and tested on macOS 26.2; earlier supported versions have not been runtime-tested. Use a compatible 64-bit host for the plug-in formats.

## Install or update

1. Extract the complete `Deep-Fry-0.1.1-macOS-universal.zip` archive. Keep `Install.command` beside the `VST3`, `AU`, and `Standalone` folders.
2. Save your work and close your DAW before replacing an installed copy.
3. Double-click **Install.command**. It opens in Terminal and installs for your user account without an administrator password.
4. Wait for the success message, then press Return to close the installer.
5. Reopen your DAW and rescan its plug-ins.

| Format | Installed location |
| --- | --- |
| VST3 | `~/Library/Audio/Plug-Ins/VST3/Deep Fry.vst3` |
| Audio Unit | `~/Library/Audio/Plug-Ins/Components/Deep Fry.component` |
| Standalone | `~/Applications/Deep Fry.app` |

In Finder, choose **Go → Go to Folder** and paste a path to open one of these locations. `~` means your home folder.

The installer copies and verifies all three new bundles before replacing an installed version. It keeps dated copies of previous bundles under `~/Library/Application Support/Deep Fry/Backups/`. If a replacement fails, it attempts to restore the previous installation and prints any paths that need manual recovery. It never quits your audio host for you.

### macOS security prompts

These downloads are **ad-hoc signed**. They have not been signed with an Apple Developer ID or notarized by Apple, so a downloaded installer, app, or plug-in may be blocked by macOS.

If macOS says the developer cannot be verified or Apple cannot check the software, and you trust this release, first attempt to open it. Then open macOS security settings (**System Settings → Privacy & Security** on recent versions), find the blocked item, and choose **Open Anyway** if that option is offered. Confirm the dialog and retry. This is Apple's documented per-item approval flow. [Apple: Safely open apps on your Mac](https://support.apple.com/en-us/102445)

Approval behavior can differ for host-loaded plug-ins and managed Macs; the installer does not guarantee that macOS will allow the downloaded binaries to load. If no approval option appears, [build from source](../README.md#build-from-source) or [report the exact message](https://github.com/mitchaiet/deep-fry/issues). If macOS reports malware or damaged software, do not use the unknown-developer exception as a workaround; download a fresh copy and check its checksum. [Apple's explanation of security alerts](https://support.apple.com/en-us/102445)

### Verify the download

Put the ZIP and `.sha256` file in the same folder. Open Terminal in that folder and run:

```sh
shasum -a 256 -c Deep-Fry-0.1.1-macOS-universal.zip.sha256
```

The result should end in `OK`. If it fails, download both files again before installing. The checksum verifies that the archive matches the published download; it is separate from Apple's signing and notarization checks.

## Ableton Live 11

Open **Preferences → Plug-Ins**, enable **Use VST3 Plug-In System Folders**, then click **Rescan**. In the browser, choose **Plug-Ins**, find **Deep Fry**, and drop it onto an audio track or after an instrument. Open the plug-in window, choose **Meme**, and start playback. Live scans both the system and user VST3 locations. [Ableton's macOS plug-in guide](https://help.ableton.com/hc/en-us/articles/209068929-Using-AU-and-VST-plug-ins-on-macOS)

If it does not appear:

1. Check that the VST3 bundle is in the exact location above and that your macOS version meets the requirement.
2. Address any macOS security prompt, then restart Live and allow scanning to finish.
3. Confirm **Use VST3 Plug-In System Folders** is enabled. Leave the VST3 custom folder off unless you intentionally use one.
4. Hold **Option** while clicking **Rescan** for a full rescan. [Ableton's missing plug-in troubleshooting](https://help.ableton.com/hc/en-us/articles/115000349184-VST-AU-plug-in-doesn-t-appear-in-Live-s-Browser)

The included binaries contain both `arm64` and `x86_64` code. Deep Fry does not require switching an Apple Silicon installation of Live to Rosetta just to match the plug-in's architecture.

## Standalone app

Open `~/Applications/Deep Fry.app`, open the audio-device settings, and select your input and output interface. Allow microphone access when prompted if you want live input. Start with a low monitoring volume and use headphones with a microphone to avoid feedback.

## Manual installation, restore, or removal

For a manual install, close your audio host and copy each complete bundle from the extracted archive into its matching location in the table. Create the destination folders if needed. When updating manually, move the old bundle aside first and replace it as a whole.

To restore a version saved by `Install.command`, close your host, open `~/Library/Application Support/Deep Fry/Backups/`, and choose the dated backup you want. Move the current installed bundle aside, then copy the backed-up bundle from its `VST3`, `AU`, or `Standalone` subfolder to the matching installation location. Reopen and rescan the host.

To uninstall, close your audio host and move the installed `Deep Fry.vst3`, `Deep Fry.component`, and `Deep Fry.app` bundles to the Trash. Backups remain in `~/Library/Application Support/Deep Fry/Backups/`; remove them separately if you no longer need them. DAW sessions and their saved effect settings are not removed.

## Isolated installer check

For release testing, create an empty directory and pass its absolute path as the installation prefix. From the extracted release folder:

```sh
./Install.command --non-interactive --prefix /absolute/existing/test-directory
```

The test directory receives its own `Library` and `Applications` folders. Running the command again exercises the update and backup path without changing your actual plug-in installation. `--non-interactive` skips the final Return prompt.
