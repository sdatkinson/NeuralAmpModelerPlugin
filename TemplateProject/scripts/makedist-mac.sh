#! /bin/sh

# this script requires xcpretty https://github.com/xcpretty/xcpretty

BASEDIR=$(dirname $0)

cd $BASEDIR/..

if [ -d build-mac ]; then
  sudo rm -f -R build-mac
fi

#---------------------------------------------------------------------------------------------------------
#variables

IPLUG2_ROOT=../iPlug2
XCCONFIG=$IPLUG2_ROOT/../common-mac.xcconfig
SCRIPTS=$IPLUG2_ROOT/Scripts

# CODESIGN disabled by default. 
CODESIGN=0

# macOS codesigning/notarization
NOTARIZE_BUNDLE_ID=com.AcmeInc.TemplateProject
NOTARIZE_BUNDLE_ID_DEMO=com.AcmeInc.TemplateProject.DEMO
APP_SPECIFIC_ID=TODO
APP_SPECIFIC_PWD=TODO

# AAX/PACE wraptool codesigning
ILOK_ID=TODO
ILOK_PWD=TODO
WRAP_GUID=TODO

DEMO=0
if [ "$1" == "demo" ]; then
  DEMO=1
fi

BUILD_INSTALLER=1
if [ "$2" == "zip" ]; then
  BUILD_INSTALLER=0
fi

