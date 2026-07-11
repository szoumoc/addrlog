add_library(memtrace_rt STATIC
    memtrace_rt.cpp)

target_compile_features(memtrace_rt PRIVATE cxx_std_20)