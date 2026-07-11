cmake_minimum_required(VERSION 3.20)
project(MemTrace)

find_package(LLVM REQUIRED CONFIG
    PATHS /opt/homebrew/opt/llvm@18/lib/cmake/llvm
    NO_DEFAULT_PATH)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

add_subdirectory(pass)
add_subdirectory(runtime)