VERSION=`echo | grep PLUG_VERSION_HEX config.h`
VERSION=${VERSION//\#define PLUG_VERSION_HEX }
VERSION=${VERSION//\'}
MAJOR_VERSION=$(($VERSION & 0xFFFF0000))
MAJOR_VERSION=$(($MAJOR_VERSION >> 16))
MINOR_VERSION=$(($VERSION & 0x0000FF00))
MINOR_VERSION=$(($MINOR_VERSION >> 8))
BUG_FIX=$(($VERSION & 0x000000FF))

FULL_VERSION=$MAJOR_VERSION"."$MINOR_VERSION"."$BUG_FIX

PLUGIN_NAME=`echo | grep BUNDLE_NAME config.h`
PLUGIN_NAME=${PLUGIN_NAME//\#define BUNDLE_NAME }
PLUGIN_NAME=${PLUGIN_NAME//\"}

ARCHIVE_NAME=$PLUGIN_NAME-v$FULL_VERSION-mac

if [ $DEMO == 1 ]; then
  ARCHIVE_NAME=$ARCHIVE_NAME-demo
fi

# TODO: use get_archive_name script
# if [ $DEMO == 1 ]; then
#   ARCHIVE_NAME=`python3 ${SCRIPTS}/get_archive_name.py ${PLUGIN_NAME} mac demo`
# else
#   ARCHIVE_NAME=`python3 ${SCRIPTS}/get_archive_name.py ${PLUGIN_NAME} mac full`
# fi

VST2=`echo | grep VST2_PATH $XCCONFIG`
VST2=$HOME${VST2//\VST2_PATH = \$(HOME)}/$PLUGIN_NAME.vst

VST3=`echo | grep VST3_PATH $XCCONFIG`
VST3=$HOME${VST3//\VST3_PATH = \$(HOME)}/$PLUGIN_NAME.vst3

AU=`echo | grep AU_PATH $XCCONFIG`
AU=$HOME${AU//\AU_PATH = \$(HOME)}/$PLUGIN_NAME.component

APP=`echo | grep APP_PATH $XCCONFIG`
APP=$HOME${APP//\APP_PATH = \$(HOME)}/$PLUGIN_NAME.app

# Dev build folder
AAX=`echo | grep AAX_PATH $XCCONFIG`
AAX=${AAX//\AAX_PATH = }/$PLUGIN_NAME.aaxplugin
AAX_FINAL="/Library/Application Support/Avid/Audio/Plug-Ins/$PLUGIN_NAME.aaxplugin"

PKG="build-mac/installer/$PLUGIN_NAME Installer.pkg"
PKG_US="build-mac/installer/$PLUGIN_NAME Installer.unsigned.pkg"

CERT_ID=`echo | grep CERTIFICATE_ID $XCCONFIG`
CERT_ID=${CERT_ID//\CERTIFICATE_ID = }
DEV_ID_APP_STR="Developer ID Application: ${CERT_ID}"
DEV_ID_INST_STR="Developer ID Installer: ${CERT_ID}"

echo $VST2
echo $VST3
echo $AU
echo $APP
echo $AAX

if [ $DEMO == 1 ]; then
 echo "making $PLUGIN_NAME version $FULL_VERSION DEMO mac distribution..."
#   cp "resources/img/AboutBox_Demo.png" "resources/img/AboutBox.png"
else
 echo "making $PLUGIN_NAME version $FULL_VERSION mac distribution..."
#   cp "resources/img/AboutBox_Registered.png" "resources/img/AboutBox.png"
fi

sleep 2

echo "touching source to force recompile"
echo ""
touch *.cpp

#---------------------------------------------------------------------------------------------------------
#remove existing binaries

echo "remove existing binaries"
echo ""

if [ -d $APP ]; then
  sudo rm -f -R -f $APP
fi

if [ -d $AU ]; then
 sudo rm -f -R $AU
fi

if [ -d $VST2 ]; then
  sudo rm -f -R $VST2
fi

if [ -d $VST3 ]; then
  sudo rm -f -R $VST3
fi

if [ -d "${AAX}" ]; then
  sudo rm -f -R "${AAX}"
fi

if [ -d "${AAX_FINAL}" ]; then
  sudo rm -f -R "${AAX_FINAL}"
fi

#---------------------------------------------------------------------------------------------------------
# build xcode project. Change target to build individual formats, or add to All target in the xcode project

xcodebuild -project ./projects/$PLUGIN_NAME-macOS.xcodeproj -xcconfig ./config/$PLUGIN_NAME-mac.xcconfig DEMO_VERSION=$DEMO -target "All" -UseModernBuildSystem=NO -configuration Release | tee build-mac.log | xcpretty #&& exit ${PIPESTATUS[0]}

if [ "${PIPESTATUS[0]}" -ne "0" ]; then
  echo "ERROR: build failed, aborting"
  echo ""
  # cat build-mac.log
  exit 1
else
  rm build-mac.log
fi

#---------------------------------------------------------------------------------------------------------
# set bundle icons - http://www.hamsoftengineering.com/codeSharing/SetFileIcon/SetFileIcon.html

echo "setting icons"
echo ""

if [ -d $AU ]; then
  ./$SCRIPTS/SetFileIcon -image resources/$PLUGIN_NAME.icns -file $AU
fi

if [ -d $VST2 ]; then
  ./$SCRIPTS/SetFileIcon -image resources/$PLUGIN_NAME.icns -file $VST2
fi

if [ -d $VST3 ]; then
  ./$SCRIPTS/SetFileIcon -image resources/$PLUGIN_NAME.icns -file $VST3
fi

if [ -d "${AAX}" ]; then
  ./$SCRIPTS/SetFileIcon -image resources/$PLUGIN_NAME.icns -file "${AAX}"
fi

#---------------------------------------------------------------------------------------------------------
#strip symbols from binaries

echo "stripping binaries"
echo ""

if [ -d $APP ]; then
  strip -x $APP/Contents/MacOS/$PLUGIN_NAME
fi

if [ -d $AU ]; then
  strip -x $AU/Contents/MacOS/$PLUGIN_NAME
fi

if [ -d $VST2 ]; then
  strip -x $VST2/Contents/MacOS/$PLUGIN_NAME
fi

if [ -d $VST3 ]; then
  strip -x $VST3/Contents/MacOS/$PLUGIN_NAME
fi

if [ -d "${AAX}" ]; then
  strip -x "${AAX}/Contents/MacOS/$PLUGIN_NAME"
fi

if [ $CODESIGN == 1 ]; then
  #---------------------------------------------------------------------------------------------------------
  # code sign AAX binary with wraptool

  # echo "copying AAX ${PLUGIN_NAME} from 3PDev to main AAX folder"
  # sudo cp -p -R "${AAX}" "${AAX_FINAL}"
  # mkdir "${AAX_FINAL}/Contents/Factory Presets/"
  
  # echo "code sign AAX binary"
  # /Applications/PACEAntiPiracy/Eden/Fusion/Current/bin/wraptool sign --verbose --account $ILOK_ID --password $ILOK_PWD --wcguid $WRAP_GUID --signid "${DEV_ID_APP_STR}" --in "${AAX_FINAL}" --out "${AAX_FINAL}"

  #---------------------------------------------------------------------------------------------------------

  #---------------------------------------------------------------------------------------------------------
  echo "code-sign binaries"
  echo ""

  codesign --force -s "${DEV_ID_APP_STR}" -v $APP --deep --strict --options=runtime #hardened runtime for app
  xattr -cr $AU 
  codesign --force -s "${DEV_ID_APP_STR}" -v $AU --deep --strict
  # xattr -cr $VST2 
  # codesign --force -s "${DEV_ID_APP_STR}" -v $VST2 --deep --strict
  xattr -cr $VST3 
  codesign --force -s "${DEV_ID_APP_STR}" -v $VST3 --deep --strict
  #---------------------------------------------------------------------------------------------------------
fi

if [ $BUILD_INSTALLER == 1 ]; then
  #---------------------------------------------------------------------------------------------------------
  # installer

  sudo rm -R -f build-mac/$PLUGIN_NAME-*.dmg

  echo "building installer"
  echo ""

  ./scripts/makeinstaller-mac.sh $FULL_VERSION

  if [ $CODESIGN == 1 ]; then
    echo "code-sign installer for Gatekeeper on macOS 10.8+"
    echo ""
    mv "${PKG}" "${PKG_US}"
    productsign --sign "${DEV_ID_INST_STR}" "${PKG_US}" "${PKG}"
    rm -R -f "${PKG_US}"
  fi

  #set installer icon
  ./$SCRIPTS/SetFileIcon -image resources/$PLUGIN_NAME.icns -file "${PKG}"

  #---------------------------------------------------------------------------------------------------------
  # make dmg, can use dmgcanvas http://www.araelium.com/dmgcanvas/ to make a nice dmg, fallback to hdiutil
  echo "building dmg"
  echo ""

  if [ -d installer/$PLUGIN_NAME.dmgCanvas ]; then
    dmgcanvas installer/$PLUGIN_NAME.dmgCanvas build-mac/$ARCHIVE_NAME.dmg
  else
    cp installer/changelog.txt build-mac/installer/
    cp installer/known-issues.txt build-mac/installer/
    cp "manual/$PLUGIN_NAME manual.pdf" build-mac/installer/
    hdiutil create build-mac/$ARCHIVE_NAME.dmg -format UDZO -srcfolder build-mac/installer/ -ov -anyowners -volname $PLUGIN_NAME
  fi

  sudo rm -R -f build-mac/installer/

  if [ $CODESIGN == 1 ]; then
    #---------------------------------------------------------------------------------------------------------
    #notarize dmg
    echo "notarizing"
    echo ""
    # you need to create an app-specific id/password https://support.apple.com/en-us/HT204397
    # arg 1 Set to the dmg path
    # arg 2 Set to a bundle ID (doesn't have to match your )
    # arg 3 Set to the app specific Apple ID username/email
    # arg 4 Set to the app specific Apple password  
    PWD=`pwd`

    if [ $DEMO == 1 ]; then
      ./$SCRIPTS/notarise.sh "${PWD}/build-mac" "${PWD}/build-mac/${ARCHIVE_NAME}.dmg" $NOTARIZE_BUNDLE_ID $APP_SPECIFIC_ID $APP_SPECIFIC_PWD
    else
      ./$SCRIPTS/notarise.sh "${PWD}/build-mac" "${PWD}/build-mac/${ARCHIVE_NAME}.dmg" $NOTARIZE_BUNDLE_ID_DEMO $APP_SPECIFIC_ID $APP_SPECIFIC_PWD
    fi

    if [ "${PIPESTATUS[0]}" -ne "0" ]; then
      echo "ERROR: notarize script failed, aborting"
      exit 1
    fi

  fi
else
  #---------------------------------------------------------------------------------------------------------
  # zip

  if [ -d build-mac/zip ]; then
    rm -R build-mac/zip
  fi

  mkdir -p build-mac/zip

  if [ -d $APP ]; then
    cp -R $APP build-mac/zip/$PLUGIN_NAME.app
  fi

  if [ -d $AU ]; then
    cp -R $AU build-mac/zip/$PLUGIN_NAME.component
  fi

  if [ -d $VST2 ]; then
    cp -R $VST2 build-mac/zip/$PLUGIN_NAME.vst
  fi

  if [ -d $VST3 ]; then
    cp -R $VST3 build-mac/zip/$PLUGIN_NAME.vst3
  fi

  if [ -d "${AAX_FINAL}" ]; then
    cp -R $AAX_FINAL build-mac/zip/$PLUGIN_NAME.aaxplugin
  fi

  echo "zipping binaries..."
  echo ""
  ditto -c -k build-mac/zip build-mac/$ARCHIVE_NAME.zip
  rm -R build-mac/zip
fi

#---------------------------------------------------------------------------------------------------------
# dSYMs
sudo rm -R -f build-mac/*-dSYMs.zip

echo "packaging dSYMs"
echo ""
zip -r ./build-mac/$ARCHIVE_NAME-dSYMs.zip ./build-mac/*.dSYM

#---------------------------------------------------------------------------------------------------------

# prepare out folder for CI

echo "preparing output folder"
echo ""
mkdir -p ./build-mac/out
if [ -f ./build-mac/$ARCHIVE_NAME.dmg ]; then
  mv ./build-mac/$ARCHIVE_NAME.dmg ./build-mac/out
fi
mv ./build-mac/*.zip ./build-mac/out

#---------------------------------------------------------------------------------------------------------

#if [ $DEMO == 1 ]
#then
#  git checkout installer/TemplateProject.iss
#  git checkout installer/TemplateProject.pkgproj
#  git checkout resources/img/AboutBox.png
#fi

echo "done!"
echo ""