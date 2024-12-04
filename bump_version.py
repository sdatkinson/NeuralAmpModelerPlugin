#!/usr/bin/env python3

# https://semver.org/
# pip3 install semver

import os, sys, shutil, subprocess, glob, fileinput, string
import semver

IPLUG2_ROOT = "iPlug2"
PROJECT_ROOT = "NeuralAmpModeler"
PROJECT_SCRIPTS = os.path.join(PROJECT_ROOT, "scripts")


def replacestrs(filename, s, r):
    files = glob.glob(filename)
    print("replacing " + s + " with " + r + " in " + filename)

    for line in fileinput.input(files, inplace=1):
        line = line.replace(s, r)
        sys.stdout.write(line)


sys.path.insert(0, os.path.join(os.getcwd(), IPLUG2_ROOT, "Scripts"))

from parse_config import parse_config


def main():
    config = parse_config(PROJECT_ROOT)
    versionStr = config["FULL_VER_STR"]
    currentVersionInfo = semver.VersionInfo.parse(versionStr)
    print("current version in config.h: v" + versionStr)

    if len(sys.argv) == 2:
        if sys.argv[1] == "major":
            newVersionInfo = currentVersionInfo.bump_major()
        elif sys.argv[1] == "minor":
            newVersionInfo = currentVersionInfo.bump_minor()
        elif sys.argv[1] == "patch":
            newVersionInfo = currentVersionInfo.bump_patch()
        elif sys.argv[1] == "none":
            newVersionInfo = currentVersionInfo
        else:
            raise ValueError(f"Unrecognized version bump for '{sys.argv[1]}'")
    else:
        print("Please supply an argument major, minor or patch")
        exit()

    newVersionInt = (
        (newVersionInfo.major << 16 & 0xFFFF0000)
        + (newVersionInfo.minor << 8 & 0x0000FF00)
        + (newVersionInfo.patch & 0x000000FF)
    )

    replacestrs(
        os.path.join(PROJECT_ROOT, "config.h"),
        '#define PLUG_VERSION_STR "' + versionStr + '"',
        '#define PLUG_VERSION_STR "' + str(newVersionInfo) + '"',
    )
    replacestrs(
        os.path.join(PROJECT_ROOT, "config.h"),
        "#define PLUG_VERSION_HEX " + config["PLUG_VERSION_HEX"],
        "#define PLUG_VERSION_HEX " + "0x{:08x}".format(newVersionInt),
    )

    os.system("cd " + PROJECT_SCRIPTS + "; python3 update_version-mac.py")
    os.system("cd " + PROJECT_SCRIPTS + "; python3 update_version-ios.py")
    os.system("cd " + PROJECT_SCRIPTS + "; python3 update_installer-win.py 0")

    print("\nCurrent changelog: \n--------------------")
    os.system("cat " + os.path.join(PROJECT_ROOT, "installer", "changelog.txt"))
    print("\n\n--------------------")

    edit = input("\nEdit changelog? Y/N: ")

    if edit == "y" or edit == "Y":
        os.system("vim " + os.path.join(PROJECT_ROOT, "installer", "changelog.txt"))

        print("\nNew changelog: \n--------------------")
        os.system("cat " + os.path.join(PROJECT_ROOT, "installer", "changelog.txt"))
        print("\n\n--------------------")

    tagname = f"v{newVersionInfo}"
    remove_tag = input(f"\nAttempt to remove existing tag for {tagname}? Y/N: ")

    if remove_tag == "y" or remove_tag == "Y":
        os.system(f"git tag -d {tagname}")
        os.system(f"git push --delete origin {tagname}")

    commit = input("\nCommit? Y/N")
    if commit == "y" or commit == "Y":
        os.system("git commit -a --allow-empty")
    edit = input(
        "\nTag version and git push to origin? Y/N: "
    )
    if edit == "y" or edit == "Y":
        os.system("git tag " + tagname)
        os.system("git push && git push --tags")


if __name__ == "__main__":
    main()
