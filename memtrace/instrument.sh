#!/bin/bash
INPUT=$1
CLANG=/opt/homebrew/opt/llvm@18/bin/clang
OPT=/opt/homebrew/opt/llvm@18/bin/opt
PASS=/Users/szoumo/Documents/sys/memtrace/build/pass/libMemTracePass.dylib
RT=/Users/szoumo/Documents/sys/memtrace/build/runtime/libmemtrace_rt.a

$CLANG -S -emit-llvm -O1 -I$(dirname $PASS)/../runtime $INPUT -o /tmp/_mt.ll
$OPT -load-pass-plugin $PASS -passes=memtrace -S -o /tmp/_mt_inst.ll /tmp/_mt.ll
$CLANG++ /tmp/_mt_inst.ll $RT -o /tmp/test_bin
/tmp/test_bin