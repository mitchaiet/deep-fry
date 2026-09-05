#!/usr/bin/env bash
# Package an existing Release build and its complete corresponding source.
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
artifacts="$project_dir/build/DeepFry_artefacts/Release"
output_dir="$project_dir/dist"
juce_archive=""
allow_dirty=()

usage() {
    echo "Usage: $0 [--artifacts DIRECTORY] [--output DIRECTORY] [--juce-archive FILE] [--allow-dirty]"
    echo "Packages an existing universal macOS Release build and complete source with SHA-256 files."
    echo "Requires macOS, Python 3, and the pinned JUCE tar archive; does not build or install."
    echo "Commit changes first. --allow-dirty is for local packaging checks only."
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --artifacts|--output|--juce-archive)
            [[ $# -ge 2 && -n "$2" ]] || { usage >&2; exit 2; }
            case "$1" in
                --artifacts) artifacts="$2" ;;
                --output) output_dir="$2" ;;
                --juce-archive) juce_archive="$2" ;;
            esac
            shift 2
            ;;
        --allow-dirty) allow_dirty=(--allow-dirty); shift ;;
        -h|--help) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
done

[[ "$(uname -s)" == Darwin ]] || { echo "Packaging requires macOS." >&2; exit 1; }
command -v python3 >/dev/null || { echo "Packaging requires Python 3." >&2; exit 1; }
version="$(sed -nE 's/^project\(DeepFry VERSION ([0-9]+\.[0-9]+\.[0-9]+) .*/\1/p' "$project_dir/CMakeLists.txt")"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
    echo "Cannot read the DeepFry version from CMakeLists.txt." >&2
    exit 1
}
if [[ -z "$juce_archive" ]]; then
    juce_version="$(sed -nE 's/^# JUCE ([0-9]+\.[0-9]+\.[0-9]+),.*/\1/p' "$project_dir/CMakeLists.txt")"
    juce_archive="$project_dir/vendor/juce-$juce_version.tar.gz"
fi

license_dir="$project_dir/packaging/LICENSES"
[[ -d "$license_dir" && -n "$(find "$license_dir" -type f -print -quit)" ]] || {
    echo "Missing distribution license notices: $license_dir" >&2
    exit 1
}
for required in Install.command README.txt; do
    [[ -f "$project_dir/packaging/$required" ]] || { echo "Missing packaging/$required" >&2; exit 1; }
done
for required in LICENSE COPYRIGHT CHANGELOG.md THIRD_PARTY_NOTICES.md; do
    [[ -f "$project_dir/$required" ]] || { echo "Missing $required" >&2; exit 1; }
done

bundle_paths=("VST3/Deep Fry.vst3" "AU/Deep Fry.component" "Standalone/Deep Fry.app")
minimum_macos=""
verify_bundle() {
    local bundle="$1" executable_name executable arch minimum_versions bundle_version bundle_minimum signature_info
    [[ -d "$bundle" ]] || { echo "Missing bundle: $bundle" >&2; return 1; }
    executable_name="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$bundle/Contents/Info.plist")"
    executable="$bundle/Contents/MacOS/$executable_name"
    [[ -f "$executable" && -x "$executable" ]] || { echo "Missing bundle executable: $executable" >&2; return 1; }
    bundle_version="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$bundle/Contents/Info.plist")"
    [[ "$bundle_version" == "$version" ]] || {
        echo "Bundle version $bundle_version does not match project version $version: $bundle" >&2
        return 1
    }
    for arch in arm64 x86_64; do
        /usr/bin/lipo "$executable" -verify_arch "$arch"
    done
    minimum_versions="$(xcrun vtool -show-build "$executable" | awk '$1 == "minos" { print $2 }')"
    bundle_minimum="$(printf '%s\n' "$minimum_versions" | sort -u)"
    [[ "$bundle_minimum" =~ ^[0-9]+\.[0-9]+(\.[0-9]+)?$ && "$(printf '%s\n' "$minimum_versions" | wc -l | tr -d ' ')" == 2 ]] || {
        echo "Expected one matching minimum macOS version across both architectures: $bundle" >&2
        return 1
    }
    if [[ -z "$minimum_macos" ]]; then minimum_macos="$bundle_minimum"; fi
    [[ "$bundle_minimum" == "$minimum_macos" ]] || { echo "Bundles have different minimum macOS versions." >&2; return 1; }
    /usr/bin/codesign --verify --deep --strict "$bundle"
    signature_info="$(/usr/bin/codesign -dv --verbose=2 "$bundle" 2>&1)"
    [[ "$signature_info" == *"Signature=adhoc"* ]] || {
        echo "Expected an ad-hoc signature. Update release signing metadata before packaging another signature type." >&2
        return 1
    }
}

