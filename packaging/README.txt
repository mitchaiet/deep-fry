DEEP FRY @VERSION@ FOR MACOS

Audio becomes 8 x 8 image tiles, gets JPEG compression artifacts, and becomes
audio again. The live display shows the image before and after processing.

REQUIREMENTS

This binary release targets macOS @MIN_MACOS@ or later. All three formats include
both Apple Silicon (arm64) and Intel (x86_64) code. The VST3 and Audio Unit
require a compatible 64-bit audio host; a standalone application is included.
Release verification was performed on macOS 26.2; older supported OS versions
have not been tested on physical machines.

The bundles are locally ad-hoc signed, not Developer ID signed or notarized by
Apple. macOS or your host may block downloaded plug-ins or the installer.
Only install if you trust this source. The installer does not remove quarantine
attributes or change Gatekeeper settings. You can also build from the source
repository using its README instructions.

INSTALLATION

1. Extract the entire ZIP. Keep Install.command beside the VST3, AU, and
   Standalone folders.
2. Save your work and close your audio host before updating an existing copy.
3. Double-click Install.command. No administrator password is needed.
4. Reopen your audio host and rescan its plug-ins.

The installer puts the formats here:
  VST3:       ~/Library/Audio/Plug-Ins/VST3/Deep Fry.vst3
  Audio Unit: ~/Library/Audio/Plug-Ins/Components/Deep Fry.component
  Standalone: ~/Applications/Deep Fry.app

Existing bundles are backed up under:
  ~/Library/Application Support/Deep Fry/Backups/<date-and-unique-suffix>/

The installer stages and verifies all bundles before replacing them. It restores
the previous installation if a replacement fails. It does not quit applications.

From Terminal, run ./Install.command --non-interactive to omit the final pause.
For an isolated installation test, add --prefix /absolute/existing/directory.
This creates the Library and Applications folders under that directory instead
of installing into your home folder.

ABLETON LIVE 11

Open Live > Preferences > Plug-Ins. Enable Use VST3 Plug-In System Folders,
then click Rescan. Find Deep Fry in the Plug-Ins browser and drag it onto an
audio track. Open the device's plug-in window, choose a preset, and start audio.

FIRST PLAY

Start at a low monitoring volume. Lower JPEG Quality for more compression
damage, increase Fry for stronger distortion, and use Mix to blend dry audio.

MANUAL INSTALLATION / REMOVAL

With your host closed, move the matching bundle into the folder listed above.
Create the folder if needed, and replace the entire old bundle when updating.
Remove those installed bundles to uninstall; backups are retained separately.

PACKAGE CONTENTS AND LICENSES

VST3/Deep Fry.vst3, AU/Deep Fry.component, Standalone/Deep Fry.app,
Install.command, README.txt, CHANGELOG.md, LICENSE, COPYRIGHT,
THIRD_PARTY_NOTICES.md, LICENSES/, and RELEASE-MANIFEST.json.

Deep Fry is licensed under the GNU Affero General Public License version 3
(AGPL-3.0-only). See LICENSE for the full terms and COPYRIGHT for attribution.
See THIRD_PARTY_NOTICES.md and LICENSES/ for dependency license notices.
This software is based in part on the work of the Independent JPEG Group.

COMPLETE SOURCE AND CHECKSUMS

Download Deep-Fry-@VERSION@-source.tar.gz from the same release for the complete
corresponding source, including the pinned JUCE archive and build scripts.
Extract it and run ./scripts/build-offline.sh to build without downloading JUCE.
Xcode Command Line Tools and CMake 3.24 or newer must already be installed.
GitHub's automatic source archives contain the project but not vendored JUCE.

The release provides .sha256 sidecars for both archives. In Terminal, run:
  shasum -a 256 -c Deep-Fry-@VERSION@-macOS-universal.zip.sha256
  shasum -a 256 -c Deep-Fry-@VERSION@-source.tar.gz.sha256

RELEASE-MANIFEST.json records the exact source commit, dependency pin, binary
checksums, and complete-source archive checksum. This release is ad-hoc signed
and has not been notarized; no Developer ID signature is supplied.

Source: https://github.com/mitchaiet/deep-fry
Release: https://github.com/mitchaiet/deep-fry/releases/tag/v@VERSION@
