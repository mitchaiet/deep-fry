DEEP FRY @VERSION@ FOR MACOS

Audio becomes 8 x 8 image tiles, gets JPEG compression artifacts, and becomes
audio again. The live display shows the image before and after processing.

REQUIREMENTS

This binary release requires macOS 26.2 or later. All three formats include
both Apple Silicon (arm64) and Intel (x86_64) code. The VST3 and Audio Unit
require a compatible 64-bit audio host; a standalone application is included.

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
Install.command, README.txt, and LICENSES/.

See LICENSES/ for third-party license notices and their source provenance.
These notices do not grant a license to Deep Fry's original source code.
This software is based in part on the work of the Independent JPEG Group.

Source and releases: https://github.com/mitchaiet/deep-fry
