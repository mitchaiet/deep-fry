DEEP FRY @VERSION@ FOR WINDOWS X64

Audio becomes 8 x 8 image tiles, gets JPEG compression artifacts, and becomes
audio again. The live display shows the image before and after processing.

REQUIREMENTS

Windows 10 or 11, 64-bit x64 (Intel or AMD). This package contains an x64 VST3
audio effect and an x64 standalone application. No Audio Unit is included.
The VST3 requires a compatible 64-bit host, such as Ableton Live 11 or later.
The Microsoft C++ runtime is linked statically; no separate Visual C++
Redistributable installer is required for these binaries.

The binaries do not have an Authenticode publisher signature. Windows may
show a SmartScreen or publisher warning. This package does not change Windows
security settings. Download only from the project's release page and compare
the supplied SHA-256 checksum, or build the source yourself.

VST3 INSTALLATION

1. Extract the entire ZIP.
2. Save your work and close your audio host before updating an existing copy.
3. Copy the entire VST3/Deep Fry.vst3 folder into:
     C:\Program Files\Common Files\VST3\
   Windows may request administrator permission for this folder. The final
   path should be C:\Program Files\Common Files\VST3\Deep Fry.vst3\.
   Keep the bundle's Contents folder intact; do not copy just the inner file.
4. Reopen your audio host and rescan its plug-ins.

When updating, keep a backup of your old Deep Fry.vst3 folder and replace the
entire folder. To uninstall, close the host and remove that installed folder.

ABLETON LIVE 11

Open Options > Preferences > Plug-Ins. Enable Use VST3 Plug-In System Folders,
then click Rescan. Find Deep Fry in the Plug-Ins browser and drag it onto an
audio track. Open the device's plug-in window, choose a preset, and play audio.

PORTABLE STANDALONE APPLICATION

Open Standalone/Deep Fry.exe from the extracted package, or copy it to a folder
you choose and run it there. Select your input and output devices in its audio
settings. WASAPI and DirectSound device support is included. ASIO is not enabled
in the standalone build; the VST3 uses the devices configured in your host.
To remove the portable application, close it and delete Deep Fry.exe.

FIRST PLAY

Start at a low monitoring volume. Lower JPEG Quality for more compression
damage, increase Fry for stronger distortion, and use Mix to blend dry audio.

PACKAGE CONTENTS AND LICENSES

VST3/Deep Fry.vst3/, Standalone/Deep Fry.exe, README.txt, CHANGELOG.md,
LICENSE, COPYRIGHT, THIRD_PARTY_NOTICES.md, LICENSES/, RELEASE-MANIFEST.json.

Deep Fry is licensed under the GNU Affero General Public License version 3
(AGPL-3.0-only). See LICENSE for the full terms and COPYRIGHT for attribution.
See THIRD_PARTY_NOTICES.md and LICENSES/ for dependency license notices.
This software is based in part on the work of the Independent JPEG Group.

COMPLETE SOURCE AND CHECKSUMS

Download Deep-Fry-@VERSION@-source.tar.gz from the same release for the complete
corresponding source, including the pinned JUCE archive and build scripts.
See its README for Windows build instructions using Visual Studio and CMake.
GitHub's automatic source archives do not contain the vendored JUCE archive.

In PowerShell, compute the ZIP checksum with:
  Get-FileHash .\Deep-Fry-@VERSION@-Windows-x64.zip -Algorithm SHA256
Compare it with the value in Deep-Fry-@VERSION@-Windows-x64.zip.sha256.

RELEASE-MANIFEST.json records the exact source commit, dependency pin,
binary architecture, DLL imports, and file checksums. The source archive is
available at the release URL below.

Source: https://github.com/mitchaiet/deep-fry
Release: https://github.com/mitchaiet/deep-fry/releases/tag/v@VERSION@
