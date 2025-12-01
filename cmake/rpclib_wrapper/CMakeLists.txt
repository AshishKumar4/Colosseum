cmake_minimum_required(VERSION 3.5.0)
project(rpclib_wrapper)

# Fix for glibc 2.38+ compatibility (UE 5.6) - MUST be before any other setup
IF(UNIX AND NOT APPLE)
    set(CMAKE_C_FLAGS "-D__GLIBC_USE_C2X_STRTOL=0 ${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "-D__GLIBC_USE_C2X_STRTOL=0 ${CMAKE_CXX_FLAGS}")
ENDIF()

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../cmake-modules")
INCLUDE("${CMAKE_CURRENT_LIST_DIR}/../cmake-modules/CommonSetup.cmake")
CommonSetup()

IF(UNIX)
    set(RPC_LIB_DEFINES " -D MSGPACK_PP_VARIADICS_MSVC=0")
    set(CMAKE_CXX_FLAGS "${RPC_LIB_DEFINES} ${CMAKE_CXX_FLAGS}")
ENDIF()

add_subdirectory("${AIRSIM_ROOT}/external/rpclib/${RPCLIB_VERSION_FOLDER}" "${CMAKE_CURRENT_BINARY_DIR}/rpclib")