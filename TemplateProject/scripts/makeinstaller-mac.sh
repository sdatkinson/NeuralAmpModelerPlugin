#!/bin/bash

# IPlug2 project macOS installer build script, using pkgbuild and productbuild
# based on script for SURGE https://github.com/surge-synthesizer/surge

# Documentation for pkgbuild and productbuild: https://developer.apple.com/library/archive/documentation/DeveloperTools/Reference/DistributionDefinitionRef/Chapters/Distribution_XML_Ref.html

# preflight check

if [[ ! -d build-mac ]]
then
  echo "You must run this script from the project directory!"
  exit 1
fi

# version
if [ "$PLUGIN_VERSION" != "" ]; then
  VERSION="$PLUGIN_VERSION"
elif [ "$1" != "" ]; then
  VERSION="$1"
fi

if [ "$VERSION" == "" ]; then
  echo "You must specify the version you are packaging as the first argument!"
  echo "eg: makeinstaller-mac.sh 1.0.6"
  exit 1
fi

PRODUCT_NAME=TemplateProject

# locations
PRODUCTS="build-mac"

VST2="${PRODUCT_NAME}.vst"
VST3="${PRODUCT_NAME}.vst3"
AU="${PRODUCT_NAME}.component"
APP="${PRODUCT_NAME}.app"
AAX="${PRODUCT_NAME}.aaxplugin"

RSRCS="~/Music/${PRODUCT_NAME}/Resources"

OUTPUT_BASE_FILENAME="${PRODUCT_NAME} Installer.pkg"

TARGET_DIR="./build-mac/installer"
PKG_DIR=${TARGET_DIR}/pkgs

if [[ ! -d ${TARGET_DIR} ]]; then
  mkdir ${TARGET_DIR}
fi

if [[ ! -d ${PKG_DIR} ]]; then
  mkdir ${PKG_DIR}
fi

build_flavor()
{
  TMPDIR=${TARGET_DIR}/tmp
  flavor=$1
  flavorprod=$2
  ident=$3
  loc=$4

  echo --- BUILDING ${PRODUCT_NAME}_${flavor}.pkg ---

  mkdir -p $TMPDIR
  cp -R -L $PRODUCTS/$flavorprod $TMPDIR

  pkgbuild --root $TMPDIR --identifier $ident --version $VERSION --install-location $loc ${PKG_DIR}/${PRODUCT_NAME}_${flavor}.pkg #|| exit 1

  rm -r $TMPDIR
}

# try to build VST2 package
if [[ -d $PRODUCTS/$VST2 ]]; then
  build_flavor "VST2" $VST2 "com.AcmeInc.vst2.pkg.${PRODUCT_NAME}" "/Library/Audio/Plug-Ins/VST"
fi

# # try to build VST3 package
if [[ -d $PRODUCTS/$VST3 ]]; then
  build_flavor "VST3" $VST3 "com.AcmeInc.vst3.pkg.${PRODUCT_NAME}" "/Library/Audio/Plug-Ins/VST3"
fi

# # try to build AU package
if [[ -d $PRODUCTS/$AU ]]; then
  build_flavor "AU" $AU "com.AcmeInc.au.pkg.${PRODUCT_NAME}" "/Library/Audio/Plug-Ins/Components"
fi

# # try to build AAX package
if [[ -d $PRODUCTS/$AAX ]]; then
  build_flavor "AAX" $AAX "com.AcmeInc.aax.pkg.${PRODUCT_NAME}" ""/Library/Application Support/Avid/Audio/Plug-Ins""
fi

# try to build App package
if [[ -d $PRODUCTS/$APP ]]; then
  build_flavor "APP" $APP "com.AcmeInc.app.pkg.${PRODUCT_NAME}" "/Applications"
fi

# write build info to resources folder

