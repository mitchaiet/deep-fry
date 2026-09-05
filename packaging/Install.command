#!/usr/bin/env bash
# Finder-friendly installer; also supports isolated verification with --prefix.
set -euo pipefail

package_dir="$(cd "$(dirname "$0")" && pwd)"
install_prefix="${HOME:?HOME must be set}"
interactive=1

usage() {
    echo "Usage: $0 [--non-interactive] [--prefix DIRECTORY]"
    echo "Installs Deep Fry VST3, Audio Unit, and standalone app for the current user."
    echo "--prefix uses another existing absolute directory in place of your home folder."
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --non-interactive) interactive=0; shift ;;
        --prefix)
            [[ $# -ge 2 && -n "$2" ]] || { usage >&2; exit 2; }
            install_prefix="$2"
            shift 2
            ;;
        -h|--help) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
done

[[ "$(uname -s)" == Darwin ]] || { echo "This installer requires macOS." >&2; exit 1; }
[[ "$install_prefix" == /* && "$install_prefix" != / && -d "$install_prefix" ]] || {
    echo "Installation prefix must be an existing absolute directory other than /." >&2
    exit 1
}
install_prefix="$(cd "$install_prefix" && pwd -P)"
IFS=. read -r os_major os_minor os_patch < <(/usr/bin/sw_vers -productVersion)
if (( os_major < 26 || (os_major == 26 && ${os_minor:-0} < 2) )); then
    echo "This build requires macOS 26.2 or later." >&2
    exit 1
fi

sources=("$package_dir/VST3/Deep Fry.vst3" "$package_dir/AU/Deep Fry.component" "$package_dir/Standalone/Deep Fry.app")
parents=("$install_prefix/Library/Audio/Plug-Ins/VST3" "$install_prefix/Library/Audio/Plug-Ins/Components" "$install_prefix/Applications")
names=("Deep Fry.vst3" "Deep Fry.component" "Deep Fry.app")
formats=("VST3" "AU" "Standalone")
stages=("" "" "")
replacement_started=(0 0 0)
support_dir="$install_prefix/Library/Application Support/Deep Fry"
backup_dir=""
lock_created=0
committed=0

finish() {
    local result=$? i target rollback_failed=0
    trap - EXIT INT TERM HUP
    set +e
    if [[ "$committed" == 0 ]]; then
        for i in 2 1 0; do
            target="${parents[$i]}/${names[$i]}"
            # Inspect the rename results as well as the transaction state, so
            # an interrupt immediately after mv still restores the old bundle.
            if [[ "${replacement_started[$i]}" == 1 && ! -e "${stages[$i]}/new" && -e "$target" ]]; then
                if ! /bin/mv "$target" "${stages[$i]}/failed-new"; then
                    echo "Could not remove the new installation at $target." >&2
                    rollback_failed=1
                fi
            fi
            if [[ -n "${stages[$i]}" && -e "${stages[$i]}/previous" ]]; then
                if [[ -e "$target" || -L "$target" ]] || ! /bin/mv "${stages[$i]}/previous" "$target"; then
                    echo "Previous bundle needs manual recovery: ${stages[$i]}/previous" >&2
                    rollback_failed=1
                fi
            fi
        done
        if [[ "$result" != 0 ]]; then
            echo "Installation failed. Existing installations were restored where possible." >&2
            [[ -z "$backup_dir" ]] || echo "Preserved backups: $backup_dir" >&2
        fi
    fi
    for i in 0 1 2; do
        if [[ -n "${stages[$i]}" && ! -e "${stages[$i]}/previous" ]]; then
            /bin/rm -rf "${stages[$i]}"
        elif [[ "$committed" == 1 && -n "${stages[$i]}" ]]; then
            /bin/rm -rf "${stages[$i]}"
        fi
    done
    if [[ "$lock_created" == 1 ]]; then /bin/rmdir "$support_dir/.install-lock"; fi
    if [[ "$rollback_failed" == 1 ]]; then result=1; fi
    if [[ "$interactive" == 1 && -t 0 ]]; then
        echo
        read -r -p "Press Return to close this installer." ignored || true
    fi
    exit "$result"
}
trap finish EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'exit 129' HUP

for i in 0 1 2; do
    [[ -d "${sources[$i]}" ]] || { echo "Missing ${sources[$i]}. Extract the complete ZIP before installing." >&2; exit 1; }
    /usr/bin/codesign --verify --deep --strict "${sources[$i]}"
done

mkdir -p "$support_dir"
if ! mkdir "$support_dir/.install-lock" 2>/dev/null; then
    echo "Another installer is running, or a previous one was interrupted." >&2
    echo "If no installer is running, remove this empty lock folder and retry: $support_dir/.install-lock" >&2
    exit 1
fi
lock_created=1

# Copy and validate every new bundle before changing an installed bundle.
for i in 0 1 2; do
    mkdir -p "${parents[$i]}"
    target="${parents[$i]}/${names[$i]}"
    if [[ -L "$target" || ( -e "$target" && ! -d "$target" ) ]]; then
        echo "Refusing to replace a symlink or non-bundle file: $target" >&2
        exit 1
    fi
    stages[$i]="$(mktemp -d "${parents[$i]}/.deep-fry-install.XXXXXX")"
    /usr/bin/ditto "${sources[$i]}" "${stages[$i]}/new"
    /usr/bin/codesign --verify --deep --strict "${stages[$i]}/new"
    if [[ -d "$target" ]]; then
        if [[ -z "$backup_dir" ]]; then
            mkdir -p "$support_dir/Backups"
            backup_dir="$(mktemp -d "$support_dir/Backups/$(date '+%Y%m%d-%H%M%S').XXXXXX")"
        fi
        mkdir -p "$backup_dir/${formats[$i]}"
        /usr/bin/ditto "$target" "$backup_dir/${formats[$i]}/${names[$i]}"
    fi
done

# Renames stay on each destination filesystem; a failure rolls back all formats.
for i in 0 1 2; do
    target="${parents[$i]}/${names[$i]}"
    replacement_started[$i]=1
    if [[ -d "$target" ]]; then
        /bin/mv "$target" "${stages[$i]}/previous"
    fi
    /bin/mv "${stages[$i]}/new" "$target"
done
committed=1

echo
echo "Installed Deep Fry:"
for i in 0 1 2; do echo "  ${parents[$i]}/${names[$i]}"; done
[[ -z "$backup_dir" ]] || echo "Previous versions are saved in: $backup_dir"
echo
echo "Save your session, restart your DAW, and rescan plug-ins."
echo "In Ableton Live 11: Preferences > Plug-Ins > Use VST3 Plug-In System Folders: On."
echo "Click Rescan, then find Deep Fry under Plug-Ins in the browser."
echo "This build is locally ad-hoc signed and is not notarized by Apple. See README.txt."
