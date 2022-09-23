#!/usr/bin/env python3

# this script will create/update info plist files based on config.h

import plistlib, os, sys, shutil

IPLUG2_ROOT = "../../iPlug2"

scriptpath = os.path.dirname(os.path.realpath(__file__))
projectpath = os.path.abspath(os.path.join(scriptpath, os.pardir))

kAudioUnitType_MusicDevice      = "aumu"
kAudioUnitType_MusicEffect      = "aumf"
kAudioUnitType_Effect           = "aufx"
kAudioUnitType_MIDIProcessor    = "aumi"

sys.path.insert(0, os.path.join(os.getcwd(), IPLUG2_ROOT + '/Scripts'))

from parse_config import parse_config, parse_xcconfig

def main():
  config = parse_config(projectpath)
  xcconfig = parse_xcconfig(os.path.join(os.getcwd(), IPLUG2_ROOT +  '/../common-mac.xcconfig'))

  CFBundleGetInfoString = config['BUNDLE_NAME'] + " v" + config['FULL_VER_STR'] + " " + config['PLUG_COPYRIGHT_STR']
  CFBundleVersion = config['FULL_VER_STR']
  CFBundlePackageType = "BNDL"
  CSResourcesFileMapped = True
  LSMinimumSystemVersion = xcconfig['DEPLOYMENT_TARGET']

  print("Processing Info.plist files...")

# VST3

  plistpath = projectpath + "/resources/" + config['BUNDLE_NAME'] + "-VST3-Info.plist"
  with open(plistpath, 'rb') as f:
    vst3 = plistlib.load(f)  
    vst3['CFBundleExecutable'] = config['BUNDLE_NAME']
    vst3['CFBundleGetInfoString'] = CFBundleGetInfoString
    vst3['CFBundleIdentifier'] = config['BUNDLE_DOMAIN'] + "." + config['BUNDLE_MFR'] + ".vst3." + config['BUNDLE_NAME'] + ""
    vst3['CFBundleName'] = config['BUNDLE_NAME']
    vst3['CFBundleVersion'] = CFBundleVersion
    vst3['CFBundleShortVersionString'] = CFBundleVersion
    vst3['LSMinimumSystemVersion'] = LSMinimumSystemVersion
    vst3['CFBundlePackageType'] = CFBundlePackageType
    vst3['CFBundleSignature'] = config['PLUG_UNIQUE_ID']
    vst3['CSResourcesFileMapped'] = CSResourcesFileMapped

    with open(plistpath, 'wb') as f2:
      plistlib.dump(vst3, f2)

# VST2

  plistpath = projectpath + "/resources/" + config['BUNDLE_NAME'] + "-VST2-Info.plist"
  with open(plistpath, 'rb') as f:
    vst2 = plistlib.load(f)  
    vst2['CFBundleExecutable'] = config['BUNDLE_NAME']
    vst2['CFBundleGetInfoString'] = CFBundleGetInfoString
    vst2['CFBundleIdentifier'] = config['BUNDLE_DOMAIN'] + "." + config['BUNDLE_MFR'] + ".vst." + config['BUNDLE_NAME'] + ""
    vst2['CFBundleName'] = config['BUNDLE_NAME']
    vst2['CFBundleVersion'] = CFBundleVersion
    vst2['CFBundleShortVersionString'] = CFBundleVersion
    vst2['LSMinimumSystemVersion'] = LSMinimumSystemVersion
    vst2['CFBundlePackageType'] = CFBundlePackageType
    vst2['CFBundleSignature'] = config['PLUG_UNIQUE_ID']
    vst2['CSResourcesFileMapped'] = CSResourcesFileMapped

    with open(plistpath, 'wb') as f2:
      plistlib.dump(vst2, f2)

# AUDIOUNIT v2

  plistpath = projectpath + "/resources/" + config['BUNDLE_NAME'] + "-AU-Info.plist"
  with open(plistpath, 'rb') as f:
    auv2 = plistlib.load(f)  
    auv2['CFBundleExecutable'] = config['BUNDLE_NAME']
    auv2['CFBundleGetInfoString'] = CFBundleGetInfoString
    auv2['CFBundleIdentifier'] = config['BUNDLE_DOMAIN'] + "." + config['BUNDLE_MFR'] + ".audiounit." + config['BUNDLE_NAME'] + ""
    auv2['CFBundleName'] = config['BUNDLE_NAME']
    auv2['CFBundleVersion'] = CFBundleVersion
    auv2['CFBundleShortVersionString'] = CFBundleVersion
    auv2['LSMinimumSystemVersion'] = LSMinimumSystemVersion
    auv2['CFBundlePackageType'] = CFBundlePackageType
    auv2['CFBundleSignature'] = config['PLUG_UNIQUE_ID']
    auv2['CSResourcesFileMapped'] = CSResourcesFileMapped

    if config['PLUG_TYPE'] == 0:
      if config['PLUG_DOES_MIDI_IN']:
        COMPONENT_TYPE = kAudioUnitType_MusicEffect
      else:
        COMPONENT_TYPE = kAudioUnitType_Effect
    elif config['PLUG_TYPE'] == 1:
      COMPONENT_TYPE = kAudioUnitType_MusicDevice
    elif config['PLUG_TYPE'] == 2:
      COMPONENT_TYPE = kAudioUnitType_MIDIProcessor

    auv2['AudioUnit Version'] = config['PLUG_VERSION_HEX']
    auv2['AudioComponents'] = [{}]
    auv2['AudioComponents'][0]['description'] = config['PLUG_NAME']
    auv2['AudioComponents'][0]['factoryFunction'] = config['AUV2_FACTORY']
    auv2['AudioComponents'][0]['manufacturer'] = config['PLUG_MFR_ID']
    auv2['AudioComponents'][0]['name'] = config['PLUG_MFR'] + ": " + config['PLUG_NAME']
    auv2['AudioComponents'][0]['subtype'] = config['PLUG_UNIQUE_ID']
    auv2['AudioComponents'][0]['type'] = COMPONENT_TYPE
    auv2['AudioComponents'][0]['version'] = config['PLUG_VERSION_INT']
    auv2['AudioComponents'][0]['sandboxSafe'] = True

    with open(plistpath, 'wb') as f2:
      plistlib.dump(auv2, f2)
      
