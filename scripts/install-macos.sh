#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ "$(uname -s)" != Darwin ]]; then
    echo "This installer is for macOS. See README.md for other platforms." >&2
    exit 1
fi
artifacts="${1:-build/DeepFry_artefacts/Release}"
for bundle in "$artifacts/VST3/Deep Fry.vst3" "$artifacts/AU/Deep Fry.component"; do
    if [[ ! -d "$bundle" ]]; then
        echo "Missing $bundle. Run ./scripts/build.sh first." >&2
        exit 1
    fi
done
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3" "$HOME/Library/Audio/Plug-Ins/Components"
ditto "$artifacts/VST3/Deep Fry.vst3" "$HOME/Library/Audio/Plug-Ins/VST3/Deep Fry.vst3"
ditto "$artifacts/AU/Deep Fry.component" "$HOME/Library/Audio/Plug-Ins/Components/Deep Fry.component"
echo "Installed Deep Fry VST3 and Audio Unit for the current user. Rescan plugins in your DAW."