for relative in "${bundle_paths[@]}"; do verify_bundle "$artifacts/$relative"; done

mkdir -p "$output_dir"
output_dir="$(cd "$output_dir" && pwd)"
package_name="Deep-Fry-$version-macOS-universal"
stage_dir="$(mktemp -d "$output_dir/.deep-fry-package.XXXXXX")"
trap 'rm -rf "$stage_dir"' EXIT
package_dir="$stage_dir/$package_name"
mkdir -p "$package_dir"

python3 "$project_dir/scripts/package-source.py" --juce-archive "$juce_archive" --output "$stage_dir" ${allow_dirty[@]+"${allow_dirty[@]}"}

for relative in "${bundle_paths[@]}"; do
    mkdir -p "$package_dir/$(dirname "$relative")"
    /usr/bin/ditto --norsrc --noextattr "$artifacts/$relative" "$package_dir/$relative"
done
/usr/bin/ditto --norsrc --noextattr "$license_dir" "$package_dir/LICENSES"
sed "s/@MIN_MACOS@/$minimum_macos/g" "$project_dir/packaging/Install.command" > "$package_dir/Install.command"
/bin/chmod 755 "$package_dir/Install.command"
sed -e "s/@VERSION@/$version/g" -e "s/@MIN_MACOS@/$minimum_macos/g" "$project_dir/packaging/README.txt" > "$package_dir/README.txt"
/bin/cp "$project_dir/LICENSE" "$project_dir/CHANGELOG.md" "$package_dir/"
sed 's|packaging/LICENSES/|LICENSES/|g' "$project_dir/COPYRIGHT" > "$package_dir/COPYRIGHT"
sed 's|packaging/LICENSES/|LICENSES/|g' "$project_dir/THIRD_PARTY_NOTICES.md" > "$package_dir/THIRD_PARTY_NOTICES.md"
find "$package_dir" -name .DS_Store -type f -delete
for relative in "${bundle_paths[@]}"; do verify_bundle "$package_dir/$relative"; done

python3 - "$package_dir" "$stage_dir/Deep-Fry-$version-source.tar.gz" "$minimum_macos" <<'PY'
import hashlib
import json
import sys
import tarfile
from pathlib import Path

package, source_archive, minimum = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3]
with tarfile.open(source_archive, "r:gz") as source:
    manifest_name = next(name for name in source.getnames() if name.count("/") == 1 and name.endswith("/SOURCE-MANIFEST.json"))
    source_manifest = json.load(source.extractfile(manifest_name))
manifest = {
    "project": "Deep Fry",
    "version": source_manifest["version"],
    "license": source_manifest["license"],
    "source_repository": source_manifest["source_repository"],
    "source_commit": source_manifest["source_commit"],
    "source_tree_dirty": source_manifest["source_tree_dirty"],
    "minimum_macos": minimum,
    "architectures": ["arm64", "x86_64"],
    "signature": "ad-hoc",
    "apple_notarized": False,
    "corresponding_source": {
        "file": source_archive.name,
        "sha256": hashlib.sha256(source_archive.read_bytes()).hexdigest(),
        "release_url": f'https://github.com/mitchaiet/deep-fry/releases/tag/v{source_manifest["version"]}',
    },
    "juce": source_manifest["juce"],
    "files_sha256": {
        path.relative_to(package).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted(package.rglob("*")) if path.is_file()
    },
}
(package / "RELEASE-MANIFEST.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
PY

/usr/bin/ditto -c -k --keepParent --norsrc --noextattr "$package_dir" "$stage_dir/$package_name.zip"
(
    cd "$stage_dir"
    /usr/bin/shasum -a 256 "$package_name.zip" > "$package_name.zip.sha256"
)
/bin/mv -f "$stage_dir/$package_name.zip" "$output_dir/$package_name.zip"
/bin/mv -f "$stage_dir/$package_name.zip.sha256" "$output_dir/$package_name.zip.sha256"
/bin/mv -f "$stage_dir/Deep-Fry-$version-source.tar.gz" "$output_dir/Deep-Fry-$version-source.tar.gz"
/bin/mv -f "$stage_dir/Deep-Fry-$version-source.tar.gz.sha256" "$output_dir/Deep-Fry-$version-source.tar.gz.sha256"
echo "Created $output_dir/$package_name.zip"
echo "Created $output_dir/$package_name.zip.sha256"
echo "Created $output_dir/Deep-Fry-$version-source.tar.gz"
echo "Created $output_dir/Deep-Fry-$version-source.tar.gz.sha256"
