#!/bin/sh
# Install / uninstall GoatEdit from a build tree (Linux) - docs/install-layout.md
#
#   scripts/install.sh --user                  # ~/.local   (only me)
#   scripts/install.sh --system                # /usr/local (all users, uses sudo)
#   scripts/install.sh --prefix <dir>          # anywhere else
#   scripts/install.sh --uninstall --user      # remove exactly what the install wrote
#   --build-dir <dir>                          # default: bld/, else cmake-build-release/
#
# The editor finds its assets relative to the binary (<prefix>/bin -> <prefix>/share/goatedit), so any
# prefix works without wrappers or environment variables. The install's file list is kept as
# <prefix>/share/goatedit/install_manifest.txt; --uninstall removes those files and nothing else -
# user config (~/.config/goatedit) and state (~/.local/state/goatedit) are never touched.
set -eu

die() { echo "install.sh: $*" >&2; exit 1; }

if [ "$(uname -s)" = "Darwin" ]; then
    die "macOS installs the app bundle: cmake --install <build-dir>  (lands in /Applications)"
fi

repoRoot=$(cd "$(dirname "$0")/.." && pwd)
prefix=""
buildDir=""
uninstall=0

while [ $# -gt 0 ]; do
    case "$1" in
        --user)      prefix="$HOME/.local" ;;
        --system)    prefix="/usr/local" ;;
        --prefix)    [ $# -ge 2 ] || die "--prefix needs a directory"; prefix="$2"; shift ;;
        --build-dir) [ $# -ge 2 ] || die "--build-dir needs a directory"; buildDir="$2"; shift ;;
        --uninstall) uninstall=1 ;;
        -h|--help)   sed -n '2,13p' "$0"; exit 0 ;;
        *)           die "unknown option '$1' (try --help)" ;;
    esac
    shift
done
[ -n "$prefix" ] || die "pick one of --user, --system or --prefix <dir>"
case "$prefix" in
    /*) ;;
    *)  prefix="$(pwd)/$prefix" ;;
esac

# sudo only when the prefix isn't ours to write
sudo=""
probe="$prefix"
while [ ! -e "$probe" ]; do probe=$(dirname "$probe"); done
if [ ! -w "$probe" ]; then
    sudo="sudo"
fi

assetDir="$prefix/share/goatedit"
manifest="$assetDir/install_manifest.txt"

if [ "$uninstall" -eq 1 ]; then
    [ -f "$manifest" ] || die "no install manifest at '$manifest' - nothing installed there by this script"
    echo "Removing GoatEdit from $prefix"
    # '|| [ -n ]' keeps a last line that has no newline (older manifests)
    while IFS= read -r file || [ -n "$file" ]; do
        # Only absolute paths inside the prefix - a damaged manifest must not reach elsewhere
        case "$file" in
            "$prefix"/*) $sudo rm -f "$file" ;;
            "") ;;
            *) echo "  skipping '$file' (outside $prefix)" >&2 ;;
        esac
    done < "$manifest"
    $sudo rm -f "$manifest"
    # Directories the asset tree leaves behind; shared dirs (bin/, applications/, icons/) stay
    if [ -d "$assetDir" ]; then
        $sudo find "$assetDir" -depth -type d -empty -delete
    fi
    echo "Done. User config and state were left alone."
    exit 0
fi

if [ -z "$buildDir" ]; then
    for candidate in "$repoRoot/bld" "$repoRoot/cmake-build-release"; do
        if [ -f "$candidate/CMakeCache.txt" ]; then
            buildDir="$candidate"
            break
        fi
    done
fi
[ -n "$buildDir" ] && [ -f "$buildDir/CMakeCache.txt" ] || die "no configured build dir - pass --build-dir"
[ -x "$buildDir/goatedit" ] || die "'$buildDir/goatedit' is not built - cmake --build $buildDir --target goatedit"

buildType=$(sed -n 's/^CMAKE_BUILD_TYPE:STRING=//p' "$buildDir/CMakeCache.txt")
if [ "$buildType" != "Release" ]; then
    echo "warning: '$buildDir' is a '${buildType:-unset}' build, not Release" >&2
fi

# A previous install's files that this one no longer ships would otherwise linger
if [ -f "$manifest" ]; then
    echo "Replacing the existing install in $prefix"
    "$0" --uninstall --prefix "$prefix"
fi

echo "Installing $buildDir -> $prefix"
$sudo cmake --install "$buildDir" --prefix "$prefix"
# CMake writes its manifest without a trailing newline - end that line before appending ours
{
    cat "$buildDir/install_manifest.txt"
    echo
    echo "$manifest"
} | grep -v '^$' | $sudo tee "$manifest" >/dev/null

case ":$PATH:" in
    *":$prefix/bin:"*) ;;
    *) echo "note: $prefix/bin is not on PATH (on Ubuntu ~/.local/bin is added at login once it exists)" ;;
esac
echo "Done: $prefix/bin/goatedit"
