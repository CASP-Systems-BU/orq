# SecureJoin calls find_package(libOTe) and then links the target name libOTe.
# That target exists when SecureJoin builds libOTe with add_subdirectory. An
# install from scripts/_setup_libote.sh exports oc::libOTe instead, and that
# imported target does not carry cryptoTools' headers. Point this search at
# that install and publish the target names SecureJoin already uses.
#
# scripts/_setup_securejoin.sh passes -DCMAKE_MODULE_PATH to this directory
# and -DLIBOTE_INSTALL_PREFIX to the install root. --noauto makes SecureJoin
# take this find_package path instead of compiling libOTe again.

if(NOT LIBOTE_INSTALL_PREFIX)
    message(FATAL_ERROR "LIBOTE_INSTALL_PREFIX is not set")
endif()

message(STATUS "Using installed libOTe at ${LIBOTE_INSTALL_PREFIX}")

# The caller asked for components (no_asan, sse, ...). The installed config
# does not mark those components found. A nested find_package of this same
# name inherits them while libOTe_FIND_MODULE is on, and then rejects the
# install. Remember the names, and turn that inheritance off for the search.
set(_orq_libote_components ${libOTe_FIND_COMPONENTS})
set(libOTe_FIND_MODULE OFF)

find_package(libOTe CONFIG REQUIRED HINTS "${LIBOTE_INSTALL_PREFIX}")
find_package(cryptoTools CONFIG REQUIRED HINTS "${LIBOTE_INSTALL_PREFIX}")

if(NOT TARGET libOTe)
    add_library(libOTe INTERFACE)
    target_link_libraries(libOTe INTERFACE oc::libOTe oc::cryptoTools)
endif()

if(TARGET oc::tests_cryptoTools AND NOT TARGET tests_cryptoTools)
    add_library(tests_cryptoTools INTERFACE)
    target_link_libraries(tests_cryptoTools INTERFACE oc::tests_cryptoTools)
endif()

foreach(comp IN LISTS _orq_libote_components)
    set(libOTe_${comp}_FOUND TRUE)
endforeach()
set(libOTe_FOUND TRUE)
