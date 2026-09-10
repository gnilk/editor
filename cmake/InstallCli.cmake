#
# GoatEdit - InstallCli.cmake
#
# Copies the generated shell wrapper into a directory on PATH. Driven by the
# `install-cli` target (cmake -P script mode), deliberately NOT hooked into
# `cmake --install` / CPack — those stage into a temporary prefix, and this
# writes to an absolute system path.
#
# Expects on the command line: WRAPPER, BIN_DIR, CLI_NAME, APP_BINARY
#
# Never escalates privileges by itself: if the destination isn't writable we
# print the exact command to run and stop (see docs/cli-install-design.md).
#

set(cliTarget "${BIN_DIR}/${CLI_NAME}")

if (NOT EXISTS "${APP_BINARY}")
    message(WARNING
            "Bundle executable not found:\n"
            "    ${APP_BINARY}\n"
            "  Installing the wrapper anyway - it won't work until the .app is in place.\n"
            "  Build the .dmg (target 'package'), drag GoatEdit to /Applications, and rerun.")
endif()

# Create the destination if it's missing - the default (~/.local/bin) may simply not exist yet on a
# fresh box. A failure here isn't fatal on its own; the copy below reports it.
execute_process(
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BIN_DIR}"
        RESULT_VARIABLE mkdirResult
        ERROR_QUIET
)

# Attempt the copy - a failure covers both 'directory missing' and 'permission
# denied' without probing for writability separately.
execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy "${WRAPPER}" "${cliTarget}"
        RESULT_VARIABLE copyResult
        ERROR_QUIET
)

if (NOT copyResult EQUAL 0)
    message(FATAL_ERROR
            "Cannot write ${cliTarget} (not writable by you).\n"
            "  Install it yourself with:\n"
            "      sudo install -m 755 '${WRAPPER}' '${cliTarget}'\n"
            "  ..or point the target somewhere you own:\n"
            "      cmake -B <builddir> -DGEDIT_CLI_BIN_DIR=<dir>")
endif()

file(CHMOD "${cliTarget}"
        PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
)

message(STATUS "Installed CLI wrapper: ${cliTarget}")
message(STATUS "                   -> ${APP_BINARY}")
