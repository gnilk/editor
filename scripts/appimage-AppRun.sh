#!/bin/sh
# Custom AppRun for the GoatEdit AppImage (CM-6, docs/cmake-cleanup.md §3).
#
# linuxdeploy's default AppRun is a bare symlink to the binary, which exports NOTHING.
# GoatEdit discovers its assets via XDGEnvironment::GetFirstSystemDataPathWithSubDir
# ("goatedit") — i.e. it scans $XDG_DATA_DIRS for a "goatedit/" subdir. Without this
# wrapper the bundled $APPDIR/usr/share/goatedit/ is never on that list, so config.yml +
# the theme don't load and the first terminal resize dereferences a null theme (SIGSEGV).
#
# Prepending $APPDIR/usr/share makes the bundled assets the first hit while still letting a
# user override via $HOME (kUser search paths win over kSystem in the loader).
HERE="$(dirname "$(readlink -f "${0}")")"
export XDG_DATA_DIRS="${HERE}/usr/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
exec "${HERE}/usr/bin/goatedit" "$@"
