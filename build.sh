#!/bin/sh
# Compile the C++ chess engine to WebAssembly.
# Needs clang++ with the wasm32 target + wasm-ld (LLVM 16+). No Emscripten.
set -e
clang++ --target=wasm32 -O2 -std=c++17 -ffreestanding -nostdlib -fno-exceptions -fno-rtti \
        -Wl,--no-entry -Wl,--export-dynamic -Wl,-z,stack-size=1048576 \
        -o chess.wasm chess.cpp
echo "built chess.wasm ($(wc -c < chess.wasm) bytes)"
