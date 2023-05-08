#!/bin/bash
# 
# git submodule update --init iPlug2
# cd scripts
# ./patch-sdk-download.sh

PATCH_SCRIPT_NAME=download-vst3-sdk-PATCH.sh
IPLUG_DIR=../iPlug2/Dependencies/iPlug

cp resources/${PATCH_SCRIPT_NAME} ${IPLUG_DIR}

if [ $(uname -s) == "Darwin" ]; then
    echo "MacOS detected"
    IN_PLACE_FLAG=-i.bak
else
    echo "Linux detected"
    IN_PLACE_FLAG=-i
fi

sed ${IN_PLACE_FLAG} "s/download-vst3-sdk.sh/${PATCH_SCRIPT_NAME}/g" ${IPLUG_DIR}/download-iplug-sdks.sh
