#!/bin/bash

# makedist-web.sh builds a Web version of an iPlug2 project using emscripten
# it copies a template folder from the iPlug2 tree and does a find and replace on various JavaScript and HTML files
# arguments:
# 1st argument : either "on", "off" or "ws" - this specifies whether $EMRUN is used to launch a server and browser after compilation. "ws" builds the project in websocket mode, without the WAM stuff
# 2nd argument : site origin -
# 3rd argument : browser - either "chrome", "safari", "firefox" - if you want to launch a browser other than chrome, you must specify the correct origin for argument #2

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
IPLUG2_ROOT=../../iPlug2
PROJECT_ROOT=$SCRIPT_DIR/..
IPLUG2_ROOT=$SCRIPT_DIR/$IPLUG2_ROOT
FILE_PACKAGER=$EMSDK/upstream/emscripten/tools/file_packager.py
EMRUN="python3 ${IPLUG2_ROOT}/Scripts/emrun/emrun.py"

PROJECT_NAME=TemplateProject
BUILD_DSP=1
BUILD_EDITOR=1
WEBSOCKET_MODE=0
EMRUN_BROWSER=chrome
LAUNCH_EMRUN=1
EMRUN_SERVER=1
EMRUN_SERVER_PORT=8001
EMRUN_CONTAINER=0
SITE_ORIGIN="/"

cd $PROJECT_ROOT

if [ "$1" = "ws" ]; then
  LAUNCH_EMRUN=0
  BUILD_DSP=0
  WEBSOCKET_MODE=1
elif [ "$1" = "off" ]; then
  LAUNCH_EMRUN=0
elif [ "$1" = "container" ]; then
  EMRUN_CONTAINER=1
fi

if [ "$#" -eq 2 ]; then
  SITE_ORIGIN=${2}
fi

if [ "$#" -eq 3 ]; then
  EMRUN_BROWSER=${3}
fi

