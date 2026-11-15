#/bin/bash
echo "Using cmake to generate  Xcode unit test project in ../build-mac ..."
mkdir -p ../build-mac
cmake  -G Xcode -S . -B ../build-mac
