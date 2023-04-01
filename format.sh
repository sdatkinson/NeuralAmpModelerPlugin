#!/bin/bash
# Apply project formatting (i.e. clang-format with LLVM style)
#
# Usage:
# $ bash format.bash

echo "Formatting..."

git ls-files "*.h" "*.cpp" | xargs clang-format -i .

echo "Formatting complete!"
echo "You can stage all of the files using:"
echo ""
echo '  git ls-files "*.h" "*.cpp" | xargs git add'
echo ""