# check to see if the build web folder has its own git repo
if [ -d build-web/.git ]
then
  # if so trash only the scripts
  if [ -d build-web/scripts ]; then
    if [ "$BUILD_DSP" -eq "1" ]; then
      rm build-web/scripts/*-wam.js
    fi

    if [ "$BUILD_EDITOR" -eq "1" ]; then
      rm build-web/scripts/*-web.*
    fi
  fi
else
  # otherwise trash the whole build-web folder
  if [ -d build-web ]; then 
    rm -r build-web
  fi

  mkdir build-web
fi

mkdir build-web/scripts

echo BUNDLING RESOURCES -----------------------------

if [ -f ./build-web/imgs.js ]; then rm ./build-web/imgs.js; fi
if [ -f ./build-web/imgs@2x.js ]; then rm ./build-web/imgs@2x.js; fi
if [ -f ./build-web/svgs.js ]; then rm ./build-web/svgs.js; fi
if [ -f ./build-web/fonts.js ]; then rm ./build-web/fonts.js; fi

FILE_PACKAGER=$EMSDK/upstream/emscripten/tools/file_packager.py
#package fonts
FOUND_FONTS=0
if [ "$(ls -A ./resources/fonts/*.ttf)" ]; then
  FOUND_FONTS=1
  python3 $FILE_PACKAGER fonts.data --preload ./resources/fonts/ --exclude *DS_Store --js-output=./fonts.js
fi

#package svgs
FOUND_SVGS=0
if [ "$(ls -A ./resources/img/*.svg)" ]; then
  FOUND_SVGS=1
  python3 $FILE_PACKAGER svgs.data --preload ./resources/img/ --exclude *.png --exclude *DS_Store --js-output=./svgs.js
fi

#package @1x pngs
FOUND_PNGS=0
if [ "$(ls -A ./resources/img/*.png)" ]; then
  FOUND_PNGS=1
  python3 $FILE_PACKAGER imgs.data --use-preload-plugins --preload ./resources/img/ --use-preload-cache --indexedDB-name="/$PROJECT_NAME_pkg" --exclude *DS_Store --exclude  *@2x.png --exclude  *.svg >> ./imgs.js
fi

# package @2x pngs into separate .data file
FOUND_2XPNGS=0
if [ "$(ls -A ./resources/img/*@2x*.png)" ]; then
  FOUND_2XPNGS=1
  mkdir ./build-web/2x/
  cp ./resources/img/*@2x* ./build-web/2x
  python3 $FILE_PACKAGER imgs@2x.data --use-preload-plugins --preload ./2x@/resources/img/ --use-preload-cache --indexedDB-name="/$PROJECT_NAME_pkg" --exclude *DS_Store >> ./imgs@2x.js
  rm -r ./build-web/2x
fi

if [ -f ./imgs.js ]; then mv ./imgs.js ./build-web/imgs.js; fi
if [ -f ./imgs@2x.js ]; then mv ./imgs@2x.js ./build-web/imgs@2x.js; fi
if [ -f ./svgs.js ]; then mv ./svgs.js ./build-web/svgs.js; fi
if [ -f ./fonts.js ]; then mv ./fonts.js ./build-web/fonts.js; fi

if [ -f ./imgs.data ]; then mv ./imgs.data ./build-web/imgs.data; fi
if [ -f ./imgs@2x.data ]; then mv ./imgs@2x.data ./build-web/imgs@2x.data; fi
if [ -f ./svgs.data ]; then mv ./svgs.data ./build-web/svgs.data; fi
if [ -f ./fonts.data ]; then mv ./fonts.data ./build-web/fonts.data; fi

if [ "$BUILD_DSP" -eq "1" ]; then
  echo MAKING  - WAM WASM MODULE -----------------------------
  cd $PROJECT_ROOT/projects
  emmake make --makefile $PROJECT_NAME-wam-processor.mk

  if [ $? -ne "0" ]; then
    echo IPlugWAM WASM compilation failed
    exit 1
  fi

  cd $PROJECT_ROOT/build-web/scripts

  # prefix the -wam.js script with scope
  echo "AudioWorkletGlobalScope.WAM = AudioWorkletGlobalScope.WAM || {}; AudioWorkletGlobalScope.WAM.$PROJECT_NAME = { ENVIRONMENT: 'WEB' };" > $PROJECT_NAME-wam.tmp.js;
  cat $PROJECT_NAME-wam.js >> $PROJECT_NAME-wam.tmp.js
  mv $PROJECT_NAME-wam.tmp.js $PROJECT_NAME-wam.js
  
  # copy in WAM SDK and AudioWorklet polyfill scripts
  cp $IPLUG2_ROOT/Dependencies/IPlug/WAM_SDK/wamsdk/*.js .
  cp $IPLUG2_ROOT/Dependencies/IPlug/WAM_AWP/*.js .

  # copy in template scripts
  cp $IPLUG2_ROOT/IPlug/WEB/Template/scripts/IPlugWAM-awn.js $PROJECT_NAME-awn.js
  cp $IPLUG2_ROOT/IPlug/WEB/Template/scripts/IPlugWAM-awp.js $PROJECT_NAME-awp.js

  # replace NAME_PLACEHOLDER in the template -awn.js and -awp.js scripts
  sed -i.bak s/NAME_PLACEHOLDER/$PROJECT_NAME/g $PROJECT_NAME-awn.js
  sed -i.bak s/NAME_PLACEHOLDER/$PROJECT_NAME/g $PROJECT_NAME-awp.js

  # replace ORIGIN_PLACEHOLDER in the template -awn.js script
  sed -i.bak s,ORIGIN_PLACEHOLDER,$SITE_ORIGIN,g $PROJECT_NAME-awn.js

  rm *.bak
else
  echo "WAM not being built, BUILD_DSP = 0"
fi

cd $PROJECT_ROOT/build-web

# copy in the template HTML - comment this out if you have customised the HTML
cp $IPLUG2_ROOT/IPlug/WEB/Template/index.html index.html
sed -i.bak s/NAME_PLACEHOLDER/$PROJECT_NAME/g index.html

if [ $FOUND_FONTS -eq "0" ]; then sed -i.bak s/'<script async src="fonts.js"><\/script>'/'<!--<script async src="fonts.js"><\/script>-->'/g index.html; fi
if [ $FOUND_SVGS -eq "0" ]; then sed -i.bak s/'<script async src="svgs.js"><\/script>'/'<!--<script async src="svgs.js"><\/script>-->'/g index.html; fi
if [ $FOUND_PNGS -eq "0" ]; then sed -i.bak s/'<script async src="imgs.js"><\/script>'/'<!--<script async src="imgs.js"><\/script>-->'/g index.html; fi
if [ $FOUND_2XPNGS -eq "0" ]; then sed -i.bak s/'<script async src="imgs@2x.js"><\/script>'/'<!--<script async src="imgs@2x.js"><\/script>-->'/g index.html; fi
if [ $WEBSOCKET_MODE -eq "1" ]; then
  cp $IPLUG2_ROOT/Dependencies/IPlug/WAM_SDK/wamsdk/wam-controller.js scripts/wam-controller.js
  cp $IPLUG2_ROOT/IPlug/WEB/Template/scripts/websocket.js scripts/websocket.js
  sed -i.bak s/'<script src="scripts\/audioworklet.js"><\/script>'/'<!--<script src="scripts\/audioworklet.js"><\/script>-->'/g index.html;
  sed -i.bak s/'let WEBSOCKET_MODE=false;'/'let WEBSOCKET_MODE=true;'/g index.html;
else
  sed -i.bak s/'<script src="scripts\/websocket.js"><\/script>'/'<!--<script src="scripts\/websocket.js"><\/script>-->'/g index.html;

  # update the i/o details for the AudioWorkletNodeOptions parameter, based on config.h channel io str
  MAXNINPUTS=$(python3 $IPLUG2_ROOT/Scripts/parse_iostr.py "$PROJECT_ROOT" inputs)
  MAXNOUTPUTS=$(python3 $IPLUG2_ROOT/Scripts/parse_iostr.py "$PROJECT_ROOT" outputs)

  if [ $MAXNINPUTS -eq "0" ]; then 
    MAXNINPUTS="";
    sed -i.bak '181,203d' index.html; # hack to remove GetUserMedia() from code, and allow WKWebKitView usage for instruments
  fi
  sed -i.bak s/"MAXNINPUTS_PLACEHOLDER"/"$MAXNINPUTS"/g index.html;
  sed -i.bak s/"MAXNOUTPUTS_PLACEHOLDER"/"$MAXNOUTPUTS"/g index.html;
fi

rm *.bak

# copy the style & WAM favicon
mkdir styles
cp $IPLUG2_ROOT/IPlug/WEB/Template/styles/style.css styles/style.css
cp $IPLUG2_ROOT/IPlug/WEB/Template/favicon.ico favicon.ico

echo MAKING  - WEB WASM MODULE -----------------------------

cd $PROJECT_ROOT/projects

emmake make --makefile $PROJECT_NAME-wam-controller.mk EXTRA_CFLAGS=-DWEBSOCKET_CLIENT=$WEBSOCKET_MODE

if [ $? -ne "0" ]; then
  echo IPlugWEB WASM compilation failed
  exit 1
fi

cd $PROJECT_ROOT/build-web

# print payload
echo payload:
find . -maxdepth 2 -mindepth 1 -name .git -type d \! -prune -o \! -name .DS_Store -type f -exec du -hs {} \;

# launch emrun
if [ "$LAUNCH_EMRUN" -eq "1" ]; then
  mkcert 127.0.0.1 localhost
  if [ "$EMRUN_CONTAINER" -eq "1" ]; then
    $EMRUN --no_browser --serve_after_close --serve_after_exit --port=$EMRUN_SERVER_PORT --hostname=0.0.0.0 .
  elif [ "$EMRUN_SERVER" -eq "0" ]; then
    $EMRUN --browser $EMRUN_BROWSER --no_server --port=$EMRUN_SERVER_PORT index.html
  else
    $EMRUN --browser $EMRUN_BROWSER --no_emrun_detect index.html
  fi
else
  echo "Not running emrun"
fi
