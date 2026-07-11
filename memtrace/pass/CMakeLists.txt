add_library(MemTracePass SHARED
    MemTracePass.cpp)

target_compile_features(MemTracePass PRIVATE cxx_std_17)

target_include_directories(MemTracePass PRIVATE
    ${LLVM_INCLUDE_DIRS})

target_compile_options(MemTracePass PRIVATE
    -fno-rtti)

# Get the libraries we need from LLVM
llvm_map_components_to_libnames(LLVM_LIBS
    Core
    Support
    IRReader
    Passes)

target_link_libraries(MemTracePass PRIVATE
    ${LLVM_LIBS})