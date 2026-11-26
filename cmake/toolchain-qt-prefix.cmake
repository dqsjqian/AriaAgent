# Minimal toolchain shim: make the Qt install prefix visible to every
# configure in this build, including ForgeDeps' nested Aria SDK transaction.
#
# Why this exists: the nested Aria SDK configure gets a protocol-controlled
# CMAKE_PREFIX_PATH (<json>;<mira>;<openssl>) from ForgeDeps, so its
# find_package(Qt6 Gui) -> FindOpenGL cannot see Qt's libGL.so, even though
# the top-level configure finds Qt fine (scripts/build.py passes
# -DCMAKE_PREFIX_PATH=<qt> there). ForgeDeps replays CMAKE_TOOLCHAIN_FILE
# into the nested configure, so this file is the one place that can add the
# Qt prefix to the nested search path without touching ForgeDeps itself
# (whose protocol deliberately forbids injecting CMAKE_PREFIX_PATH).
# The prefix comes from the ARIAAGENT_QT_PREFIX environment variable, set by
# scripts/build.py when it resolves --qt-dir / QT_DIR / auto-detection.
# No-op when the variable is unset, e.g. a plain cmake invocation that does
# not go through build.py.
if(DEFINED ENV{ARIAAGENT_QT_PREFIX} AND NOT "$ENV{ARIAAGENT_QT_PREFIX}" STREQUAL "")
    list(APPEND CMAKE_PREFIX_PATH "$ENV{ARIAAGENT_QT_PREFIX}")
    list(REMOVE_DUPLICATES CMAKE_PREFIX_PATH)
    message(STATUS "AriaAgent: Qt prefix $ENV{ARIAAGENT_QT_PREFIX} added to CMAKE_PREFIX_PATH (toolchain shim)")
else()
    message(STATUS "AriaAgent: ARIAAGENT_QT_PREFIX unset; Qt search prefix not added by toolchain shim")
endif()
