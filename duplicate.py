#!/usr/bin/python3

# Python shell script for Duplicating IPlug Projects
# Oli Larkin 2012-2019
# License: WTFPL http://sam.zoy.org/wtfpl/COPYING
# Modified from this script by Bibha Tripathi http://code.activestate.com/recipes/435904-sedawk-python-script-to-rename-subdirectories-of-a/
# Author accepts no responsibilty for wiping your hd

# NOTES:
# should work with Python2 or Python3
# not designed to be fool proof- think carefully about what you choose for a project name
# best to stick to standard characters in your project names - avoid spaces, numbers and dots
# windows users need to install python and set it up so you can run it from the command line
# see http://www.voidspace.org.uk/python/articles/command_line.shtml
# this involves adding the python folder e.g. C:\Python27\ to your %PATH% environment variable

# USAGE:
# duplicate.py [inputprojectname] [outputprojectname] [manufacturername] (outputpath)

# TODO:
# - indentation of directory structure
# - variable manufacturer name


from __future__ import generators

import fileinput, glob, string, sys, os, re, uuid, pprint, random
from shutil import copy, copytree, ignore_patterns, rmtree
from os.path import join

scriptpath = os.path.dirname(os.path.realpath(__file__))

sys.path.insert(0, scriptpath + '/iPlug2/Scripts/')

from parse_config import parse_config, parse_xcconfig, set_uniqueid

VERSION = "0.95"

# binary files that we don't want to do find and replace inside
FILTERED_FILE_EXTENSIONS = [".ico",".icns", ".pdf", ".png", ".zip", ".exe", ".wav", ".aif", ".data", ".wasm", "mkcert"]
FILTERED_FILE_NAMES = [".DS_Store"]
# files that we don't want to duplicate
DONT_COPY = (".vs", "*.exe", "*.dmg", "*.pkg", "*.mpkg", "*.svn", "*.ncb", "*.suo", "*sdf", "ipch", "*.layout", "*.depend", ".DS_Store", "xcuserdata", "*.aps")

SUBFOLDERS_TO_SEARCH = [
"projects",
"config",
"resources",
"installer",
"scripts",
"manual",
"xcschemes",
"xcshareddata",
"xcuserdata",
"en-osx.lproj",
"project.xcworkspace",
"Images.xcassets",
"build-web"
]

def randomFourChar(chars=string.ascii_letters + string.digits):
  return ''.join(random.choice(chars) for _ in range(4))

def checkdirname(name, searchproject):
  "check if directory name matches with the given pattern"
  print("")
  if name == searchproject:
    return True
  else:
    return False

def replacestrs(filename, s, r):
  files = glob.glob(filename)

  for line in fileinput.input(files,inplace=1):
    line.find(s)
    line = line.replace(s, r)
    sys.stdout.write(line)

def replacestrsChop(filename, s, r):
  files = glob.glob(filename)

  for line in fileinput.input(files,inplace=1):
    if(line.startswith(s)):
      line = r + "\n"
    sys.stdout.write(line)

def dirwalk(dir, searchproject, replaceproject, searchman, replaceman, oldroot= "", newroot=""):
  for f in os.listdir(dir):
    fullpath = os.path.join(dir, f)

    if os.path.isdir(fullpath) and not os.path.islink(fullpath):
      if checkdirname(f, searchproject + "-macOS.xcodeproj"):
        os.rename(fullpath, os.path.join(dir, replaceproject + "-macOS.xcodeproj"))
        fullpath = os.path.join(dir, replaceproject + "-macOS.xcodeproj")

        print("recursing in macOS xcode project directory: ")
        for x in dirwalk(fullpath, searchproject, replaceproject, searchman, replaceman, oldroot, newroot):
          yield x
      elif checkdirname(f, searchproject + "-iOS.xcodeproj"):
        os.rename(fullpath, os.path.join(dir, replaceproject + "-iOS.xcodeproj"))
        fullpath = os.path.join(dir, replaceproject + "-iOS.xcodeproj")

        print("recursing in iOS xcode project directory: ")
        for x in dirwalk(fullpath, searchproject, replaceproject, searchman, replaceman, oldroot, newroot):
          yield x
      elif checkdirname(f, searchproject + ".xcworkspace"):
        os.rename(fullpath, os.path.join(dir, replaceproject + ".xcworkspace"))
        fullpath = os.path.join(dir, replaceproject + ".xcworkspace")

        print("recursing in main xcode workspace directory: ")
        for x in dirwalk(fullpath, searchproject, replaceproject, searchman, replaceman, oldroot, newroot):
          yield x
      elif checkdirname(f, searchproject + "-iOS.appiconset"):
        os.rename(fullpath, os.path.join(dir, replaceproject + "-iOS.appiconset"))
        fullpath = os.path.join(dir, replaceproject + "-iOS.appiconset")

        print("recursing in -iOS.appiconset directory: ")
      elif checkdirname(f, searchproject + "-macOS.appiconset"):
        os.rename(fullpath, os.path.join(dir, replaceproject + "-macOS.appiconset"))
        fullpath = os.path.join(dir, replaceproject + "-macOS.appiconset")

        print("recursing in -macOS.appiconset directory: ")
        for x in dirwalk(fullpath, searchproject, replaceproject, searchman, replaceman, oldroot, newroot):
          yield x
      elif (f in SUBFOLDERS_TO_SEARCH):
        print('recursing in ' + f + ' directory: ')
        for x in dirwalk(fullpath, searchproject, replaceproject, searchman, replaceman, oldroot, newroot):
          yield x

    if os.path.isfile(fullpath):
      filename = os.path.basename(fullpath)
      newfilename = filename.replace(searchproject, replaceproject)
      base, extension = os.path.splitext(filename)

      if (not(extension in FILTERED_FILE_EXTENSIONS) and not(filename in FILTERED_FILE_NAMES)):

        print("Replacing project name strings in file " + filename)
        replacestrs(fullpath, searchproject, replaceproject)

        print("Replacing captitalized project name strings in file " + filename)
        replacestrs(fullpath, searchproject.upper(), replaceproject.upper())

        print("Replacing manufacturer name strings in file " + filename)
        replacestrs(fullpath, searchman, replaceman)

        if (oldroot and newroot):
          print ("Replacing iPlug2 root folder in file  " + filename)
          replacestrs(fullpath, oldroot, newroot)
          replacestrs(fullpath, oldroot.replace('/', '\\'), newroot.replace('/', '\\'))

      else:
        print("NOT replacing name strings in file " + filename)

      if filename != newfilename:
        print("Renaming file " + filename + " to " + newfilename)
        os.rename(fullpath, os.path.join(dir, newfilename))

      yield f, fullpath
    else:
      yield f, fullpath

