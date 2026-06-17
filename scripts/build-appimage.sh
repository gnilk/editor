#!/usr/bin/env bash
# Build a portable GoatEdit AppImage (CM-6, docs/cmake-cleanup.md §3).
#
# Why AppImage: it bundles SDL + the C++ runtime into a single self-contained file that
# "downloads and runs on any distro" — sidestepping the "is the right SDL installed?"
# problem. The embedded forkpty terminal Just Works because AppImage is NOT sandboxed
# (the shell runs against the host $PATH/toolchain) — the reason Flatpak/Snap are out.
#
# Asset discovery: linuxdeploy's AppRun exports XDG_DATA_DIRS="$APPDIR/usr/share:...",
# and the editor resolves its assets via XDGEnvironment::GetFirstSystemDataPathWithSubDir
# ("goatedit") — so the bundled $APPDIR/usr/share/goatedit/ is found with NO code changes.
#
# Portability rule: build on the OLDEST base you support (an older LTS Ubuntu) so the
# bundled glibc is old enough to run on newer distros. This script just packages whatever
# was built; pick the base in the CI runner image.
#
# Usage:
#   scripts/build-appimage.sh [BUILD_DIR]
# Env:
#   BUILD_DIR   build tree to install from   (default: cmake-build-release)
#   APPDIR      staging AppDir               (default: <BUILD_DIR>/AppDir)
#   OUTPUT_DIR  where the .AppImage lands    (default: repo root)
#   ARCH        target arch for tooling      (default: uname -m)

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-${BUILD_DIR:-${REPO_ROOT}/cmake-build-release}}"
APPDIR="${APPDIR:-${BUILD_DIR}/AppDir}"
OUTPUT_DIR="${OUTPUT_DIR:-${REPO_ROOT}}"
ARCH="${ARCH:-$(uname -m)}"
TOOLS_DIR="${TOOLS_DIR:-${BUILD_DIR}/appimage-tools}"

if [ ! -d "${BUILD_DIR}" ]; then
    echo "error: build dir '${BUILD_DIR}' not found — configure+build goatedit first:" >&2
    echo "  cmake --preset release && cmake --build --preset release" >&2
    exit 1
fi

# Containers/CI runners often lack FUSE; let the AppImage tools self-extract instead.
export APPIMAGE_EXTRACT_AND_RUN="${APPIMAGE_EXTRACT_AND_RUN:-1}"

echo "==> Staging install tree into ${APPDIR}"
rm -rf "${APPDIR}"
# Uses the project install() rules (bin/, share/goatedit, share/applications, share/icons).
DESTDIR="" cmake --install "${BUILD_DIR}" --prefix "${APPDIR}/usr"

# linuxdeploy + appimagetool, fetched once and cached in the build tree.
fetch_tool() {
    local name="$1" url="$2" dest="${TOOLS_DIR}/$1"
    if [ ! -x "${dest}" ]; then
        echo "==> Fetching ${name}" >&2
        mkdir -p "${TOOLS_DIR}"
        curl -fsSL -o "${dest}" "${url}"
        chmod +x "${dest}"
    fi
    echo "${dest}"
}

LINUXDEPLOY="$(fetch_tool "linuxdeploy-${ARCH}.AppImage" \
    "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${ARCH}.AppImage")"

echo "==> Running linuxdeploy"
# VERSION (e.g. the git tag) is folded into the filename when set, so release assets are
# self-describing; otherwise the name is just goatedit-<arch>.AppImage.
VERSION="${VERSION:-}"
if [ -n "${VERSION}" ]; then
    export OUTPUT="${OUTPUT_DIR}/goatedit-${VERSION}-${ARCH}.AppImage"
else
    export OUTPUT="${OUTPUT_DIR}/goatedit-${ARCH}.AppImage"
fi
rm -f "${OUTPUT}"

# Custom AppRun: the default is a bare symlink that exports nothing, so the bundled
# assets under $APPDIR/usr/share/goatedit are never seen by the XDG-based asset loader
# (→ null theme → SIGSEGV on first terminal resize). This wrapper sets XDG_DATA_DIRS.
"${LINUXDEPLOY}" \
    --appdir "${APPDIR}" \
    --executable "${APPDIR}/usr/bin/goatedit" \
    --desktop-file "${APPDIR}/usr/share/applications/com.gnilk.goatedit.desktop" \
    --icon-file "${APPDIR}/usr/share/icons/hicolor/512x512/apps/com.gnilk.goatedit.png" \
    --custom-apprun "${REPO_ROOT}/scripts/appimage-AppRun.sh" \
    --output appimage

echo "==> Done: ${OUTPUT}"
ls -la "${OUTPUT_DIR}"/goatedit*.AppImage
