#
# Mirror the asset sources into the build's asset dir (EDITOR_ASSET_DIR): copy what changed, and DELETE what the
# sources no longer have - a plain copy only adds, so an asset removed from the repo lived on in every build and
# install (open-bugs #22).
#
#   cmake -DRESOURCES_DIR=<Assets/Resources> -DPLUGINS_DIR=<src/Plugins> -DDEST_DIR=<asset dir> -P MirrorAssets.cmake
#
# The plugins land in <asset dir>/Plugins.
#
# A -P script sets no policies of its own: without this, CMake 3.x runs it with CMP0057 OLD and `IN_LIST` is an error
# (CMake 4 has no OLD for it, so the Mac never saw it).
cmake_minimum_required(VERSION 3.28)

foreach(var RESOURCES_DIR PLUGINS_DIR DEST_DIR)
    if (NOT DEFINED ${var})
        message(FATAL_ERROR "MirrorAssets: ${var} not set")
    endif()
endforeach()

set(keepFiles "")

function(mirror_tree srcDir dstDir)
    file(GLOB_RECURSE srcFiles RELATIVE "${srcDir}" "${srcDir}/*")
    foreach(relPath IN LISTS srcFiles)
        get_filename_component(dstParent "${dstDir}/${relPath}" DIRECTORY)
        file(MAKE_DIRECTORY "${dstParent}")
        file(COPY_FILE "${srcDir}/${relPath}" "${dstDir}/${relPath}" ONLY_IF_DIFFERENT)
        list(APPEND keepFiles "${dstDir}/${relPath}")
    endforeach()
    set(keepFiles "${keepFiles}" PARENT_SCOPE)
endfunction()

mirror_tree("${RESOURCES_DIR}" "${DEST_DIR}")
mirror_tree("${PLUGINS_DIR}" "${DEST_DIR}/Plugins")

file(GLOB_RECURSE dstFiles "${DEST_DIR}/*")
foreach(dstFile IN LISTS dstFiles)
    if (NOT dstFile IN_LIST keepFiles)
        message(STATUS "Removing stale asset: ${dstFile}")
        file(REMOVE "${dstFile}")
    endif()
endforeach()
