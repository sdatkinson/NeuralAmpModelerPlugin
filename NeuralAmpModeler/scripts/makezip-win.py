import zipfile, os, fileinput, string, sys, shutil

scriptpath = os.path.dirname(os.path.realpath(__file__))
projectpath = os.path.abspath(os.path.join(scriptpath, os.pardir))

IPLUG2_ROOT = "..\..\iPlug2"

sys.path.insert(0, os.path.join(scriptpath, IPLUG2_ROOT + "\Scripts"))

from get_archive_name import get_archive_name


def main():
    if len(sys.argv) != 3:
        print("Usage: make_zip.py demo[0/1] zip[0/1]")
        sys.exit(1)
    else:
        demo = int(sys.argv[1])
        zip = int(sys.argv[2])

    dir = projectpath + "\\build-win\\out"

    if os.path.exists(dir):
        shutil.rmtree(dir)

    os.makedirs(dir)

    files = []

    if not zip:
        installer = "\\build-win\\installer\\NeuralAmpModeler Installer.exe"

        if demo:
            installer = "\\build-win\\installer\\NeuralAmpModeler Demo Installer.exe"

        files = [
            projectpath + installer,
            projectpath + "\\installer\\changelog.txt",
            projectpath + "\\installer\\known-issues.txt",
            projectpath + "\\manual\\NeuralAmpModeler manual.pdf",
        ]
    else:
        files = [
            projectpath + "\\build-win\\NeuralAmpModeler_x64.exe",
            projectpath + "\\build-win\\NeuralAmpModeler_ARM64EC.exe",
        ]

    zipname = get_archive_name(projectpath, "win", "demo" if demo == 1 else "full")

    zf = zipfile.ZipFile(
        projectpath + "\\build-win\\out\\" + zipname + ".zip", mode="w"
    )

    for f in files:
        print("adding " + f)
        zf.write(f, os.path.basename(f), zipfile.ZIP_DEFLATED)

    if zip:
        # add the multi-arch VST3 bundle (x86_64-win + arm64ec-win), preserving structure
        bundlepath = projectpath + "\\build-win\\NeuralAmpModeler.vst3"
        excluded_exts = (".pdb", ".exp", ".lib", ".ilk", ".ico", ".ini")
        for root, dirs, bundlefiles in os.walk(bundlepath):
            dirs[:] = [d for d in dirs if d != "x86-win"]  # stale 32-bit folder from old builds
            for bf in bundlefiles:
                fullpath = os.path.join(root, bf)
                if os.path.splitext(bf)[1].lower() in excluded_exts:
                    continue
                arcname = "NeuralAmpModeler.vst3\\" + os.path.relpath(fullpath, bundlepath)
                print("adding " + fullpath)
                zf.write(fullpath, arcname, zipfile.ZIP_DEFLATED)

    zf.close()
    print("wrote " + zipname)

    zf = zipfile.ZipFile(
        projectpath + "\\build-win\\out\\" + zipname + "-pdbs.zip", mode="w"
    )

    files = [
        projectpath + "\\build-win\\pdbs\\NeuralAmpModeler-vst3_x64.pdb",
        projectpath + "\\build-win\\pdbs\\NeuralAmpModeler-vst3_ARM64EC.pdb",
        projectpath + "\\build-win\\pdbs\\NeuralAmpModeler-app_x64.pdb",
        projectpath + "\\build-win\\pdbs\\NeuralAmpModeler-app_ARM64EC.pdb",
    ]

    for f in files:
        print("adding " + f)
        zf.write(f, os.path.basename(f), zipfile.ZIP_DEFLATED)

    zf.close()
    print("wrote " + zipname)


if __name__ == "__main__":
    main()
