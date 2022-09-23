#!/usr/bin/env python3

# this script will create/update info plist files based on config.h

kAudioUnitType_MusicDevice      = "aumu"
kAudioUnitType_MusicEffect      = "aumf"
kAudioUnitType_Effect           = "aufx"
kAudioUnitType_MIDIProcessor    = "aumi"

import plistlib, os, datetime, fileinput, glob, sys, string, shutil

scriptpath = os.path.dirname(os.path.realpath(__file__))
projectpath = os.path.abspath(os.path.join(scriptpath, os.pardir))

IPLUG2_ROOT = "../../iPlug2"

sys.path.insert(0, os.path.join(os.getcwd(), IPLUG2_ROOT + '/Scripts'))

from parse_config import parse_config, parse_xcconfig

def main():
  if(len(sys.argv) == 2):
     if(sys.argv[1] == "app"):
       print("Copying resources ...")
     
       dst = os.environ["TARGET_BUILD_DIR"] + "/" + os.environ["UNLOCALIZED_RESOURCES_FOLDER_PATH"]
          
       if os.path.exists(projectpath + "/resources/img/"):
         imgs = os.listdir(projectpath + "/resources/img/")
         for img in imgs:
           print("copying " + img + " to " + dst)
           shutil.copy(projectpath + "/resources/img/" + img, dst)
     
       if os.path.exists(projectpath + "/resources/fonts/"):
         fonts = os.listdir(projectpath + "/resources/fonts/")
         for font in fonts:
           print("copying " + font + " to " + dst)
           shutil.copy(projectpath + "/resources/fonts/" + font, dst)

if __name__ == '__main__':
  main()
