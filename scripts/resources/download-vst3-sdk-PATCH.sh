#!/usr/bin/env bash
# THIS IS A COPY OF iPlug2/Dependencies/IPlug/downlaod-vst3-sdk.sh
# Line 21 is new.

# 1st argument = tag name
# 2nd argument = "build-validator" to build the vst3-validator

TAG="master"
if [ "$1" != "" ]; then
  TAG=$1
fi

rm -f -r VST3_SDK
git clone https://github.com/steinbergmedia/vst3sdk.git --branch $TAG --single-branch --depth=1 VST3_SDK
cd VST3_SDK
git submodule update --init pluginterfaces
git submodule update --init base
git submodule update --init public.sdk
git submodule update --init cmake
git submodule update --init vstgui4
git submodule update --init doc

if [ "$2" == "build-validator" ]; then
  mkdir VST3_BUILD
  cd VST3_BUILD
  cmake ..
  cmake --build . --target validator -j --config=Release
  if [ -d "./bin/Release" ]; then
    mv ./bin/Release/validator ../validator
  else
    mv ./bin/validator ../validator
  fi
  cd ..
fi

rm -f -r VST3_BUILD
rm -f -r public.sdk/samples
rm -f -r vstgui4
rm -f -r .git*
rm -f -r */.git*
git checkout README.md
