#!/usr/bin/env bash
# Build a complete release source archive without fetching JUCE from the network.
set -euo pipefail
project_dir="$(cd "$(dirname "$0")/.." && pwd)"
juce_version="$(sed -nE 's/^# JUCE ([0-9]+\.[0-9]+\.[0-9]+),.*/\1/p' "$project_dir/CMakeLists.txt")"
juce_commit="$(sed -nE 's|^[[:space:]]*URL https://codeload.github.com/juce-framework/JUCE/tar.gz/([a-f0-9]{40})$|\1|p' "$project_dir/CMakeLists.txt")"
expected_sha="$(sed -nE 's/^[[:space:]]*URL_HASH SHA256=([a-f0-9]{64})$/\1/p' "$project_dir/CMakeLists.txt")"
archive="$project_dir/vendor/juce-$juce_version.tar.gz"
[[ -f "$archive" ]] || {
    echo "Missing vendored JUCE archive. Extract the complete Deep-Fry-*-source.tar.gz release asset." >&2
    echo "For a Git checkout with network access, use ./scripts/build.sh instead." >&2
    exit 1
}
actual_sha="$(shasum -a 256 "$archive" | awk '{ print $1 }')"
[[ "$expected_sha" =~ ^[a-f0-9]{64}$ && "$actual_sha" == "$expected_sha" ]] || {
    echo "Vendored JUCE archive does not match the CMake SHA-256 pin." >&2
    exit 1
}
dependency_root="$project_dir/build/offline-dependencies"
mkdir -p "$dependency_root"
dependency_stage="$(mktemp -d "$dependency_root/.extract.XXXXXX")"
trap 'rm -rf "$dependency_stage"' EXIT
tar -xzf "$archive" -C "$dependency_stage"
[[ -f "$dependency_stage/JUCE-$juce_commit/CMakeLists.txt" ]] || {
    echo "The JUCE archive has an unexpected layout." >&2
    exit 1
}
dependency_dir="$dependency_root/JUCE-$juce_commit"
# Re-extract from the verified archive for each invocation, avoiding a modified
# dependency tree being silently reused by a subsequent release build.
rm -rf "$dependency_dir"
mv "$dependency_stage/JUCE-$juce_commit" "$dependency_dir"
"$project_dir/scripts/build.sh" "$@" \
    -DFETCHCONTENT_SOURCE_DIR_JUCE="$dependency_dir" \
    -DFETCHCONTENT_FULLY_DISCONNECTED=ON
