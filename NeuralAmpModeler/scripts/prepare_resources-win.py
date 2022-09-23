#!/usr/bin/env python3

import plistlib, os, datetime, fileinput, glob, sys, string, shutil

scriptpath = os.path.dirname(os.path.realpath(__file__))
projectpath = os.path.abspath(os.path.join(scriptpath, os.pardir))

IPLUG2_ROOT = "../../iPlug2"

sys.path.insert(0, os.path.join(os.getcwd(), IPLUG2_ROOT + '/Scripts'))

from parse_config import parse_config

def main():
  print("not modifying rc file");
  # config = parse_config(projectpath)
  
  # rc = open(projectpath + "/resources/main.rc", "w")
  
  # rc.write("\n")
  # rc.write("/////////////////////////////////////////////////////////////////////////////\n")
  # rc.write("// Version\n")
  # rc.write("/////////////////////////////////////////////////////////////////////////////\n")
  # rc.write("VS_VERSION_INFO VERSIONINFO\n")
  # rc.write("FILEVERSION " + config['MAJOR_STR'] + "," + config['MINOR_STR'] + "," + config['BUGFIX_STR'] + ",0\n")
  # rc.write("PRODUCTVERSION " + config['MAJOR_STR'] + "," + config['MINOR_STR'] + "," + config['BUGFIX_STR'] + ",0\n")
  # rc.write(" FILEFLAGSMASK 0x3fL\n")
  # rc.write("#ifdef _DEBUG\n")
  # rc.write(" FILEFLAGS 0x1L\n")
  # rc.write("#else\n")
  # rc.write(" FILEFLAGS 0x0L\n")
  # rc.write("#endif\n")
  # rc.write(" FILEOS 0x40004L\n")
  # rc.write(" FILETYPE 0x1L\n")
  # rc.write(" FILESUBTYPE 0x0L\n")
  # rc.write("BEGIN\n")
  # rc.write('    BLOCK "StringFileInfo"\n')
  # rc.write("    BEGIN\n")
  # rc.write('        BLOCK "040004e4"\n')
  # rc.write("        BEGIN\n")
  # rc.write('            VALUE "FileVersion", "' + config['FULL_VER_STR'] + '"\0\n')
  # rc.write('            VALUE "ProductVersion", "' + config['FULL_VER_STR'] + '"0\n')
  # rc.write("#ifdef VST2_API\n")
  # rc.write('            VALUE "OriginalFilename", "' + config['BUNDLE_NAME'] + '.dll"\0\n')
  # rc.write("#elif defined VST3_API\n")
  # rc.write('            VALUE "OriginalFilename", "' + config['BUNDLE_NAME'] + '.vst3"\0\n')
  # rc.write("#elif defined AAX_API\n")
  # rc.write('            VALUE "OriginalFilename", "' + config['BUNDLE_NAME'] + '.aaxplugin"\0\n')
  # rc.write("#elif defined APP_API\n")
  # rc.write('            VALUE "OriginalFilename", "' + config['BUNDLE_NAME'] + '.exe"\0\n')
  # rc.write("#endif\n")  
  # rc.write('            VALUE "FileDescription", "' + config['PLUG_NAME'] + '"\0\n')
  # rc.write('            VALUE "InternalName", "' + config['PLUG_NAME'] + '"\0\n')
  # rc.write('            VALUE "ProductName", "' + config['PLUG_NAME'] + '"\0\n')
  # rc.write('            VALUE "CompanyName", "' + config['PLUG_MFR'] + '"\0\n')
  # rc.write('            VALUE "LegalCopyright", "' + config['PLUG_COPYRIGHT_STR'] + '"\0\n')
  # rc.write('            VALUE "LegalTrademarks", "' + config['PLUG_TRADEMARKS'] + '"\0\n')
  # rc.write("        END\n")
  # rc.write("    END\n")
  # rc.write('    BLOCK "VarFileInfo"\n')
  # rc.write("    BEGIN\n")
  # rc.write('        VALUE "Translation", 0x400, 1252\n')
  # rc.write("    END\n")
  # rc.write("END\n")
  # rc.write("\n")

if __name__ == '__main__':
  main()
