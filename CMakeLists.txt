cmake_minimum_required(VERSION 3.15)

project(Secrecy VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-O3 -g -pthread")

add_compile_options(-Wno-deprecated-declarations)

if (DEFINED PROTOCOL)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D PROTOCOL_NUM=${PROTOCOL}")
endif (DEFINED PROTOCOL)

# This is necessary for perf & flamegraph
if (PROFILE)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -pg")
endif (PROFILE)

# For address sanitizer and other performance testing
if (PERF)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
endif (PERF)


if (NOT NO_X86_SSE)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mbmi -march=native")
endif (NOT NO_X86_SSE)

if (EXTRA)
    string(REPLACE "," " " EXTRA "${EXTRA}")
    string(STRIP EXTRA "${EXTRA}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EXTRA}")
endif (EXTRA)

set(VALID_COMMUNICATORS "MPI" "NOCOPY")
if (DEFINED COMM)
    if (COMM IN_LIST VALID_COMMUNICATORS)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCOMMUNICATOR_NUM=${COMM}_COMMUNICATOR")
    else()
        message(FATAL_ERROR "Unknown communicator type specified: ${COMM}")
    endif()
endif(DEFINED COMM)

if (COMM_THREADS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D NOCOPY_COMMUNICATOR_THREADS=${COMM_THREADS}")
endif (COMM_THREADS)

set(SINGLE 0 CACHE STRING "whether this is a communicator (0) or single (1) execution")
if(SINGLE EQUAL 1)
    add_definitions(-DSINGLE_EXECUTION)
else()
    remove_definitions(-DSINGLE_EXECUTION)
endif()

# Define the TRIPLES option, with DUMMY as the default value
# For benchmarks, use ZERO.
set(TRIPLES "DUMMY" CACHE STRING "Set type of triples to use (REAL, DUMMY, ZERO)")
set(LINK_LIBOTE 0)
if(TRIPLES STREQUAL "DUMMY")
    add_compile_definitions(USE_DUMMY_TRIPLES)
elseif(TRIPLES STREQUAL "ZERO")
    add_compile_definitions(USE_ZERO_TRIPLES)
elseif(NOT TRIPLES STREQUAL "REAL")
    message(FATAL_ERROR "Invalid value for TRIPLES: ${TRIPLES}. Choose REAL, DUMMY, or ZERO.")
else()
    add_compile_definitions(USE_LIBOTE)
    set(LINK_LIBOTE 1)
endif()

set(DEFAULT_BITWIDTH 32 CACHE STRING "Set default bitwidth")
add_definitions(-DDEFAULT_BITWIDTH=${DEFAULT_BITWIDTH})


message("Compiler flags: ${CMAKE_CXX_FLAGS}")

find_package(Threads REQUIRED)

# Adding Dependencies
# add_subdirectory(src/external-lib/sql-parser)

# Adding MPI Library
find_package(MPI REQUIRED)
if (MPI_FOUND)
    include_directories(SYSTEM ${MPI_INCLUDE_PATH})
else (MPI_FOUND)
    message(SEND_ERROR "This application cannot compile without MPI")
endif (MPI_FOUND)

# Using libsodium
find_path(SODIUM_INCLUDE_DIR sodium.h HINTS ${LIBSODIUM_INCLUDE_DIRS} /usr/local/include /opt/local/include /opt/include)
find_library(SODIUM_LIBRARY NAMES sodium HINTS ${LIBSODIUM_LIBRARY_DIRS} /usr/local/lib /opt/local/lib /opt/lib)

# SQLite for TPCH query tests
find_package(SQLite3)

# Base list of link libraries
set(LINK_LIBRARIES "${MPI_LIBRARIES}" "${SODIUM_LIBRARY}")

# Add libOTe and SecureJoin for 2pc only
if (PROTOCOL EQUAL "2" AND LINK_LIBOTE)
    find_package(libOTe REQUIRED HINTS
        libOTe-install/lib/cmake/libOTe/
        libOTe-install/lib64/cmake/libOTe/
        build/libOTe-install/lib/cmake/libOTe/
        build/libOTe-install/lib64/cmake/libOTe/)
    list(APPEND LINK_LIBRARIES "oc::libOTe")
    
    find_package(secureJoin REQUIRED HINTS
        secure-join-install/lib/cmake/secureJoin/
        build/secure-join-install/lib/cmake/secureJoin/)
    link_directories(${CMAKE_CURRENT_SOURCE_DIR}/build/secure-join-install/lib)
endif ()

function(add_secure_join target_name)
  if(PROTOCOL EQUAL "2" AND LINK_LIBOTE)
    target_link_libraries(${target_name} secureJoin)
    target_include_directories(${target_name} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/build/secure-join-install/include/secureJoin)
  endif()
endfunction()

if (PROTOCOL EQUAL "4")
    add_library(print_4pc_info include/core/protocols/_4pc_selector.cpp)
else()
    # nop
    add_custom_target(print_4pc_info)
endif (PROTOCOL EQUAL "4")

# Option: switch between optimized 4PC and original Dalskov et al. protocol
option(USE_DALSKOV_FANTASTIC_FOUR
       "Build with the original Dalskov et al. 4-party protocol (undef = optimized)"
       OFF)

if(USE_DALSKOV_FANTASTIC_FOUR)
    add_compile_definitions(USE_DALSKOV_FANTASTIC_FOUR)
endif()

function(configure_target _target)
    set(options LINK_SQL)
    cmake_parse_arguments(ARG "${options}" "" "" ${ARGN})

    get_filename_component(fileName ${_target} NAME_WLE)
    add_executable(${fileName} ${_target})
    add_dependencies(${fileName} print_4pc_info)

    add_secure_join(${fileName})
    
    if (ARG_LINK_SQL)
        target_link_libraries(${fileName} ${LINK_LIBRARIES} ${SQLite3_LIBRARIES})
    else()
        target_link_libraries(${fileName} ${LINK_LIBRARIES})
    endif()

    target_include_directories(${fileName} PUBLIC ${SODIUM_INCLUDE_DIR})
endfunction()

function(aggregate_target agg_name sources)
    add_custom_target(${agg_name} DEPENDS ${${sources}})
    foreach(test_source IN LISTS ${sources})
        get_filename_component(test_name ${test_source} NAME_WE)
        add_dependencies(${agg_name} ${test_name})
    endforeach()
endfunction()


######################################################
##################### EXECUTABLES ####################
######################################################
file(GLOB EXE_EXAMPLES src/examples/*.cpp)
file(GLOB EXE_MICRO src/micro/*.cpp)
file(GLOB EXE_QUERIES src/queries/*.cpp)
file(GLOB EXE_TESTS tests/*.cpp)

file(GLOB TPCH_QUERIES src/queries/tpch/q*.cpp)
file(GLOB SECRETFLOW_QUERIES src/queries/secretflow/*.cpp)
file(GLOB OTHER_QUERIES src/queries/2024-paper/*.cpp)

foreach (_target
            ${EXE_EXAMPLES} ${EXE_MICRO} ${EXE_QUERIES} ${EXE_TESTS})
    configure_target(${_target})
endforeach ()

foreach (_target ${TPCH_QUERIES} ${SECRETFLOW_QUERIES} ${OTHER_QUERIES})
    configure_target(${_target} LINK_SQL)
endforeach()

aggregate_target(tests-only EXE_TESTS)
aggregate_target(tpch-queries TPCH_QUERIES)
aggregate_target(secretflow-queries SECRETFLOW_QUERIES)
aggregate_target(other-queries OTHER_QUERIES)
