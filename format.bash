#!/bin/bash
# Apply project formatting (i.e. clang-format with LLVM style)
#
# Usage:
# $ bash format.bash

git ls-files "*.h" "*.cpp" | xargs clang-format --style=llvm -i