# echo "Version: ${VERSION}" > "$RSRCS/BuildInfo.txt"
# echo "Packaged on: $(date "+%Y-%m-%d %H:%M:%S")" >> "$RSRCS/BuildInfo.txt"
# echo "On host: $(hostname)" >> "$RSRCS/BuildInfo.txt"
# echo "Commit: $(git rev-parse HEAD)" >> "$RSRCS/BuildInfo.txt"

# build resources package
# --scripts ResourcesPackageScript
# pkgbuild --root "$RSRCS" --identifier "com.AcmeInc.resources.pkg.${PRODUCT_NAME}" --version $VERSION --install-location "/tmp/${PRODUCT_NAME}" ${PRODUCT_NAME}_RES.pkg

# remove build info from resource folder
# rm "$RSRCS/BuildInfo.txt"

# create distribution.xml

if [[ -d $PRODUCTS/$VST2 ]]; then
	VST2_PKG_REF="<pkg-ref id=\"com.AcmeInc.vst2.pkg.${PRODUCT_NAME}\"/>"
	VST2_CHOICE="<line choice=\"com.AcmeInc.vst2.pkg.${PRODUCT_NAME}\"/>"
	VST2_CHOICE_DEF="<choice id=\"com.AcmeInc.vst2.pkg.${PRODUCT_NAME}\" visible=\"true\" start_selected=\"true\" title=\"VST2 Plug-in\"><pkg-ref id=\"com.AcmeInc.vst2.pkg.${PRODUCT_NAME}\"/></choice><pkg-ref id=\"com.AcmeInc.vst2.pkg.${PRODUCT_NAME}\" version=\"${VERSION}\" onConclusion=\"none\">${PRODUCT_NAME}_VST2.pkg</pkg-ref>"
fi
if [[ -d $PRODUCTS/$VST3 ]]; then
	VST3_PKG_REF="<pkg-ref id=\"com.AcmeInc.vst3.pkg.${PRODUCT_NAME}\"/>"
	VST3_CHOICE="<line choice=\"com.AcmeInc.vst3.pkg.${PRODUCT_NAME}\"/>"
	VST3_CHOICE_DEF="<choice id=\"com.AcmeInc.vst3.pkg.${PRODUCT_NAME}\" visible=\"true\" start_selected=\"true\" title=\"VST3 Plug-in\"><pkg-ref id=\"com.AcmeInc.vst3.pkg.${PRODUCT_NAME}\"/></choice><pkg-ref id=\"com.AcmeInc.vst3.pkg.${PRODUCT_NAME}\" version=\"${VERSION}\" onConclusion=\"none\">${PRODUCT_NAME}_VST3.pkg</pkg-ref>"
fi
if [[ -d $PRODUCTS/$AU ]]; then
	AU_PKG_REF="<pkg-ref id=\"com.AcmeInc.au.pkg.${PRODUCT_NAME}\"/>"
	AU_CHOICE="<line choice=\"com.AcmeInc.au.pkg.${PRODUCT_NAME}\"/>"
	AU_CHOICE_DEF="<choice id=\"com.AcmeInc.au.pkg.${PRODUCT_NAME}\" visible=\"true\" start_selected=\"true\" title=\"Audio Unit (v2) Plug-in\"><pkg-ref id=\"com.AcmeInc.au.pkg.${PRODUCT_NAME}\"/></choice><pkg-ref id=\"com.AcmeInc.au.pkg.${PRODUCT_NAME}\" version=\"${VERSION}\" onConclusion=\"none\">${PRODUCT_NAME}_AU.pkg</pkg-ref>"
