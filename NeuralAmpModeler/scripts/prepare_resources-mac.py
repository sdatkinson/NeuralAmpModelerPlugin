#!/usr/bin/env python3

# this script will copy the project's resources (pngs, ttfs, svgs etc) to the correct place
# depending on the value of PLUG_SHARED_RESOURCES in config.h
# resources can either be copied into the plug-in bundle or into a shared path
# since the shared path should be accesible from the mac app sandbox,
# the path used is ~/Music/SHARED_RESOURCES_SUBPATH

import os, sys, shutil

scriptpath = os.path.dirname(os.path.realpath(__file__))
projectpath = os.path.abspath(os.path.join(scriptpath, os.pardir))

IPLUG2_ROOT = "../../iPlug2"

sys.path.insert(0, os.path.join(os.getcwd(), IPLUG2_ROOT + '/Scripts'))

from parse_config import parse_config

def main():
  config = parse_config(projectpath)

  print("Copying resources ...")

  if config['PLUG_SHARED_RESOURCES']:
    dst = os.path.expanduser("~") + "/Music/" + config['SHARED_RESOURCES_SUBPATH'] + "/Resources"
  else:
    dst = os.environ["TARGET_BUILD_DIR"] + os.environ["UNLOCALIZED_RESOURCES_FOLDER_PATH"]

  if os.path.exists(dst) == False:
    os.makedirs(dst + "/", 0o0755 )

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