def main():
  global VERSION
  print("\nIPlug Project Duplicator v" + VERSION + " by Oli Larkin ------------------------------\n")

  numargs = len(sys.argv) - 1

  if not (numargs == 3 or numargs == 4):
    print("Usage: duplicate.py inputprojectname outputprojectname manufacturername (outputprojectpath)")
    sys.exit(1)
  else:
    inputprojectname=sys.argv[1]
    outputprojectname=sys.argv[2]
    manufacturer=sys.argv[3]

  if numargs == 4:
    outputbasepath=os.path.abspath(sys.argv[4])
  else:
    outputbasepath=os.getcwd()

  if not (os.path.isdir(outputbasepath)):
    print("error: Output path does not exist")
    sys.exit(1)

  outputpath = os.path.join(outputbasepath, outputprojectname)

  if ' ' in inputprojectname:
    print("error: input project name has spaces")
    sys.exit(1)

  if inputprojectname not in os.listdir(os.curdir):
    print("error: input project " +  inputprojectname + " doesn't exist, check spelling/case?")
    sys.exit(1)

  if ' ' in outputprojectname:
    print("error: output project name has spaces")
    sys.exit(1)

  if ' ' in manufacturer:
    print("error: manufacturer name has spaces")
    sys.exit(1)

  # remove a trailing slash if it exists
  if inputprojectname[-1:] == "/":
    inputprojectname = inputprojectname[0:-1]

  if outputprojectname[-1:] == "/":
    outputprojectname = outputprojectname[0:-1]

  #check that the folders are OK
  if os.path.isdir(inputprojectname) == False:
    print("error: input project not found")
    sys.exit(1)

  if os.path.isdir(outputpath):
    print("error: output project allready exists")
    sys.exit(1)
  # rmtree(output)

  print("copying " + inputprojectname + " folder to " + outputpath)
  copytree(inputprojectname, outputpath, ignore=ignore_patterns(*DONT_COPY))

  oldroot = ""
  newroot = ""
  
  if numargs == 4:
    configpath = os.path.join(inputprojectname, "config")
    xcconfig = parse_xcconfig(configpath + "/" + inputprojectname + "-mac.xcconfig")
    oldroot = xcconfig["IPLUG2_ROOT"]
    iplug2folder = os.path.abspath(os.path.join(configpath, oldroot))
    newroot = os.path.relpath(iplug2folder, os.path.join(outputpath, "config"))
  else:
    newroot = ""

  #replace manufacturer name strings
  for dir in dirwalk(outputpath, inputprojectname, outputprojectname, "AcmeInc", manufacturer, oldroot, newroot):
    pass

  #replace project name in root
  for dir in dirwalk(scriptpath, inputprojectname, outputprojectname, "AcmeInc", manufacturer, oldroot, newroot):
    pass

  #replace project name in github
  for dir in dirwalk(scriptpath + "/.github/workflows", inputprojectname, outputprojectname, "AcmeInc", manufacturer, oldroot, newroot):
    pass

  #replace project name in vscode
  for dir in dirwalk(scriptpath + "/.vscode", inputprojectname, outputprojectname, "AcmeInc", manufacturer, oldroot, newroot):
    pass

  # print("\ncopying gitignore template into project folder\n")

  # copy('gitignore_template', outputpath + "/.gitignore")

  config = parse_config(outputpath)

  config["PLUG_UNIQUE_ID"] = randomFourChar()

  set_uniqueid(outputpath, config["PLUG_UNIQUE_ID"])

  pp = pprint.PrettyPrinter(indent=4)
  pp.pprint(config)

  print("\ndone - don't forget to change PLUG_MFR_UID in config.h")

if __name__ == '__main__':
  main()