fi
if [[ -d $PRODUCTS/$AAX ]]; then
	AAX_PKG_REF="<pkg-ref id=\"com.AcmeInc.aax.pkg.${PRODUCT_NAME}\"/>"
	AAX_CHOICE="<line choice=\"com.AcmeInc.aax.pkg.${PRODUCT_NAME}\"/>"
	AAX_CHOICE_DEF="<choice id=\"com.AcmeInc.aax.pkg.${PRODUCT_NAME}\" visible=\"true\" start_selected=\"true\" title=\"AAX Plug-in\"><pkg-ref id=\"com.AcmeInc.aax.pkg.${PRODUCT_NAME}\"/></choice><pkg-ref id=\"com.AcmeInc.aax.pkg.${PRODUCT_NAME}\" version=\"${VERSION}\" onConclusion=\"none\">${PRODUCT_NAME}_AAX.pkg</pkg-ref>"
fi
if [[ -d $PRODUCTS/$APP ]]; then
	APP_PKG_REF="<pkg-ref id=\"com.AcmeInc.app.pkg.${PRODUCT_NAME}\"/>"
	APP_CHOICE="<line choice=\"com.AcmeInc.app.pkg.${PRODUCT_NAME}\"/>"
	APP_CHOICE_DEF="<choice id=\"com.AcmeInc.app.pkg.${PRODUCT_NAME}\" visible=\"true\" start_selected=\"true\" title=\"Stand-alone App\"><pkg-ref id=\"com.AcmeInc.app.pkg.${PRODUCT_NAME}\"/></choice><pkg-ref id=\"com.AcmeInc.app.pkg.${PRODUCT_NAME}\" version=\"${VERSION}\" onConclusion=\"none\">${PRODUCT_NAME}_APP.pkg</pkg-ref>"
fi

# if [[ -d $PRODUCTS/$RES ]]; then
	# RES_PKG_REF="<pkg-ref id="com.AcmeInc.resources.pkg.${PRODUCT_NAME}"/>'
	# RES_CHOICE="<line choice="com.AcmeInc.resources.pkg.${PRODUCT_NAME}"/>'
	# RES_CHOICE_DEF="<choice id=\"com.AcmeInc.resources.pkg.${PRODUCT_NAME}\" visible=\"true\" enabled=\"false\" selected=\"true\" title=\"Shared Resources\"><pkg-ref id=\"com.AcmeInc.resources.pkg.${PRODUCT_NAME}\"/></choice><pkg-ref id=\"com.AcmeInc.resources.pkg.${PRODUCT_NAME}\" version=\"${VERSION}\" onConclusion=\"none\">${PRODUCT_NAME}_RES.pkg</pkg-ref>"
# fi

cat > ${TARGET_DIR}/distribution.xml << XMLEND
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>${PRODUCT_NAME} ${VERSION}</title>
    <license file="license.rtf" mime-type="application/rtf"/>
    <readme file="readme-mac.rtf" mime-type="application/rtf"/>
    <welcome file="intro.rtf" mime-type="application/rtf"/>
    <background file="${PRODUCT_NAME}-installer-bg.png" alignment="topleft" scaling="none"/>
    ${VST2_PKG_REF}
    ${VST3_PKG_REF}
    ${AU_PKG_REF}
    ${AAX_PKG_REF}
    ${APP_PKG_REF}
    ${RES_PKG_REF}
    <options require-scripts="false" customize="always" />
    <choices-outline>
        ${VST2_CHOICE}
        ${VST3_CHOICE}
        ${AU_CHOICE}
        ${AAX_CHOICE}
        ${APP_CHOICE}
        ${RES_CHOICE}
    </choices-outline>
    ${VST2_CHOICE_DEF}
    ${VST3_CHOICE_DEF}
    ${AU_CHOICE_DEF}
    ${AAX_CHOICE_DEF}
    ${APP_CHOICE_DEF}
    ${RES_CHOICE_DEF}
</installer-gui-script>
XMLEND

# build installation bundle
# --resources .

productbuild --distribution ${TARGET_DIR}/distribution.xml --package-path ${PKG_DIR} "${TARGET_DIR}/$OUTPUT_BASE_FILENAME"

rm ${TARGET_DIR}/distribution.xml
rm -r $PKG_DIR