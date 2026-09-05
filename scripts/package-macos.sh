#!/usr/bin/env bash
# Package an existing Release build. This script never builds or installs it.
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
artifacts="$project_dir/build/DeepFry_artefacts/Release"
output_dir="$project_dir/dist"

usage() {
    echo "Usage: $0 [--artifacts DIRECTORY] [--output DIRECTORY]"
    echo "Packages an existing universal macOS Release build into a ZIP and SHA-256 file."
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --artifacts|--output)
            [[ $# -ge 2 && -n "$2" ]] || { usage >&2; exit 2; }
            if [[ "$1" == --artifacts ]]; then artifacts="$2"; else output_dir="$2"; fi
            shift 2
            ;;
        -h|--help) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
done

[[ "$(uname -s)" == Darwin ]] || { echo "Packaging requires macOS." >&2; exit 1; }
version="$(sed -nE 's/^project\(DeepFry VERSION ([0-9]+\.[0-9]+\.[0-9]+) .*/\1/p' "$project_dir/CMakeLists.txt")"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
    echo "Cannot read the DeepFry version from CMakeLists.txt." >&2
    exit 1
}

license_dir="$project_dir/packaging/LICENSES"
[[ -d "$license_dir" && -n "$(find "$license_dir" -type f -print -quit)" ]] || {
    echo "Missing distribution license notices: $license_dir" >&2
    exit 1
}
for required in Install.command README.txt; do
    [[ -f "$project_dir/packaging/$required" ]] || { echo "Missing packaging/$required" >&2; exit 1; }
done

bundle_paths=("VST3/Deep Fry.vst3" "AU/Deep Fry.component" "Standalone/Deep Fry.app")
verify_bundle() {
    local bundle="$1" executable_name executable arch minimum_versions bundle_version
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
    [[ "$minimum_versions" == $'26.2\n26.2' ]] || {
        echo "Expected minimum macOS 26.2 in both architectures: $bundle" >&2
        echo "Found minimum versions: $minimum_versions" >&2
        echo "Update the packaging README and installer if the supported OS changes." >&2
        return 1
    }
    /usr/bin/codesign --verify --deep --strict "$bundle"
}

for relative in "${bundle_paths[@]}"; do verify_bundle "$artifacts/$relative"; done

mkdir -p "$output_dir"
output_dir="$(cd "$output_dir" && pwd)"
package_name="Deep-Fry-$version-macOS-universal"
stage_dir="$(mktemp -d "$output_dir/.deep-fry-package.XXXXXX")"
trap 'rm -rf "$stage_dir"' EXIT
package_dir="$stage_dir/$package_name"
mkdir -p "$package_dir"

for relative in "${bundle_paths[@]}"; do
    mkdir -p "$package_dir/$(dirname "$relative")"
    /usr/bin/ditto --norsrc --noextattr "$artifacts/$relative" "$package_dir/$relative"
done
/usr/bin/ditto --norsrc --noextattr "$license_dir" "$package_dir/LICENSES"
/bin/cp "$project_dir/packaging/Install.command" "$package_dir/Install.command"
/bin/chmod 755 "$package_dir/Install.command"
sed "s/@VERSION@/$version/g" "$project_dir/packaging/README.txt" > "$package_dir/README.txt"
find "$package_dir" -name .DS_Store -type f -delete
for relative in "${bundle_paths[@]}"; do verify_bundle "$package_dir/$relative"; done

/usr/bin/ditto -c -k --keepParent --norsrc --noextattr "$package_dir" "$stage_dir/$package_name.zip"
(
    cd "$stage_dir"
    /usr/bin/shasum -a 256 "$package_name.zip" > "$package_name.zip.sha256"
)
/bin/mv -f "$stage_dir/$package_name.zip" "$output_dir/$package_name.zip"
/bin/mv -f "$stage_dir/$package_name.zip.sha256" "$output_dir/$package_name.zip.sha256"
echo "Created $output_dir/$package_name.zip"
echo "Created $output_dir/$package_name.zip.sha256"
