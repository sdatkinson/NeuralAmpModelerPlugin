#!/bin/bash
# 
# git submodule update --init iPlug2
# cd scripts
# ./patch-sdk-download.sh

PATCH_SCRIPT_NAME=download-vst3-sdk-PATCH.sh
IPLUG_DIR=../iPlug2/Dependencies/iPlug

cp resources/${PATCH_SCRIPT_NAME} ${IPLUG_DIR}

sed -i "s/download-vst3-sdk.sh/${PATCH_SCRIPT_NAME}/g" ${IPLUG_DIR}/download-iplug-sdks.sh