# AUDIOUNIT v3

  if config['PLUG_HAS_UI']:
    NSEXTENSIONPOINTIDENTIFIER  = "com.apple.AudioUnit-UI"
  else:
    NSEXTENSIONPOINTIDENTIFIER  = "com.apple.AudioUnit"

  plistpath = projectpath + "/resources/" + config['BUNDLE_NAME'] + "-macOS-AUv3-Info.plist"

  with open(plistpath, 'rb') as f:
    auv3 = plistlib.load(f)  
    auv3['CFBundleExecutable'] = config['BUNDLE_NAME']
    auv3['CFBundleGetInfoString'] = CFBundleGetInfoString
    auv3['CFBundleIdentifier'] = config['BUNDLE_DOMAIN'] + "." + config['BUNDLE_MFR'] + ".app." + config['BUNDLE_NAME'] + ".AUv3"
    auv3['CFBundleName'] = config['BUNDLE_NAME']
    auv3['CFBundleVersion'] = CFBundleVersion
    auv3['CFBundleShortVersionString'] = CFBundleVersion
    auv3['LSMinimumSystemVersion'] = "10.12.0"
    auv3['CFBundlePackageType'] = "XPC!"
    auv3['NSExtension'] = dict(
    NSExtensionAttributes = dict(
                                AudioComponentBundle = "com.AcmeInc.app." + config['BUNDLE_NAME'] + ".AUv3Framework",
                                AudioComponents = [{}]),
  #                               NSExtensionServiceRoleType = "NSExtensionServiceRoleTypeEditor",
    NSExtensionPointIdentifier = NSEXTENSIONPOINTIDENTIFIER,
    NSExtensionPrincipalClass = "IPlugAUViewController_vTemplateProject"
                              )
    auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'] = [{}]
    auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['description'] = config['PLUG_NAME']
    auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['manufacturer'] = config['PLUG_MFR_ID']
    auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['name'] = config['PLUG_MFR'] + ": " + config['PLUG_NAME']
    auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['subtype'] = config['PLUG_UNIQUE_ID']
    auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['type'] = COMPONENT_TYPE
    auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['version'] = config['PLUG_VERSION_INT']
    auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['sandboxSafe'] = True
    auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['tags'] = [{}]

    if config['PLUG_TYPE'] == 1:
      auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['tags'][0] = "Synth"
    else:
      auv3['NSExtension']['NSExtensionAttributes']['AudioComponents'][0]['tags'][0] = "Effects"

    with open(plistpath, 'wb') as f2:
      plistlib.dump(auv3, f2)

# AAX

  plistpath = projectpath + "/resources/" + config['BUNDLE_NAME'] + "-AAX-Info.plist"
  with open(plistpath, 'rb') as f:
    aax = plistlib.load(f)  
    aax['CFBundleExecutable'] = config['BUNDLE_NAME']
    aax['CFBundleGetInfoString'] = CFBundleGetInfoString
    aax['CFBundleIdentifier'] = config['BUNDLE_DOMAIN'] + "." + config['BUNDLE_MFR'] + ".aax." + config['BUNDLE_NAME'] + ""
    aax['CFBundleName'] = config['BUNDLE_NAME']
    aax['CFBundleVersion'] = CFBundleVersion
    aax['CFBundleShortVersionString'] = CFBundleVersion
    aax['LSMinimumSystemVersion'] = LSMinimumSystemVersion
    aax['CSResourcesFileMapped'] = CSResourcesFileMapped

    with open(plistpath, 'wb') as f2:
      plistlib.dump(aax, f2)

# APP

  plistpath = projectpath + "/resources/" + config['BUNDLE_NAME'] + "-macOS-Info.plist"

  with open(plistpath, 'rb') as f:
    macOSapp = plistlib.load(f)
    macOSapp['CFBundleExecutable'] = config['BUNDLE_NAME']
    macOSapp['CFBundleGetInfoString'] = CFBundleGetInfoString
    macOSapp['CFBundleIdentifier'] = config['BUNDLE_DOMAIN'] + "." + config['BUNDLE_MFR'] + ".app." + config['BUNDLE_NAME'] + ""
    macOSapp['CFBundleName'] = config['BUNDLE_NAME']
    macOSapp['CFBundleVersion'] = CFBundleVersion
    macOSapp['CFBundleShortVersionString'] = CFBundleVersion
    macOSapp['LSMinimumSystemVersion'] = LSMinimumSystemVersion
    macOSapp['CFBundlePackageType'] = CFBundlePackageType
    macOSapp['CFBundleSignature'] = config['PLUG_UNIQUE_ID']
    macOSapp['CSResourcesFileMapped'] = CSResourcesFileMapped
    macOSapp['NSPrincipalClass'] = "SWELLApplication"
    macOSapp['NSMainNibFile'] = config['BUNDLE_NAME'] + "-macOS-MainMenu"
    macOSapp['LSApplicationCategoryType'] = "public.app-category.music"
    macOSapp['NSMicrophoneUsageDescription'] = 	"This app needs mic access to process audio."

    with open(plistpath, 'wb') as f2:
      plistlib.dump(macOSapp, f2)

if __name__ == '__main__':
  main()
